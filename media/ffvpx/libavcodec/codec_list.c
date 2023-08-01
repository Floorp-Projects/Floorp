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
#if CONFIG_LIBVORBIS_DECODER
    &ff_libvorbis_decoder,
#endif
#if CONFIG_PCM_ALAW_DECODER
    &ff_pcm_alaw_decoder,
#endif
#if CONFIG_PCM_F32LE_DECODER
    &ff_pcm_f32le_decoder,
#endif
#if CONFIG_PCM_MULAW_DECODER
    &ff_pcm_mulaw_decoder,
#endif
#if CONFIG_PCM_S16LE_DECODER
    &ff_pcm_s16le_decoder,
#endif
#if CONFIG_PCM_S24LE_DECODER
    &ff_pcm_s24le_decoder,
#endif
#if CONFIG_PCM_S32LE_DECODER
    &ff_pcm_s32le_decoder,
#endif
#if CONFIG_PCM_U8_DECODER
    &ff_pcm_u8_decoder,
#endif
#if CONFIG_LIBOPUS_DECODER
    &ff_libopus_decoder,
#endif
    NULL };
