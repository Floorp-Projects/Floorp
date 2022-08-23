#include "config_components.h"

static const FFBitStreamFilter * const bitstream_filters[] = {
    &ff_null_bsf,
#if CONFIG_VP9_SUPERFRAME_SPLIT_BSF
    &ff_vp9_superframe_split_bsf,
#endif
    NULL };
