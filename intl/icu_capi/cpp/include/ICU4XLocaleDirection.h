#ifndef ICU4XLocaleDirection_H
#define ICU4XLocaleDirection_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XLocaleDirection {
  ICU4XLocaleDirection_LeftToRight = 0,
  ICU4XLocaleDirection_RightToLeft = 1,
  ICU4XLocaleDirection_Unknown = 2,
} ICU4XLocaleDirection;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XLocaleDirection_destroy(ICU4XLocaleDirection* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
