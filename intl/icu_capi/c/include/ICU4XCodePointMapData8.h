#ifndef ICU4XCodePointMapData8_H
#define ICU4XCodePointMapData8_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XCodePointMapData8 ICU4XCodePointMapData8;
#ifdef __cplusplus
} // namespace capi
#endif
#include "CodePointRangeIterator.h"
#include "ICU4XCodePointSetData.h"
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XCodePointMapData8_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

uint8_t ICU4XCodePointMapData8_get(const ICU4XCodePointMapData8* self, char32_t cp);

uint8_t ICU4XCodePointMapData8_get32(const ICU4XCodePointMapData8* self, uint32_t cp);

uint32_t ICU4XCodePointMapData8_general_category_to_mask(uint8_t gc);

CodePointRangeIterator* ICU4XCodePointMapData8_iter_ranges_for_value(const ICU4XCodePointMapData8* self, uint8_t value);

CodePointRangeIterator* ICU4XCodePointMapData8_iter_ranges_for_value_complemented(const ICU4XCodePointMapData8* self, uint8_t value);

CodePointRangeIterator* ICU4XCodePointMapData8_iter_ranges_for_mask(const ICU4XCodePointMapData8* self, uint32_t mask);

ICU4XCodePointSetData* ICU4XCodePointMapData8_get_set_for_value(const ICU4XCodePointMapData8* self, uint8_t value);

diplomat_result_box_ICU4XCodePointMapData8_ICU4XError ICU4XCodePointMapData8_load_general_category(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XCodePointMapData8_ICU4XError ICU4XCodePointMapData8_load_bidi_class(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XCodePointMapData8_ICU4XError ICU4XCodePointMapData8_load_east_asian_width(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XCodePointMapData8_ICU4XError ICU4XCodePointMapData8_load_indic_syllabic_category(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XCodePointMapData8_ICU4XError ICU4XCodePointMapData8_load_line_break(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XCodePointMapData8_ICU4XError ICU4XCodePointMapData8_try_grapheme_cluster_break(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XCodePointMapData8_ICU4XError ICU4XCodePointMapData8_load_word_break(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XCodePointMapData8_ICU4XError ICU4XCodePointMapData8_load_sentence_break(const ICU4XDataProvider* provider);
void ICU4XCodePointMapData8_destroy(ICU4XCodePointMapData8* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
