// This file contains overrides for config.h, that can be platform-specific.

#undef CONFIG_FFT
#undef CONFIG_RDFT
#define CONFIG_FFT 1
#define CONFIG_RDFT 1

// override '#define EXTERN_ASM _' in config_generic.h to allow building with
// gcc on arm
#if defined(__GNUC__) && defined(__arm__)
#undef EXTERN_ASM
#define EXTERN_ASM
#endif

#undef CONFIG_VAAPI
#undef CONFIG_VAAPI_1
#undef CONFIG_VP8_VAAPI_HWACCEL
#undef CONFIG_VP9_VAAPI_HWACCEL
#undef CONFIG_AV1_VAAPI_HWACCEL

#if defined(MOZ_WIDGET_GTK) && !defined(MOZ_FFVPX_AUDIOONLY)
#define CONFIG_VAAPI 1
#define CONFIG_VAAPI_1 1
#define CONFIG_VP8_VAAPI_HWACCEL 1
#define CONFIG_VP9_VAAPI_HWACCEL 1
#define CONFIG_AV1_VAAPI_HWACCEL 1
#else
#define CONFIG_VAAPI 0
#define CONFIG_VAAPI_1 0
#define CONFIG_VP8_VAAPI_HWACCEL 0
#define CONFIG_VP9_VAAPI_HWACCEL 0
#define CONFIG_AV1_VAAPI_HWACCEL 0
#endif
