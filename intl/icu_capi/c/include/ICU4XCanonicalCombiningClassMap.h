#ifndef ICU4XCanonicalCombiningClassMap_H
#define ICU4XCanonicalCombiningClassMap_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XCanonicalCombiningClassMap ICU4XCanonicalCombiningClassMap;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XCanonicalCombiningClassMap_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XCanonicalCombiningClassMap_ICU4XError ICU4XCanonicalCombiningClassMap_create(const ICU4XDataProvider* provider);

uint8_t ICU4XCanonicalCombiningClassMap_get(const ICU4XCanonicalCombiningClassMap* self, char32_t ch);

uint8_t ICU4XCanonicalCombiningClassMap_get32(const ICU4XCanonicalCombiningClassMap* self, uint32_t ch);
void ICU4XCanonicalCombiningClassMap_destroy(ICU4XCanonicalCombiningClassMap* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
