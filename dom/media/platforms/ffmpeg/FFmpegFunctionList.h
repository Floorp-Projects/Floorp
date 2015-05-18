/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* libavformat */
AV_FUNC(av_register_all, 0)

/* libavcodec */
AV_FUNC(avcodec_align_dimensions2, 0)
AV_FUNC(avcodec_get_frame_defaults, 0)
AV_FUNC(avcodec_close, 0)
AV_FUNC(avcodec_decode_audio4, 0)
AV_FUNC(avcodec_decode_video2, 0)
AV_FUNC(avcodec_default_get_buffer, 0)
AV_FUNC(avcodec_default_release_buffer, 0)
AV_FUNC(avcodec_find_decoder, 0)
AV_FUNC(avcodec_flush_buffers, 0)
AV_FUNC(avcodec_alloc_context3, 0)
AV_FUNC(avcodec_get_edge_width, 0)
AV_FUNC(avcodec_open2, 0)
AV_FUNC(av_init_packet, 0)
AV_FUNC(av_dict_get, 0)

/* libavutil */
AV_FUNC(av_image_fill_linesizes, 0)
AV_FUNC(av_image_fill_pointers, 0)
AV_FUNC(av_log_set_level, 0)
AV_FUNC(av_malloc, 0)
AV_FUNC(av_freep, 0)

#if defined(LIBAVCODEC_VERSION_MAJOR) || defined(LIBAVCODEC_ALLVERSION)
#if LIBAVCODEC_VERSION_MAJOR == 54 || defined(LIBAVCODEC_ALLVERSION)
/* libavutil v54 only */
AV_FUNC(avcodec_alloc_frame, 54)
AV_FUNC(avcodec_free_frame, 54)
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 55 || defined(LIBAVCODEC_ALLVERSION)
/* libavutil v55 only */
AV_FUNC(av_frame_alloc, 55)
AV_FUNC(av_frame_free, 55)
AV_FUNC(av_frame_unref, 55)
#endif
#endif
