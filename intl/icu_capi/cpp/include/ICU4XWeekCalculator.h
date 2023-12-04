#ifndef ICU4XWeekCalculator_H
#define ICU4XWeekCalculator_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XWeekCalculator ICU4XWeekCalculator;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "ICU4XLocale.h"
#include "diplomat_result_box_ICU4XWeekCalculator_ICU4XError.h"
#include "ICU4XIsoWeekday.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XWeekCalculator_ICU4XError ICU4XWeekCalculator_create(const ICU4XDataProvider* provider, const ICU4XLocale* locale);

ICU4XWeekCalculator* ICU4XWeekCalculator_create_from_first_day_of_week_and_min_week_days(ICU4XIsoWeekday first_weekday, uint8_t min_week_days);

ICU4XIsoWeekday ICU4XWeekCalculator_first_weekday(const ICU4XWeekCalculator* self);

uint8_t ICU4XWeekCalculator_min_week_days(const ICU4XWeekCalculator* self);
void ICU4XWeekCalculator_destroy(ICU4XWeekCalculator* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
