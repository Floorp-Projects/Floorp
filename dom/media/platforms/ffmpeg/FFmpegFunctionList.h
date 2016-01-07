/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* libavcodec */
AV_FUNC(avcodec_alloc_context3, AV_FUNC_AVCODEC_ALL)
AV_FUNC(avcodec_close, AV_FUNC_AVCODEC_ALL)
AV_FUNC(avcodec_decode_audio4, AV_FUNC_AVCODEC_ALL)
AV_FUNC(avcodec_decode_video2, AV_FUNC_AVCODEC_ALL)
AV_FUNC(avcodec_find_decoder, AV_FUNC_AVCODEC_ALL)
AV_FUNC(avcodec_flush_buffers, AV_FUNC_AVCODEC_ALL)
AV_FUNC(avcodec_open2, AV_FUNC_AVCODEC_ALL)
AV_FUNC(avcodec_register_all, AV_FUNC_AVCODEC_ALL)
AV_FUNC(av_init_packet, AV_FUNC_AVCODEC_ALL)
AV_FUNC(av_parser_init, AV_FUNC_AVCODEC_ALL)
AV_FUNC(av_parser_close, AV_FUNC_AVCODEC_ALL)
AV_FUNC(av_parser_parse2, AV_FUNC_AVCODEC_ALL)
#if LIBAVCODEC_VERSION_MAJOR <= 54 || defined(LIBAVCODEC_ALLVERSION)
AV_FUNC(avcodec_get_frame_defaults, (AV_FUNC_53 | AV_FUNC_54))
#endif

/* libavutil */
AV_FUNC(av_log_set_level, AV_FUNC_AVUTIL_ALL)
AV_FUNC(av_malloc, AV_FUNC_AVUTIL_ALL)
AV_FUNC(av_freep, AV_FUNC_AVUTIL_ALL)

#if defined(LIBAVCODEC_VERSION_MAJOR) || defined(LIBAVCODEC_ALLVERSION)
#if LIBAVCODEC_VERSION_MAJOR == 54 || defined(LIBAVCODEC_ALLVERSION)
/* libavutil v54 only */
AV_FUNC(avcodec_alloc_frame, AV_FUNC_AVUTIL_54)
AV_FUNC(avcodec_free_frame, AV_FUNC_AVUTIL_54)
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 55 || defined(LIBAVCODEC_ALLVERSION)
/* libavutil v55 and later only */
AV_FUNC(av_frame_alloc, (AV_FUNC_AVUTIL_55 | AV_FUNC_AVUTIL_56 | AV_FUNC_AVUTIL_57))
AV_FUNC(av_frame_free, (AV_FUNC_AVUTIL_55 | AV_FUNC_AVUTIL_56 | AV_FUNC_AVUTIL_57))
AV_FUNC(av_frame_unref, (AV_FUNC_AVUTIL_55 | AV_FUNC_AVUTIL_56 | AV_FUNC_AVUTIL_57))
#endif
#endif
