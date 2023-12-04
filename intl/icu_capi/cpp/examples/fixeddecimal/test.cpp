// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XFixedDecimalFormatter.hpp"
#include "../../include/ICU4XDataStruct.hpp"
#include "../../include/ICU4XLogger.hpp"

#include <iostream>
#include <array>

int main() {
    ICU4XLogger::init_simple_logger();
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

    decimal = ICU4XFixedDecimal::create_from_f64_with_floating_precision(100.01).ok().value();
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted float value is " << out << std::endl;
    if (out != "১০০.০১") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    decimal.pad_end(-4);
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted left-padded float value is " << out << std::endl;
    if (out != "১০০.০১০০") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    decimal.pad_start(4);
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted right-padded float value is " << out << std::endl;
    if (out != "০,১০০.০১০০") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    decimal.set_max_position(3);
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted truncated float value is " << out << std::endl;
    if (out != "১০০.০১০০") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    decimal = ICU4XFixedDecimal::create_from_f64_with_lower_magnitude(100.0006, -2).ok().value();
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted float value from precision 2 is " << out << std::endl;
    if (out != "১০০.০০") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    decimal = ICU4XFixedDecimal::create_from_f64_with_significant_digits(100.0006, 5).ok().value();
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted float value with 5 digits is " << out << std::endl;
    if (out != "১০০.০০") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    std::array<char32_t, 10> digits = {U'a', U'b', U'c', U'd', U'e', U'f', U'g', U'h', U'i', U'j'};

    auto data = ICU4XDataStruct::create_decimal_symbols_v1("+", "", "-", "", "/", "_", 4, 2, 4, digits).ok().value();

    fdf = ICU4XFixedDecimalFormatter::create_with_decimal_symbols_v1(data, ICU4XFixedDecimalGroupingStrategy::Auto).ok().value();

    decimal = ICU4XFixedDecimal::create_from_f64_with_floating_precision(123456.8901).ok().value();
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted float value for custom numeric system is " << out << std::endl;
    if (out != "bcdefg/ijab") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }
    decimal = ICU4XFixedDecimal::create_from_f64_with_floating_precision(123451234567.8901).ok().value();
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted float value for custom numeric system is " << out << std::endl;
    if (out != "bc_de_fb_cd_efgh/ijab") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }

    locale = ICU4XLocale::create_from_string("th-u-nu-thai").ok().value();
    std::cout << "Running test for locale " << locale.to_string().ok().value() << std::endl;
    fdf = ICU4XFixedDecimalFormatter::create_with_grouping_strategy(
        dp, locale, ICU4XFixedDecimalGroupingStrategy::Auto).ok().value();

    decimal = ICU4XFixedDecimal::create_from_f64_with_floating_precision(123456.8901).ok().value();
    out = fdf.format(decimal).ok().value();
    std::cout << "Formatted value is " << out << std::endl;
    if (out != "๑๒๓,๔๕๖.๘๙๐๑") {
        std::cout << "Output does not match expected output" << std::endl;
        return 1;
    }
    return 0;
}
