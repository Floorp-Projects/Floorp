#ifndef ICU4XIanaToBcp47Mapper_H
#define ICU4XIanaToBcp47Mapper_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XIanaToBcp47Mapper ICU4XIanaToBcp47Mapper;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XIanaToBcp47Mapper_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XIanaToBcp47Mapper_ICU4XError ICU4XIanaToBcp47Mapper_create(const ICU4XDataProvider* provider);
void ICU4XIanaToBcp47Mapper_destroy(ICU4XIanaToBcp47Mapper* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif
