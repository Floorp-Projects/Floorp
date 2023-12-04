#ifndef diplomat_result_ICU4XAnyCalendarKind_void_H
#define diplomat_result_ICU4XAnyCalendarKind_void_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ICU4XAnyCalendarKind.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif
typedef struct diplomat_result_ICU4XAnyCalendarKind_void {
    union {
        ICU4XAnyCalendarKind ok;
    };
    bool is_ok;
} diplomat_result_ICU4XAnyCalendarKind_void;
#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
