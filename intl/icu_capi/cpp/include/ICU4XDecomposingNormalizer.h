#ifndef ICU4XDecomposingNormalizer_H
#define ICU4XDecomposingNormalizer_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XDecomposingNormalizer ICU4XDecomposingNormalizer;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XDecomposingNormalizer_ICU4XError.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XDecomposingNormalizer_ICU4XError ICU4XDecomposingNormalizer_create_nfd(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XDecomposingNormalizer_ICU4XError ICU4XDecomposingNormalizer_create_nfkd(const ICU4XDataProvider* provider);

diplomat_result_void_ICU4XError ICU4XDecomposingNormalizer_normalize(const ICU4XDecomposingNormalizer* self, const char* s_data, size_t s_len, DiplomatWriteable* write);

bool ICU4XDecomposingNormalizer_is_normalized(const ICU4XDecomposingNormalizer* self, const char* s_data, size_t s_len);
void ICU4XDecomposingNormalizer_destroy(ICU4XDecomposingNormalizer* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
