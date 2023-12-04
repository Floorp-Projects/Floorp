#ifndef ICU4XTitlecaseOptionsV1_H
#define ICU4XTitlecaseOptionsV1_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ICU4XLeadingAdjustment.h"
#include "ICU4XTrailingCase.h"
#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XTitlecaseOptionsV1 {
    ICU4XLeadingAdjustment leading_adjustment;
    ICU4XTrailingCase trailing_case;
} ICU4XTitlecaseOptionsV1;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XLeadingAdjustment.h"
#include "ICU4XTrailingCase.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

ICU4XTitlecaseOptionsV1 ICU4XTitlecaseOptionsV1_default_options();
void ICU4XTitlecaseOptionsV1_destroy(ICU4XTitlecaseOptionsV1* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
