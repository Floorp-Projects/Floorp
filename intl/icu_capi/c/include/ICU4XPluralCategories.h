#ifndef ICU4XPluralCategories_H
#define ICU4XPluralCategories_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XPluralCategories {
    bool zero;
    bool one;
    bool two;
    bool few;
    bool many;
    bool other;
} ICU4XPluralCategories;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XPluralCategories_destroy(ICU4XPluralCategories* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
