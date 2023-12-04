#ifndef ICU4XLineBreakWordOption_H
#define ICU4XLineBreakWordOption_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XLineBreakWordOption {
  ICU4XLineBreakWordOption_Normal = 0,
  ICU4XLineBreakWordOption_BreakAll = 1,
  ICU4XLineBreakWordOption_KeepAll = 2,
} ICU4XLineBreakWordOption;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XLineBreakWordOption_destroy(ICU4XLineBreakWordOption* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
