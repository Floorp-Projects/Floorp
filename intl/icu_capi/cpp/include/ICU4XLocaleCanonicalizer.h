#ifndef ICU4XLocaleCanonicalizer_H
#define ICU4XLocaleCanonicalizer_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XLocaleCanonicalizer ICU4XLocaleCanonicalizer;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XLocaleCanonicalizer_ICU4XError.h"
#include "ICU4XLocale.h"
#include "ICU4XTransformResult.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XLocaleCanonicalizer_ICU4XError ICU4XLocaleCanonicalizer_create(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XLocaleCanonicalizer_ICU4XError ICU4XLocaleCanonicalizer_create_extended(const ICU4XDataProvider* provider);

ICU4XTransformResult ICU4XLocaleCanonicalizer_canonicalize(const ICU4XLocaleCanonicalizer* self, ICU4XLocale* locale);
void ICU4XLocaleCanonicalizer_destroy(ICU4XLocaleCanonicalizer* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
