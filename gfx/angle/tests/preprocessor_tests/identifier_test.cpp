//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "gtest/gtest.h"
#include "Preprocessor.h"
#include "Token.h"

static void PreprocessAndVerifyIdentifier(const char* str)
{
    pp::Token token;
    pp::Preprocessor preprocessor;
    ASSERT_TRUE(preprocessor.init(1, &str, 0));
    EXPECT_EQ(pp::Token::IDENTIFIER, preprocessor.lex(&token));
    EXPECT_EQ(pp::Token::IDENTIFIER, token.type);
    EXPECT_STREQ(str, token.value.c_str());
}

#if GTEST_HAS_COMBINE

typedef std::tr1::tuple<char, char> IdentifierParams;
class IdentifierTest : public testing::TestWithParam<IdentifierParams>
{
};

// This test covers identifier names of form [_a-zA-Z][_a-zA-Z0-9]?.
TEST_P(IdentifierTest, IdentifierIdentified)
{
    std::string str(1, std::tr1::get<0>(GetParam()));
    char c = std::tr1::get<1>(GetParam());
    if (c != '\0') str.push_back(c);

    PreprocessAndVerifyIdentifier(str.c_str());
}

// Test string: '_'
INSTANTIATE_TEST_CASE_P(SingleLetter_Underscore,
                        IdentifierTest,
                        testing::Combine(testing::Values('_'),
                                         testing::Values('\0')));

// Test string: [a-f]
INSTANTIATE_TEST_CASE_P(SingleLetter_a_f,
                        IdentifierTest,
                        testing::Combine(testing::Range('a', 'f'),
                                         testing::Values('\0')));

// Test string: [A-F]
INSTANTIATE_TEST_CASE_P(SingleLetter_A_F,
                        IdentifierTest,
                        testing::Combine(testing::Range('A', 'F'),
                                         testing::Values('\0')));

// Test string: "__"
INSTANTIATE_TEST_CASE_P(DoubleLetter_Underscore_Underscore,
                        IdentifierTest,
                        testing::Combine(testing::Values('_'),
                                         testing::Values('_')));

// Test string: "_"[a-f]
INSTANTIATE_TEST_CASE_P(DoubleLetter_Underscore_a_f,
                        IdentifierTest,
                        testing::Combine(testing::Values('_'),
                                         testing::Range('a', 'f')));

// Test string: "_"[A-F]
INSTANTIATE_TEST_CASE_P(DoubleLetter_Underscore_A_F,
                        IdentifierTest,
                        testing::Combine(testing::Values('_'),
                                         testing::Range('A', 'F')));

// Test string: "_"[0-9]
INSTANTIATE_TEST_CASE_P(DoubleLetter_Underscore_0_9,
                        IdentifierTest,
                        testing::Combine(testing::Values('_'),
                                         testing::Range('0', '9')));

// Test string: [a-f]"_"
INSTANTIATE_TEST_CASE_P(DoubleLetter_a_f_Underscore,
                        IdentifierTest,
                        testing::Combine(testing::Range('a', 'f'),
                                         testing::Values('_')));

// Test string: [a-f][a-f]
INSTANTIATE_TEST_CASE_P(DoubleLetter_a_f_a_f,
                        IdentifierTest,
                        testing::Combine(testing::Range('a', 'f'),
                                         testing::Range('a', 'f')));

// Test string: [a-f][A-f]
INSTANTIATE_TEST_CASE_P(DoubleLetter_a_f_A_F,
                        IdentifierTest,
                        testing::Combine(testing::Range('a', 'f'),
                                         testing::Range('A', 'F')));

// Test string: [a-f][0-9]
INSTANTIATE_TEST_CASE_P(DoubleLetter_a_f_0_9,
                        IdentifierTest,
                        testing::Combine(testing::Range('a', 'f'),
                                         testing::Range('0', '9')));

// Test string: [A-F]"_"
INSTANTIATE_TEST_CASE_P(DoubleLetter_A_F_Underscore,
                        IdentifierTest,
                        testing::Combine(testing::Range('A', 'F'),
                                         testing::Values('_')));

// Test string: [A-F][a-f]
INSTANTIATE_TEST_CASE_P(DoubleLetter_A_F_a_f,
                        IdentifierTest,
                        testing::Combine(testing::Range('A', 'F'),
                                         testing::Range('a', 'f')));

// Test string: [A-F][A-F]
INSTANTIATE_TEST_CASE_P(DoubleLetter_A_F_A_F,
                        IdentifierTest,
                        testing::Combine(testing::Range('A', 'F'),
                                         testing::Range('A', 'F')));

// Test string: [A-F][0-9]
INSTANTIATE_TEST_CASE_P(DoubleLetter_A_F_0_9,
                        IdentifierTest,
                        testing::Combine(testing::Range('A', 'F'),
                                         testing::Range('0', '9')));

#endif  // GTEST_HAS_COMBINE

// The tests above cover one-letter and various combinations of two-letter
// identifier names. This test covers all characters in a single string.
TEST(IdentifierTestAllCharacters, IdentifierIdentified)
{
    std::string str;
    for (int c = 'a'; c <= 'z'; ++c)
        str.push_back(c);

    str.push_back('_');

    for (int c = 'A'; c <= 'Z'; ++c)
        str.push_back(c);

    str.push_back('_');

    for (int c = '0'; c <= '9'; ++c)
        str.push_back(c);

    PreprocessAndVerifyIdentifier(str.c_str());
}
