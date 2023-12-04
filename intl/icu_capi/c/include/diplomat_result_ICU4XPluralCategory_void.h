#ifndef diplomat_result_ICU4XPluralCategory_void_H
#define diplomat_result_ICU4XPluralCategory_void_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ICU4XPluralCategory.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif
typedef struct diplomat_result_ICU4XPluralCategory_void {
    union {
        ICU4XPluralCategory ok;
    };
    bool is_ok;
} diplomat_result_ICU4XPluralCategory_void;
#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
