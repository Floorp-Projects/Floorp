#ifndef ICU4XOrdering_H
#define ICU4XOrdering_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XOrdering {
  ICU4XOrdering_Less = -1,
  ICU4XOrdering_Equal = 0,
  ICU4XOrdering_Greater = 1,
} ICU4XOrdering;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XOrdering_destroy(ICU4XOrdering* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
