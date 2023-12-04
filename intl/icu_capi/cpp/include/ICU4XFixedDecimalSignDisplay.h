#ifndef ICU4XFixedDecimalSignDisplay_H
#define ICU4XFixedDecimalSignDisplay_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XFixedDecimalSignDisplay {
  ICU4XFixedDecimalSignDisplay_Auto = 0,
  ICU4XFixedDecimalSignDisplay_Never = 1,
  ICU4XFixedDecimalSignDisplay_Always = 2,
  ICU4XFixedDecimalSignDisplay_ExceptZero = 3,
  ICU4XFixedDecimalSignDisplay_Negative = 4,
} ICU4XFixedDecimalSignDisplay;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XFixedDecimalSignDisplay_destroy(ICU4XFixedDecimalSignDisplay* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
