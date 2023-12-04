#ifndef CodePointRangeIterator_H
#define CodePointRangeIterator_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct CodePointRangeIterator CodePointRangeIterator;
#ifdef __cplusplus
} // namespace capi
#endif
#include "CodePointRangeIteratorResult.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

CodePointRangeIteratorResult CodePointRangeIterator_next(CodePointRangeIterator* self);
void CodePointRangeIterator_destroy(CodePointRangeIterator* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
