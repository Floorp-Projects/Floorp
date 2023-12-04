#ifndef ICU4XTitlecaseMapper_H
#define ICU4XTitlecaseMapper_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XTitlecaseMapper ICU4XTitlecaseMapper;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XTitlecaseMapper_ICU4XError.h"
#include "ICU4XLocale.h"
#include "ICU4XTitlecaseOptionsV1.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XTitlecaseMapper_ICU4XError ICU4XTitlecaseMapper_create(const ICU4XDataProvider* provider);

diplomat_result_void_ICU4XError ICU4XTitlecaseMapper_titlecase_segment_v1(const ICU4XTitlecaseMapper* self, const char* s_data, size_t s_len, const ICU4XLocale* locale, ICU4XTitlecaseOptionsV1 options, DiplomatWriteable* write);
void ICU4XTitlecaseMapper_destroy(ICU4XTitlecaseMapper* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
