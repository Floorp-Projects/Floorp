#ifndef ICU4XGregorianDateTimeFormatter_H
#define ICU4XGregorianDateTimeFormatter_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XGregorianDateTimeFormatter ICU4XGregorianDateTimeFormatter;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "ICU4XDateLength.h"
#include "ICU4XTimeLength.h"
#include "diplomat_result_box_ICU4XGregorianDateTimeFormatter_ICU4XError.h"
#include "ICU4XIsoDateTime.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XGregorianDateTimeFormatter_ICU4XError ICU4XGregorianDateTimeFormatter_create_with_lengths(const ICU4XDataProvider* provider, const ICU4XLocale* locale, ICU4XDateLength date_length, ICU4XTimeLength time_length);

diplomat_result_void_ICU4XError ICU4XGregorianDateTimeFormatter_format_iso_datetime(const ICU4XGregorianDateTimeFormatter* self, const ICU4XIsoDateTime* value, DiplomatWriteable* write);
void ICU4XGregorianDateTimeFormatter_destroy(ICU4XGregorianDateTimeFormatter* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
