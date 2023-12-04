// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XDataProvider.hpp"
#include "../../include/ICU4XBidi.hpp"
#include "../../include/ICU4XLogger.hpp"

#include <iostream>

int main() {
    ICU4XDataProvider dp = ICU4XDataProvider::create_compiled();
    auto bidi = ICU4XBidi::create(dp).ok().value();

    // Written char-by-char to avoid messing up certain text editors.
    std::string_view str = 
        "א"
        "ב"
        "ג"
        "a"
        "b"
        "c"
        "\n"
        "a"
        "b"
        "c"
        "א"
        "ב"
        "ג";

    // reordered string is same for both lines
    std::string_view reordered_expected =
        "a"
        "b"
        "c"
        "ג"
        "ב"
        "א";
    auto bidi_info = bidi.for_text(str, ICU4XBidi::level_ltr());
    auto n_para = bidi_info.paragraph_count();
    if (n_para != 2) {
        std::cout << "Expected 2 paragraphs, found " << n_para << std::endl;
        return 1;
    }

    auto para = bidi_info.paragraph_at(0).value();

    auto size = para.size();
    if (size != 10) {
        std::cout << "Expected paragraph of size 10, found " << size << std::endl;
        return 1;
    }

    // The first paragraph's first strongly directional character is RTL
    uint8_t level = para.level_at(0);
    std::cout << "Level of first paragraph at index 0 is " << unsigned(level) << std::endl;
    if (!ICU4XBidi::level_is_rtl(level)) {
        std::cout << "Expected level at index 0 to be RTL" << std::endl;
        return 1;
    }


    std::string reordered = para.reorder_line(0, 9).ok().value();
    std::cout << "Reordered paragraph: " << reordered << std::endl;

    if (reordered != reordered) {
        std::cout << "Found incorrect reordering, expected: " << reordered << std::endl;
    }

    para.set_paragraph_in_text(1).ok().value();

    size = para.size();
    if (size != 9) {
        std::cout << "Expected paragraph of size 9, found " << size << std::endl;
        return 1;
    }

    // The second paragraph's first strongly directional character is LTR
    level = para.level_at(0);
    std::cout << "Level of second paragraph at index 0 is " << unsigned(level) << std::endl;
    if (!ICU4XBidi::level_is_ltr(level)) {
        std::cout << "Expected level at index 0 to be LTR" << std::endl;
        return 1;
    }
    reordered = para.reorder_line(10, 19).ok().value();
    std::cout << "Reordered paragraph: " << reordered << std::endl;

    if (reordered != reordered) {
        std::cout << "Found incorrect reordering, expected: " << reordered << std::endl;
    }

    return 0;
}
