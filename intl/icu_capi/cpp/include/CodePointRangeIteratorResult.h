#ifndef CodePointRangeIteratorResult_H
#define CodePointRangeIteratorResult_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct CodePointRangeIteratorResult {
    uint32_t start;
    uint32_t end;
    bool done;
} CodePointRangeIteratorResult;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void CodePointRangeIteratorResult_destroy(CodePointRangeIteratorResult* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
