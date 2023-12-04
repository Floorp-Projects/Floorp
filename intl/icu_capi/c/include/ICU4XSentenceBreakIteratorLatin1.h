#ifndef ICU4XSentenceBreakIteratorLatin1_H
#define ICU4XSentenceBreakIteratorLatin1_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XSentenceBreakIteratorLatin1 ICU4XSentenceBreakIteratorLatin1;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

int32_t ICU4XSentenceBreakIteratorLatin1_next(ICU4XSentenceBreakIteratorLatin1* self);
void ICU4XSentenceBreakIteratorLatin1_destroy(ICU4XSentenceBreakIteratorLatin1* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
