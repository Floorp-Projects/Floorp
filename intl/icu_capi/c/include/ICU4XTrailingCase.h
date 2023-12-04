#ifndef ICU4XTrailingCase_H
#define ICU4XTrailingCase_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XTrailingCase {
  ICU4XTrailingCase_Lower = 0,
  ICU4XTrailingCase_Unchanged = 1,
} ICU4XTrailingCase;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XTrailingCase_destroy(ICU4XTrailingCase* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
