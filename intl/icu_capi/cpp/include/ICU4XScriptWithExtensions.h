#ifndef ICU4XScriptWithExtensions_H
#define ICU4XScriptWithExtensions_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XScriptWithExtensions ICU4XScriptWithExtensions;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XScriptWithExtensions_ICU4XError.h"
#include "ICU4XScriptWithExtensionsBorrowed.h"
#include "CodePointRangeIterator.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XScriptWithExtensions_ICU4XError ICU4XScriptWithExtensions_create(const ICU4XDataProvider* provider);

uint16_t ICU4XScriptWithExtensions_get_script_val(const ICU4XScriptWithExtensions* self, uint32_t code_point);

bool ICU4XScriptWithExtensions_has_script(const ICU4XScriptWithExtensions* self, uint32_t code_point, uint16_t script);

ICU4XScriptWithExtensionsBorrowed* ICU4XScriptWithExtensions_as_borrowed(const ICU4XScriptWithExtensions* self);

CodePointRangeIterator* ICU4XScriptWithExtensions_iter_ranges_for_script(const ICU4XScriptWithExtensions* self, uint16_t script);
void ICU4XScriptWithExtensions_destroy(ICU4XScriptWithExtensions* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
