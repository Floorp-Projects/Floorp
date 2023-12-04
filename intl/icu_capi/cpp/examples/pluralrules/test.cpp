// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XPluralRules.hpp"
#include "../../include/ICU4XLogger.hpp"

#include <iostream>

const std::string_view path = "../../../../../provider/datagen/tests/data/json/";

int main() {
    ICU4XLogger::init_simple_logger();
    ICU4XLocale locale = ICU4XLocale::create_from_string("ar").ok().value();
    std::cout << "Running test for locale " << locale.to_string().ok().value() << std::endl;
    ICU4XDataProvider dp = ICU4XDataProvider::create_fs(path).ok().value();
    ICU4XPluralRules pr = ICU4XPluralRules::create_cardinal(dp, locale).ok().value();

    ICU4XPluralOperands op = ICU4XPluralOperands::create_from_string("3").ok().value();
    ICU4XPluralCategory cat = pr.category_for(op);

    std::cout << "Category is " << static_cast<int32_t>(cat)
                                << " (should be " << static_cast<int32_t>(ICU4XPluralCategory::Few) << ")"
                                << std::endl;
    if (cat != ICU4XPluralCategory::Few) {
        return 1;
    }

    op = ICU4XPluralOperands::create_from_string("1011.0").ok().value();
    cat = pr.category_for(op);
    std::cout << "Category is " << static_cast<int32_t>(cat)
                                << " (should be " << static_cast<int32_t>(ICU4XPluralCategory::Many) << ")"
                                << std::endl;
    if (cat != ICU4XPluralCategory::Many) {
        return 1;
    }
    return 0;
}
