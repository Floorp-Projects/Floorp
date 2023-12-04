#ifndef ICU4XCollatorCaseFirst_H
#define ICU4XCollatorCaseFirst_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XCollatorCaseFirst {
  ICU4XCollatorCaseFirst_Auto = 0,
  ICU4XCollatorCaseFirst_Off = 1,
  ICU4XCollatorCaseFirst_LowerFirst = 2,
  ICU4XCollatorCaseFirst_UpperFirst = 3,
} ICU4XCollatorCaseFirst;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XCollatorCaseFirst_destroy(ICU4XCollatorCaseFirst* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
