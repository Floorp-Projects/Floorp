#ifndef ICU4XRoundingIncrement_H
#define ICU4XRoundingIncrement_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XRoundingIncrement {
  ICU4XRoundingIncrement_MultiplesOf1 = 0,
  ICU4XRoundingIncrement_MultiplesOf2 = 1,
  ICU4XRoundingIncrement_MultiplesOf5 = 2,
  ICU4XRoundingIncrement_MultiplesOf25 = 3,
} ICU4XRoundingIncrement;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XRoundingIncrement_destroy(ICU4XRoundingIncrement* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
