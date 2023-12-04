#ifndef ICU4XCodePointSetBuilder_H
#define ICU4XCodePointSetBuilder_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XCodePointSetBuilder ICU4XCodePointSetBuilder;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XCodePointSetData.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

ICU4XCodePointSetBuilder* ICU4XCodePointSetBuilder_create();

ICU4XCodePointSetData* ICU4XCodePointSetBuilder_build(ICU4XCodePointSetBuilder* self);

void ICU4XCodePointSetBuilder_complement(ICU4XCodePointSetBuilder* self);

bool ICU4XCodePointSetBuilder_is_empty(const ICU4XCodePointSetBuilder* self);

void ICU4XCodePointSetBuilder_add_char(ICU4XCodePointSetBuilder* self, char32_t ch);

void ICU4XCodePointSetBuilder_add_u32(ICU4XCodePointSetBuilder* self, uint32_t ch);

void ICU4XCodePointSetBuilder_add_inclusive_range(ICU4XCodePointSetBuilder* self, char32_t start, char32_t end);

void ICU4XCodePointSetBuilder_add_inclusive_range_u32(ICU4XCodePointSetBuilder* self, uint32_t start, uint32_t end);

void ICU4XCodePointSetBuilder_add_set(ICU4XCodePointSetBuilder* self, const ICU4XCodePointSetData* data);

void ICU4XCodePointSetBuilder_remove_char(ICU4XCodePointSetBuilder* self, char32_t ch);

void ICU4XCodePointSetBuilder_remove_inclusive_range(ICU4XCodePointSetBuilder* self, char32_t start, char32_t end);

void ICU4XCodePointSetBuilder_remove_set(ICU4XCodePointSetBuilder* self, const ICU4XCodePointSetData* data);

void ICU4XCodePointSetBuilder_retain_char(ICU4XCodePointSetBuilder* self, char32_t ch);

void ICU4XCodePointSetBuilder_retain_inclusive_range(ICU4XCodePointSetBuilder* self, char32_t start, char32_t end);

void ICU4XCodePointSetBuilder_retain_set(ICU4XCodePointSetBuilder* self, const ICU4XCodePointSetData* data);

void ICU4XCodePointSetBuilder_complement_char(ICU4XCodePointSetBuilder* self, char32_t ch);

void ICU4XCodePointSetBuilder_complement_inclusive_range(ICU4XCodePointSetBuilder* self, char32_t start, char32_t end);

void ICU4XCodePointSetBuilder_complement_set(ICU4XCodePointSetBuilder* self, const ICU4XCodePointSetData* data);
void ICU4XCodePointSetBuilder_destroy(ICU4XCodePointSetBuilder* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
