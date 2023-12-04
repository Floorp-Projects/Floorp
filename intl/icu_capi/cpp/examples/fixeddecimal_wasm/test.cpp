// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#endif

#include "../../include/ICU4XFixedDecimalFormatter.hpp"
#include "../../include/ICU4XLogger.hpp"

#include <iostream>

extern "C" void diplomat_init();
extern "C" void log_js(char* s, u_int len) {
    std::cout<<"LOG: " << std::string_view(s, len) <<std::endl;
}
extern "C" void warn_js(char* s, u_int len) {
    std::cout<<"WARN: " << std::string_view(s, len) <<std::endl;
}

int runFixedDecimal() {
#ifdef __EMSCRIPTEN__
    diplomat_init();
    ICU4XLogger::init_console_logger();
#endif
    ICU4XLocale locale = ICU4XLocale::create_from_string("bn").ok().value();
    std::cout << "Running test for locale " << locale.to_string().ok().value() << std::endl;
    ICU4XDataProvider dp = ICU4XDataProvider::create_compiled();
    ICU4XFixedDecimalFormatter fdf = ICU4XFixedDecimalFormatter::create_with_grouping_strategy(
        dp, locale, ICU4XFixedDecimalGroupingStrategy::Auto).ok().value();

    ICU4XFixedDecimal decimal = ICU4XFixedDecimal::create_from_u64(1000007);
    std::string out = fdf.format(decimal).ok().value();
    std::cout << "Formatted value is " << out << std::endl;
    if (out != "১০,০০,০০৭") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    std::string out2;
    fdf.format_to_writeable(decimal, out2);
    std::cout << "Formatted writeable value is " << out2 << std::endl;
    if (out2 != "১০,০০,০০৭") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    decimal.multiply_pow10(2);
    decimal.set_sign(ICU4XFixedDecimalSign::Negative);
    out = fdf.format(decimal).ok().value();
    std::cout << "Value x100 and negated is " << out << std::endl;
    if (out != "-১০,০০,০০,৭০০") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }
    return 0;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(testFixedDecimal) {
  emscripten::function("runFixedDecimal", &runFixedDecimal);
}
#endif

#ifndef NOMAIN
int main() {
    return runFixedDecimal();
}
#endif
