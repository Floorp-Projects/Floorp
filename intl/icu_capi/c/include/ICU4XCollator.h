#ifndef ICU4XCollator_H
#define ICU4XCollator_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XCollator ICU4XCollator;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "ICU4XCollatorOptionsV1.h"
#include "diplomat_result_box_ICU4XCollator_ICU4XError.h"
#include "ICU4XOrdering.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XCollator_ICU4XError ICU4XCollator_create_v1(const ICU4XDataProvider* provider, const ICU4XLocale* locale, ICU4XCollatorOptionsV1 options);

ICU4XOrdering ICU4XCollator_compare(const ICU4XCollator* self, const char* left_data, size_t left_len, const char* right_data, size_t right_len);

ICU4XOrdering ICU4XCollator_compare_valid_utf8(const ICU4XCollator* self, const char* left_data, size_t left_len, const char* right_data, size_t right_len);

ICU4XOrdering ICU4XCollator_compare_utf16(const ICU4XCollator* self, const uint16_t* left_data, size_t left_len, const uint16_t* right_data, size_t right_len);
void ICU4XCollator_destroy(ICU4XCollator* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
