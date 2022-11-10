#include "config_components.h"

static const FFBitStreamFilter * const bitstream_filters[] = {
#if CONFIG_VP9_SUPERFRAME_SPLIT_BSF
    &ff_vp9_superframe_split_bsf,
#endif
#if CONFIG_AV1_VAAPI_HWACCEL
    &ff_av1_frame_split_bsf,
#endif
    &ff_null_bsf,
    NULL };
