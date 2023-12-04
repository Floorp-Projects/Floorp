#ifndef ICU4XCanonicalComposition_H
#define ICU4XCanonicalComposition_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XCanonicalComposition ICU4XCanonicalComposition;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XCanonicalComposition_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XCanonicalComposition_ICU4XError ICU4XCanonicalComposition_create(const ICU4XDataProvider* provider);

char32_t ICU4XCanonicalComposition_compose(const ICU4XCanonicalComposition* self, char32_t starter, char32_t second);
void ICU4XCanonicalComposition_destroy(ICU4XCanonicalComposition* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
