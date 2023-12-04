#ifndef ICU4XLanguageDisplay_H
#define ICU4XLanguageDisplay_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef enum ICU4XLanguageDisplay {
  ICU4XLanguageDisplay_Dialect = 0,
  ICU4XLanguageDisplay_Standard = 1,
} ICU4XLanguageDisplay;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XLanguageDisplay_destroy(ICU4XLanguageDisplay* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
