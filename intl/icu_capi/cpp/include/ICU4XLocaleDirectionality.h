#ifndef ICU4XLocaleDirectionality_H
#define ICU4XLocaleDirectionality_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XLocaleDirectionality ICU4XLocaleDirectionality;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XLocaleDirectionality_ICU4XError.h"
#include "ICU4XLocaleExpander.h"
#include "ICU4XLocale.h"
#include "ICU4XLocaleDirection.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XLocaleDirectionality_ICU4XError ICU4XLocaleDirectionality_create(const ICU4XDataProvider* provider);

diplomat_result_box_ICU4XLocaleDirectionality_ICU4XError ICU4XLocaleDirectionality_create_with_expander(const ICU4XDataProvider* provider, const ICU4XLocaleExpander* expander);

ICU4XLocaleDirection ICU4XLocaleDirectionality_get(const ICU4XLocaleDirectionality* self, const ICU4XLocale* locale);

bool ICU4XLocaleDirectionality_is_left_to_right(const ICU4XLocaleDirectionality* self, const ICU4XLocale* locale);

bool ICU4XLocaleDirectionality_is_right_to_left(const ICU4XLocaleDirectionality* self, const ICU4XLocale* locale);
void ICU4XLocaleDirectionality_destroy(ICU4XLocaleDirectionality* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
