#ifndef ICU4XBcp47ToIanaMapper_H
#define ICU4XBcp47ToIanaMapper_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XBcp47ToIanaMapper ICU4XBcp47ToIanaMapper;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XBcp47ToIanaMapper_ICU4XError.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XBcp47ToIanaMapper_ICU4XError ICU4XBcp47ToIanaMapper_create(const ICU4XDataProvider* provider);

diplomat_result_void_ICU4XError ICU4XBcp47ToIanaMapper_get(const ICU4XBcp47ToIanaMapper* self, const char* value_data, size_t value_len, DiplomatWriteable* write);
void ICU4XBcp47ToIanaMapper_destroy(ICU4XBcp47ToIanaMapper* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
