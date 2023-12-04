// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XPluralRules.h"
#include "../../include/ICU4XLogger.h"
#include <string.h>
#include <stdio.h>

int main() {
    ICU4XLogger_init_simple_logger();
    diplomat_result_box_ICU4XLocale_ICU4XError locale_result = ICU4XLocale_create_from_string("ar", 2);
    if (!locale_result.is_ok) {
        return 1;
    }
    ICU4XLocale* locale = locale_result.ok;
    ICU4XDataProvider* provider = ICU4XDataProvider_create_compiled();
    diplomat_result_box_ICU4XPluralRules_ICU4XError plural_result = ICU4XPluralRules_create_cardinal(provider, locale);
    if (!plural_result.is_ok) {
        printf("Failed to create PluralRules\n");
        return 1;
    }
    ICU4XPluralRules* rules = plural_result.ok;

    ICU4XPluralCategories categories = ICU4XPluralRules_categories(rules);
    printf("Plural Category zero  (should be true): %s\n", categories.zero  ? "true" : "false");
    printf("Plural Category one   (should be true): %s\n", categories.one   ? "true" : "false");
    printf("Plural Category two   (should be true): %s\n", categories.two   ? "true" : "false");
    printf("Plural Category few   (should be true): %s\n", categories.few   ? "true" : "false");
    printf("Plural Category many  (should be true): %s\n", categories.many  ? "true" : "false");
    printf("Plural Category other (should be true): %s\n", categories.other ? "true" : "false");

    diplomat_result_box_ICU4XPluralOperands_ICU4XError op1_result = ICU4XPluralOperands_create_from_string("3", 1);

    if (!op1_result.is_ok) {
        printf("Failed to create PluralOperands from string\n");
        return 1;
    }

    ICU4XPluralCategory cat1 = ICU4XPluralRules_category_for(rules, op1_result.ok);

    printf("Plural Category %d (should be %d)\n", (int)cat1, (int)ICU4XPluralCategory_Few);

    diplomat_result_box_ICU4XPluralOperands_ICU4XError op2_result = ICU4XPluralOperands_create_from_string("1011.0", 6);

    if (!op2_result.is_ok) {
        printf("Failed to create PluralOperands from string\n");
        return 1;
    }

    ICU4XPluralCategory cat2 = ICU4XPluralRules_category_for(rules, op2_result.ok);

    printf("Plural Category %d (should be %d)\n", (int)cat2, (int)ICU4XPluralCategory_Many);

    ICU4XPluralRules_destroy(rules);
    ICU4XDataProvider_destroy(provider);
    ICU4XLocale_destroy(locale);

    if (!categories.zero)  { return 1; }
    if (!categories.one)   { return 1; }
    if (!categories.two)   { return 1; }
    if (!categories.few)   { return 1; }
    if (!categories.many)  { return 1; }
    if (!categories.other) { return 1; }

    if (cat1 != ICU4XPluralCategory_Few)  { return 1; }
    if (cat2 != ICU4XPluralCategory_Many) { return 1; }

    return 0;
}
