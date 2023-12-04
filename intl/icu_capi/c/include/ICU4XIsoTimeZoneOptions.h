#ifndef ICU4XIsoTimeZoneOptions_H
#define ICU4XIsoTimeZoneOptions_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ICU4XIsoTimeZoneFormat.h"
#include "ICU4XIsoTimeZoneMinuteDisplay.h"
#include "ICU4XIsoTimeZoneSecondDisplay.h"
#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XIsoTimeZoneOptions {
    ICU4XIsoTimeZoneFormat format;
    ICU4XIsoTimeZoneMinuteDisplay minutes;
    ICU4XIsoTimeZoneSecondDisplay seconds;
} ICU4XIsoTimeZoneOptions;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XIsoTimeZoneFormat.h"
#include "ICU4XIsoTimeZoneMinuteDisplay.h"
#include "ICU4XIsoTimeZoneSecondDisplay.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XIsoTimeZoneOptions_destroy(ICU4XIsoTimeZoneOptions* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
