// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XDataProvider.hpp"
#include "../../include/ICU4XGraphemeClusterSegmenter.hpp"
#include "../../include/ICU4XLineSegmenter.hpp"
#include "../../include/ICU4XSentenceSegmenter.hpp"
#include "../../include/ICU4XWordSegmenter.hpp"
#include "../../include/ICU4XLogger.hpp"

#include <iostream>
#include <string_view>

using std::cout;
using std::endl;

void print_ruler(size_t str_len) {
    for (size_t i = 0; i < str_len; i++) {
        if (i % 10 == 0) {
            cout << "0";
        } else if (i % 5 == 0) {
            cout << "5";
        } else {
            cout << ".";
        }
    }
    cout << endl;
}

template <typename Iterator>
void iterate_breakpoints(Iterator& iterator) {
    while (true) {
        int32_t breakpoint = iterator.next();
        if (breakpoint == -1) {
            break;
        }
        cout << " " << breakpoint;
    }
    cout << endl;
}

template <typename Iterator>
void iterate_word_breakpoints(Iterator& iterator) {
    while (true) {
        int32_t breakpoint = iterator.next();
        if (breakpoint == -1) {
            break;
        }
        cout << " " << breakpoint;
        switch (iterator.word_type()) {
            case ICU4XSegmenterWordType::None:
                cout << " (none";
                break;
            case ICU4XSegmenterWordType::Number:
                cout << " (number";
                break;
            case ICU4XSegmenterWordType::Letter:
                cout << " (letter";
                break;
            default:
                cout << " (unknown status";
                break;
        }
        if (iterator.is_word_like()) {
            cout << ", word-like";
        }
        cout << ")";
    }
    cout << endl;
}

void test_line(const std::string_view& str) {
    const auto provider = ICU4XDataProvider::create_compiled();
    const auto segmenter_auto =
        ICU4XLineSegmenter::create_auto(provider).ok().value();
    const auto segmenter_lstm =
        ICU4XLineSegmenter::create_lstm(provider).ok().value();
    const auto segmenter_dictionary =
        ICU4XLineSegmenter::create_dictionary(provider).ok().value();

    const ICU4XLineSegmenter* segmenters[] = {&segmenter_auto, &segmenter_lstm,
                                              &segmenter_dictionary};
    for (const auto* segmenter : segmenters) {
        cout << "Finding line breakpoints in string:" << endl << str << endl;
        print_ruler(str.size());

        cout << "Line breakpoints:";
        auto iterator = segmenter->segment_utf8(str);
        iterate_breakpoints(iterator);
    }
}

void test_grapheme(const std::string_view& str) {
    const auto provider = ICU4XDataProvider::create_compiled();
    const auto segmenter = ICU4XGraphemeClusterSegmenter::create(provider).ok().value();
    cout << "Finding grapheme cluster breakpoints in string:" << endl
         << str << endl;
    print_ruler(str.size());

    cout << "Grapheme cluster breakpoints:";
    auto iterator = segmenter.segment_utf8(str);
    iterate_breakpoints(iterator);
}

void test_word(const std::string_view& str) {
    const auto provider = ICU4XDataProvider::create_compiled();
    const auto segmenter_auto =
        ICU4XWordSegmenter::create_auto(provider).ok().value();
    const auto segmenter_lstm =
        ICU4XWordSegmenter::create_lstm(provider).ok().value();
    const auto segmenter_dictionary =
        ICU4XWordSegmenter::create_dictionary(provider).ok().value();

    const ICU4XWordSegmenter* segmenters[] = {&segmenter_auto, &segmenter_lstm,
                                              &segmenter_dictionary};
    for (const auto* segmenter : segmenters) {
        cout << "Finding word breakpoints in string:" << endl << str << endl;
        print_ruler(str.size());

        cout << "Word breakpoints:";
        auto iterator = segmenter->segment_utf8(str);
        iterate_word_breakpoints(iterator);
    }
}

void test_sentence(const std::string_view& str) {
    const auto provider = ICU4XDataProvider::create_compiled();
    const auto segmenter = ICU4XSentenceSegmenter::create(provider).ok().value();
    cout << "Finding sentence breakpoints in string:" << endl
         << str << endl;
    print_ruler(str.size());

    cout << "Sentence breakpoints:";
    auto iterator = segmenter.segment_utf8(str);
    iterate_breakpoints(iterator);
}

int main(int argc, char* argv[]) {
    ICU4XLogger::init_simple_logger();
    std::string_view str;
    if (argc >= 2) {
        str = argv[1];
    } else {
        str = "The 101 quick brown foxes jump over the lazy dog.";
    }

    test_line(str);
    cout << endl;

    test_grapheme(str);
    cout << endl;

    test_word(str);
    cout << endl;

    test_sentence(str);
    cout << endl;
    return 0;
}
