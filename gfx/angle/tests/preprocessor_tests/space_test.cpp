//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "gtest/gtest.h"
#include "Preprocessor.h"
#include "Token.h"

// Whitespace characters allowed in GLSL.
// Note that newline characters (\n) will be tested separately.
static const char kSpaceChars[] = {' ', '\t', '\v', '\f'};

#if GTEST_HAS_PARAM_TEST

// This test fixture tests the processing of a single whitespace character.
// All tests in this fixture are ran with all possible whitespace character
// allowed in GLSL.
class SpaceCharTest : public testing::TestWithParam<char>
{
};

TEST_P(SpaceCharTest, SpaceIgnored)
{
    // Construct test string with the whitespace char before "foo".
    const char* identifier = "foo";
    std::string str(1, GetParam());
    str.append(identifier);
    const char* cstr = str.c_str();

    pp::Token token;
    pp::Preprocessor preprocessor;
    ASSERT_TRUE(preprocessor.init(1, &cstr, 0));
    // Identifier "foo" is returned after ignoring the whitespace characters.
    EXPECT_EQ(pp::Token::IDENTIFIER, preprocessor.lex(&token));
    EXPECT_EQ(pp::Token::IDENTIFIER, token.type);
    EXPECT_STREQ(identifier, token.value.c_str());
    // The whitespace character is however recorded with the next token.
    EXPECT_TRUE(token.hasLeadingSpace());
}

INSTANTIATE_TEST_CASE_P(SingleSpaceChar,
                        SpaceCharTest,
                        testing::ValuesIn(kSpaceChars));

#endif  // GTEST_HAS_PARAM_TEST

#if GTEST_HAS_COMBINE

// This test fixture tests the processing of a string containing consecutive
// whitespace characters. All tests in this fixture are ran with all possible
// combinations of whitespace characters allowed in GLSL.
typedef std::tr1::tuple<char, char, char> SpaceStringParams;
class SpaceStringTest : public testing::TestWithParam<SpaceStringParams>
{
};

TEST_P(SpaceStringTest, SpaceIgnored)
{
    // Construct test string with the whitespace char before "foo".
    const char* identifier = "foo";
    std::string str(1, std::tr1::get<0>(GetParam()));
    str.push_back(std::tr1::get<1>(GetParam()));
    str.push_back(std::tr1::get<2>(GetParam()));
    str.append(identifier);
    const char* cstr = str.c_str();

    pp::Token token;
    pp::Preprocessor preprocessor;
    ASSERT_TRUE(preprocessor.init(1, &cstr, 0));
    // Identifier "foo" is returned after ignoring the whitespace characters.
    EXPECT_EQ(pp::Token::IDENTIFIER, preprocessor.lex(&token));
    EXPECT_EQ(pp::Token::IDENTIFIER, token.type);
    EXPECT_STREQ(identifier, token.value.c_str());
    // The whitespace character is however recorded with the next token.
    EXPECT_TRUE(token.hasLeadingSpace());
}

INSTANTIATE_TEST_CASE_P(SpaceCharCombination,
                        SpaceStringTest,
                        testing::Combine(testing::ValuesIn(kSpaceChars),
                                         testing::ValuesIn(kSpaceChars),
                                         testing::ValuesIn(kSpaceChars)));

#endif  // GTEST_HAS_COMBINE

// The tests above make sure that the space char is recorded in the
// next token. This test makes sure that a token is not incorrectly marked
// to have leading space.
TEST(SpaceTest, LeadingSpace)
{
    const char* str = " foo+ -bar";

    pp::Token token;
    pp::Preprocessor preprocessor;
    ASSERT_TRUE(preprocessor.init(1, &str, 0));

    EXPECT_EQ(pp::Token::IDENTIFIER, preprocessor.lex(&token));
    EXPECT_EQ(pp::Token::IDENTIFIER, token.type);
    EXPECT_STREQ("foo", token.value.c_str());
    EXPECT_TRUE(token.hasLeadingSpace());

    EXPECT_EQ('+', preprocessor.lex(&token));
    EXPECT_EQ('+', token.type);
    EXPECT_FALSE(token.hasLeadingSpace());

    EXPECT_EQ('-', preprocessor.lex(&token));
    EXPECT_EQ('-', token.type);
    EXPECT_TRUE(token.hasLeadingSpace());

    EXPECT_EQ(pp::Token::IDENTIFIER, preprocessor.lex(&token));
    EXPECT_EQ(pp::Token::IDENTIFIER, token.type);
    EXPECT_STREQ("bar", token.value.c_str());
    EXPECT_FALSE(token.hasLeadingSpace());
}
