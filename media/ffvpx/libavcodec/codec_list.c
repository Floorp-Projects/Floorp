static const AVCodec * const codec_list[] = {
#if CONFIG_VP8_DECODER
    &ff_vp8_decoder,
#endif
#if CONFIG_VP9_DECODER
    &ff_vp9_decoder,
#endif
#if CONFIG_FLAC_DECODER
    &ff_flac_decoder,
#endif
    NULL };
