#ifndef ICU4XCollatorCaseLevel_H
#define ICU4XCollatorCaseLevel_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XCollatorCaseLevel {
  ICU4XCollatorCaseLevel_Auto = 0,
  ICU4XCollatorCaseLevel_Off = 1,
  ICU4XCollatorCaseLevel_On = 2,
} ICU4XCollatorCaseLevel;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XCollatorCaseLevel_destroy(ICU4XCollatorCaseLevel* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
