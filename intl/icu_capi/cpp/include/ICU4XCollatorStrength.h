#ifndef ICU4XCollatorStrength_H
#define ICU4XCollatorStrength_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XCollatorStrength {
  ICU4XCollatorStrength_Auto = 0,
  ICU4XCollatorStrength_Primary = 1,
  ICU4XCollatorStrength_Secondary = 2,
  ICU4XCollatorStrength_Tertiary = 3,
  ICU4XCollatorStrength_Quaternary = 4,
  ICU4XCollatorStrength_Identical = 5,
} ICU4XCollatorStrength;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XCollatorStrength_destroy(ICU4XCollatorStrength* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
