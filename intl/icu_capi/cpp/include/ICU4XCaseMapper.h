#ifndef ICU4XCaseMapper_H
#define ICU4XCaseMapper_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XCaseMapper ICU4XCaseMapper;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XCaseMapper_ICU4XError.h"
#include "ICU4XLocale.h"
#include "diplomat_result_void_ICU4XError.h"
#include "ICU4XTitlecaseOptionsV1.h"
#include "ICU4XCodePointSetBuilder.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XCaseMapper_ICU4XError ICU4XCaseMapper_create(const ICU4XDataProvider* provider);

diplomat_result_void_ICU4XError ICU4XCaseMapper_lowercase(const ICU4XCaseMapper* self, const char* s_data, size_t s_len, const ICU4XLocale* locale, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XCaseMapper_uppercase(const ICU4XCaseMapper* self, const char* s_data, size_t s_len, const ICU4XLocale* locale, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XCaseMapper_titlecase_segment_with_only_case_data_v1(const ICU4XCaseMapper* self, const char* s_data, size_t s_len, const ICU4XLocale* locale, ICU4XTitlecaseOptionsV1 options, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XCaseMapper_fold(const ICU4XCaseMapper* self, const char* s_data, size_t s_len, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XCaseMapper_fold_turkic(const ICU4XCaseMapper* self, const char* s_data, size_t s_len, DiplomatWriteable* write);

void ICU4XCaseMapper_add_case_closure_to(const ICU4XCaseMapper* self, char32_t c, ICU4XCodePointSetBuilder* builder);

char32_t ICU4XCaseMapper_simple_lowercase(const ICU4XCaseMapper* self, char32_t ch);

char32_t ICU4XCaseMapper_simple_uppercase(const ICU4XCaseMapper* self, char32_t ch);

char32_t ICU4XCaseMapper_simple_titlecase(const ICU4XCaseMapper* self, char32_t ch);

char32_t ICU4XCaseMapper_simple_fold(const ICU4XCaseMapper* self, char32_t ch);

char32_t ICU4XCaseMapper_simple_fold_turkic(const ICU4XCaseMapper* self, char32_t ch);
void ICU4XCaseMapper_destroy(ICU4XCaseMapper* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
