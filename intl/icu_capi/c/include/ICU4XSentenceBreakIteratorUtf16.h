#ifndef ICU4XSentenceBreakIteratorUtf16_H
#define ICU4XSentenceBreakIteratorUtf16_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XSentenceBreakIteratorUtf16 ICU4XSentenceBreakIteratorUtf16;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

int32_t ICU4XSentenceBreakIteratorUtf16_next(ICU4XSentenceBreakIteratorUtf16* self);
void ICU4XSentenceBreakIteratorUtf16_destroy(ICU4XSentenceBreakIteratorUtf16* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
