#ifndef ICU4XLocaleFallbackConfig_H
#define ICU4XLocaleFallbackConfig_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ICU4XLocaleFallbackPriority.h"
#include "ICU4XLocaleFallbackSupplement.h"
#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XLocaleFallbackConfig {
    ICU4XLocaleFallbackPriority priority;
    DiplomatStringView extension_key;
    ICU4XLocaleFallbackSupplement fallback_supplement;
} ICU4XLocaleFallbackConfig;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XLocaleFallbackPriority.h"
#include "ICU4XLocaleFallbackSupplement.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XLocaleFallbackConfig_destroy(ICU4XLocaleFallbackConfig* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
