#ifndef ICU4XFixedDecimalSign_H
#define ICU4XFixedDecimalSign_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XFixedDecimalSign {
  ICU4XFixedDecimalSign_None = 0,
  ICU4XFixedDecimalSign_Negative = 1,
  ICU4XFixedDecimalSign_Positive = 2,
} ICU4XFixedDecimalSign;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XFixedDecimalSign_destroy(ICU4XFixedDecimalSign* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
