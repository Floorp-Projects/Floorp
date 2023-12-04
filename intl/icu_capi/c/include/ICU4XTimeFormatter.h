#ifndef ICU4XTimeFormatter_H
#define ICU4XTimeFormatter_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XTimeFormatter ICU4XTimeFormatter;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "ICU4XTimeLength.h"
#include "diplomat_result_box_ICU4XTimeFormatter_ICU4XError.h"
#include "ICU4XTime.h"
#include "diplomat_result_void_ICU4XError.h"
#include "ICU4XDateTime.h"
#include "ICU4XIsoDateTime.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XTimeFormatter_ICU4XError ICU4XTimeFormatter_create_with_length(const ICU4XDataProvider* provider, const ICU4XLocale* locale, ICU4XTimeLength length);

diplomat_result_void_ICU4XError ICU4XTimeFormatter_format_time(const ICU4XTimeFormatter* self, const ICU4XTime* value, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XTimeFormatter_format_datetime(const ICU4XTimeFormatter* self, const ICU4XDateTime* value, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XTimeFormatter_format_iso_datetime(const ICU4XTimeFormatter* self, const ICU4XIsoDateTime* value, DiplomatWriteable* write);
void ICU4XTimeFormatter_destroy(ICU4XTimeFormatter* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
