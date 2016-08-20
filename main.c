#include <assert.h>
#include <stdio.h>
#include <pulse/pulseaudio.h>

void ctx_state_cb(pa_context *ctx, void *loop);
void stream_state_cb(pa_stream *stream, void *loop);
void stream_write_cb(pa_stream *stream, size_t requested_bytes, void *userdata);
void stream_success_cb(pa_stream *stream, int success, void *userdata);

int main(int argc, char *argv[]) {
    pa_threaded_mainloop *loop;
    pa_mainloop_api *api;
    pa_context *ctx;
    pa_stream *stream;

    loop = pa_threaded_mainloop_new();
    assert(loop);
    api = pa_threaded_mainloop_get_api(loop);
    ctx = pa_context_new(api, "playback");
    assert(ctx);

    pa_context_set_state_callback(ctx, &ctx_state_cb, loop);

    pa_threaded_mainloop_lock(loop);
    assert(pa_threaded_mainloop_start(loop) == 0);
    assert(pa_context_connect(ctx, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) == 0);

    for(;;) {
        pa_context_state_t ctx_state = pa_context_get_state(ctx);
        assert(PA_CONTEXT_IS_GOOD(ctx_state));
        if (ctx_state == PA_CONTEXT_READY) {
            break;
        }
        pa_threaded_mainloop_wait(loop);
    }

    pa_sample_spec sample_specifications;
    sample_specifications.format = PA_SAMPLE_S16LE;
    sample_specifications.rate = 44100;
    sample_specifications.channels = 2;

    pa_channel_map map;
    pa_channel_map_init_stereo(&map);

    stream = pa_stream_new(ctx, "playback", &sample_specifications, &map);
    pa_stream_set_state_callback(stream, stream_state_cb, loop);
    pa_stream_set_write_callback(stream, stream_write_cb, loop);

    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = (uint32_t) -1;
    buffer_attr.tlength = (uint32_t) -1;
    buffer_attr.prebuf = (uint32_t) -1;
    buffer_attr.minreq = (uint32_t) -1;

    pa_stream_flags_t stream_flags;
    stream_flags = PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_NOT_MONOTONIC |
        PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY;

    assert(pa_stream_connect_playback(stream, NULL, &buffer_attr, stream_flags, NULL, NULL) == 0);

    for(;;) {
        pa_stream_state_t stream_state = pa_stream_get_state(stream);
        assert(PA_STREAM_IS_GOOD(stream_state));
        if (stream_state == PA_STREAM_READY) {
            break;
        }
        pa_threaded_mainloop_wait(loop);
    }

    pa_threaded_mainloop_unlock(loop);

    pa_stream_cork(stream, 0, stream_success_cb, loop);

    getc(stdin);
}

void ctx_state_cb(pa_context *ctx, void *loop) {
    pa_threaded_mainloop_signal(loop, 0);
}

void stream_state_cb(pa_stream *stream, void *loop) {
    pa_threaded_mainloop_signal(loop, 0);
}

void stream_write_cb(pa_stream *stream, size_t requested_bytes, void *userdata) {
    size_t bytes_remaining = requested_bytes;
    while (bytes_remaining > 0) {
        int16_t *buffer = NULL;
        size_t bytes_to_fill = 44100 * 2;
        size_t i;

        if(bytes_to_fill > bytes_remaining) {
            bytes_to_fill = bytes_remaining;
        }

        pa_stream_begin_write(stream, (void**) &buffer, &bytes_to_fill);

        int16_t lastL = 1, lastR = 1;
        for (i = 0; i < bytes_to_fill; i += 2) {

            buffer[i] = (int16_t)(lastL + (0.02 * rand()) / 1.02);
            lastL = buffer[i];
            buffer[i+1] = (int16_t)(lastR + (0.02 * rand()) / 1.02);
            lastR = buffer[i+1];
        }

        pa_stream_write(stream, buffer, bytes_to_fill, NULL, 0LL, PA_SEEK_RELATIVE);

        bytes_remaining -= bytes_to_fill;
    }
}

void stream_success_cb(pa_stream *stream, int success, void *userdata) {
    return ;
}

