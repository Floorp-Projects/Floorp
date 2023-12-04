#ifndef ICU4XTransformResult_H
#define ICU4XTransformResult_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XTransformResult {
  ICU4XTransformResult_Modified = 0,
  ICU4XTransformResult_Unmodified = 1,
} ICU4XTransformResult;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XTransformResult_destroy(ICU4XTransformResult* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
