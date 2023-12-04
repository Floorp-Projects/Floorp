#ifndef ICU4XLocaleFallbackSupplement_H
#define ICU4XLocaleFallbackSupplement_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XLocaleFallbackSupplement {
  ICU4XLocaleFallbackSupplement_None = 0,
  ICU4XLocaleFallbackSupplement_Collation = 1,
} ICU4XLocaleFallbackSupplement;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XLocaleFallbackSupplement_destroy(ICU4XLocaleFallbackSupplement* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
