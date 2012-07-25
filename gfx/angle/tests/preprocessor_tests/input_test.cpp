//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "PreprocessorTest.h"
#include "Input.h"
#include "Token.h"

class InitTest : public PreprocessorTest
{
};

TEST_F(InitTest, NegativeCount)
{
    EXPECT_FALSE(mPreprocessor.init(-1, NULL, NULL));
}

TEST_F(InitTest, ZeroCount)
{
    EXPECT_TRUE(mPreprocessor.init(0, NULL, NULL));

    pp::Token token;
    mPreprocessor.lex(&token);
    EXPECT_EQ(pp::Token::LAST, token.type);
}

TEST_F(InitTest, NullString)
{
    EXPECT_FALSE(mPreprocessor.init(1, NULL, NULL));
}

TEST(InputTest, DefaultConstructor)
{
    pp::Input input;
    EXPECT_EQ(0, input.count());
    EXPECT_EQ(0, input.read(NULL, 1));
}

TEST(InputTest, NullLength)
{
    const char* str[] = {"foo"};
    pp::Input input(1, str, NULL);
    EXPECT_EQ(3, input.length(0));
}

TEST(InputTest, NegativeLength)
{
    const char* str[] = {"foo"};
    int length[] = {-1};
    pp::Input input(1, str, length);
    EXPECT_EQ(3, input.length(0));
}

TEST(InputTest, ActualLength)
{
    const char* str[] = {"foobar"};
    int length[] = {3};
    pp::Input input(1, str, length);
    // Note that strlen(str[0]) != length[0].
    // Even then Input should just accept any non-negative number.
    EXPECT_EQ(length[0], input.length(0));
}

TEST(InputTest, String)
{
    const char* str[] = {"foo"};
    pp::Input input(1, str, NULL);
    EXPECT_STREQ(str[0], input.string(0));
}

TEST(InputTest, ReadSingleString)
{
    int count = 1;
    const char* str[] = {"foo"};
    char buf[4] = {'\0', '\0', '\0', '\0'};

    int maxSize = 1;
    pp::Input input1(count, str, NULL);
    EXPECT_EQ(1, input1.read(buf, maxSize));
    EXPECT_EQ('f', buf[0]);
    EXPECT_EQ(1, input1.read(buf, maxSize));
    EXPECT_EQ('o', buf[0]);
    EXPECT_EQ(1, input1.read(buf, maxSize));
    EXPECT_EQ('o', buf[0]);
    EXPECT_EQ(0, input1.read(buf, maxSize));

    maxSize = 2;
    pp::Input input2(count, str, NULL);
    EXPECT_EQ(2, input2.read(buf, maxSize));
    EXPECT_STREQ("fo", buf);
    EXPECT_EQ(1, input2.read(buf, maxSize));
    EXPECT_EQ('o', buf[0]);
    EXPECT_EQ(0, input2.read(buf, maxSize));

    maxSize = 3;
    pp::Input input3(count, str, NULL);
    EXPECT_EQ(3, input3.read(buf, maxSize));
    EXPECT_STREQ("foo", buf);
    EXPECT_EQ(0, input3.read(buf, maxSize));

    maxSize = 4;
    pp::Input input4(count, str, NULL);
    EXPECT_EQ(3, input4.read(buf, maxSize));
    EXPECT_STREQ("foo", buf);
    EXPECT_EQ(0, input4.read(buf, maxSize));
}

TEST(InputTest, ReadMultipleStrings)
{
    int count = 3;
    const char* str[] = {"f", "o", "o"};
    char buf[4] = {'\0', '\0', '\0', '\0'};

    int maxSize = 1;
    pp::Input input1(count, str, NULL);
    EXPECT_EQ(1, input1.read(buf, maxSize));
    EXPECT_EQ('f', buf[0]);
    EXPECT_EQ(1, input1.read(buf, maxSize));
    EXPECT_EQ('o', buf[0]);
    EXPECT_EQ(1, input1.read(buf, maxSize));
    EXPECT_EQ('o', buf[0]);
    EXPECT_EQ(0, input1.read(buf, maxSize));

    maxSize = 2;
    pp::Input input2(count, str, NULL);
    EXPECT_EQ(2, input2.read(buf, maxSize));
    EXPECT_STREQ("fo", buf);
    EXPECT_EQ(1, input2.read(buf, maxSize));
    EXPECT_EQ('o', buf[0]);
    EXPECT_EQ(0, input2.read(buf, maxSize));

    maxSize = 3;
    pp::Input input3(count, str, NULL);
    EXPECT_EQ(3, input3.read(buf, maxSize));
    EXPECT_STREQ("foo", buf);
    EXPECT_EQ(0, input3.read(buf, maxSize));

    maxSize = 4;
    pp::Input input4(count, str, NULL);
    EXPECT_EQ(3, input4.read(buf, maxSize));
    EXPECT_STREQ("foo", buf);
    EXPECT_EQ(0, input4.read(buf, maxSize));
}

TEST(InputTest, ReadStringsWithLength)
{
    int count = 2;
    const char* str[] = {"foo", "bar"};
    // Note that the length for the first string is 2 which is less than
    // strlen(str[0]. We want to make sure that the last character is ignored.
    int length[] = {2, 3};
    char buf[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};
    int maxSize = 5;

    pp::Input input(count, str, length);
    EXPECT_EQ(maxSize, input.read(buf, maxSize));
    EXPECT_STREQ("fobar", buf);
}

