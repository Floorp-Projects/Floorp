// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XFixedDecimalFormatter.h"
#include "../../include/ICU4XLogger.h"
#include <string.h>
#include <stdio.h>

int main() {
    ICU4XLogger_init_simple_logger();
    diplomat_result_box_ICU4XLocale_ICU4XError locale_result = ICU4XLocale_create_from_string("bn", 2);
    if (!locale_result.is_ok) {
        return 1;
    }
    ICU4XLocale* locale = locale_result.ok;
    ICU4XDataProvider* provider = ICU4XDataProvider_create_compiled();

    ICU4XFixedDecimal* decimal = ICU4XFixedDecimal_create_from_u64(1000007);

    diplomat_result_box_ICU4XFixedDecimalFormatter_ICU4XError fdf_result =
        ICU4XFixedDecimalFormatter_create_with_grouping_strategy(provider, locale, ICU4XFixedDecimalGroupingStrategy_Auto);
    if (!fdf_result.is_ok)  {
        printf("Failed to create FixedDecimalFormatter\n");
        return 1;
    }
    ICU4XFixedDecimalFormatter* fdf = fdf_result.ok;
    char output[40];

    DiplomatWriteable write = diplomat_simple_writeable(output, 40);

    bool success = ICU4XFixedDecimalFormatter_format(fdf, decimal, &write).is_ok;
    if (!success) {
        printf("Failed to write result of FixedDecimalFormatter::format to string.\n");
        return 1;
    }
    printf("Output is %s\n", output);

    const char* expected = u8"১০,০০,০০৭";
    if (strcmp(output, expected) != 0) {
        printf("Output does not match expected output!\n");
        return 1;
    }

    ICU4XFixedDecimal_multiply_pow10(decimal, 2);
    if (!success) {
        printf("Failed to multiply FixedDecimal\n");
        return 1;
    }

    ICU4XFixedDecimal_set_sign(decimal, ICU4XFixedDecimalSign_Negative);

    write = diplomat_simple_writeable(output, 40);

    success = ICU4XFixedDecimalFormatter_format(fdf, decimal, &write).is_ok;
    if (!success) {
        printf("Failed to write result of FixedDecimalFormatter::format to string.\n");
        return 1;
    }
    printf("Output x100 and negated is %s\n", output);

    expected = u8"-১০,০০,০০,৭০০";
    if (strcmp(output, expected) != 0) {
        printf("Output does not match expected output!\n");
        return 1;
    }

    ICU4XFixedDecimal_destroy(decimal);

    diplomat_result_box_ICU4XFixedDecimal_ICU4XError fd_result = ICU4XFixedDecimal_create_from_string("1000007.070", 11);
    if (!fd_result.is_ok) {
        printf("Failed to create FixedDecimal from string.\n");
        return 1;
    }
    decimal = fd_result.ok;

    write = diplomat_simple_writeable(output, 40);

    success = ICU4XFixedDecimalFormatter_format(fdf, decimal, &write).is_ok;
    if (!success) {
        printf("Failed to write result of FixedDecimalFormatter::format to string.\n");
        return 1;
    }
    printf("Output is %s\n", output);

    expected = u8"১০,০০,০০৭.০৭০";
    if (strcmp(output, expected) != 0) {
        printf("Output does not match expected output!\n");
        return 1;
    }

    ICU4XFixedDecimal_destroy(decimal);
    ICU4XFixedDecimalFormatter_destroy(fdf);
    ICU4XLocale_destroy(locale);
    ICU4XDataProvider_destroy(provider);

    return 0;
}
