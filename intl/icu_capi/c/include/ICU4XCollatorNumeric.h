#ifndef ICU4XCollatorNumeric_H
#define ICU4XCollatorNumeric_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XCollatorNumeric {
  ICU4XCollatorNumeric_Auto = 0,
  ICU4XCollatorNumeric_Off = 1,
  ICU4XCollatorNumeric_On = 2,
} ICU4XCollatorNumeric;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XCollatorNumeric_destroy(ICU4XCollatorNumeric* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
