static const FFCodec * const codec_list[] = {
#if CONFIG_VP8_DECODER
    &ff_vp8_decoder,
#endif
#if CONFIG_VP9_DECODER
    &ff_vp9_decoder,
#endif
#if CONFIG_FLAC_DECODER
    &ff_flac_decoder,
#endif
#if CONFIG_MP3_DECODER
    &ff_mp3_decoder,
#endif
#if CONFIG_LIBDAV1D
    &ff_libdav1d_decoder,
#endif
#if CONFIG_AV1_DECODER
    &ff_av1_decoder,
#endif
    NULL };
