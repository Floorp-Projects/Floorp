//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "PreprocessorTest.h"
#include "Token.h"

class IfTest : public PreprocessorTest
{
};

TEST_F(IfTest, If_0)
{
    const char* str = "pass_1\n"
                      "#if 0\n"
                      "fail\n"
                      "#endif\n"
                      "pass_2\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n";

    preprocess(str, expected);
}

TEST_F(IfTest, If_1)
{
    const char* str = "pass_1\n"
                      "#if 1\n"
                      "pass_2\n"
                      "#endif\n"
                      "pass_3\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "pass_3\n";

    preprocess(str, expected);
}

TEST_F(IfTest, If_0_Else)
{
    const char* str = "pass_1\n"
                      "#if 0\n"
                      "fail\n"
                      "#else\n"
                      "pass_2\n"
                      "#endif\n"
                      "pass_3\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "pass_3\n";

    preprocess(str, expected);
}

TEST_F(IfTest, If_1_Else)
{
    const char* str = "pass_1\n"
                      "#if 1\n"
                      "pass_2\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n"
                      "pass_3\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_3\n";

    preprocess(str, expected);
}

TEST_F(IfTest, If_0_Elif)
{
    const char* str = "pass_1\n"
                      "#if 0\n"
                      "fail_1\n"
                      "#elif 0\n"
                      "fail_2\n"
                      "#elif 1\n"
                      "pass_2\n"
                      "#elif 1\n"
                      "fail_3\n"
                      "#else\n"
                      "fail_4\n"
                      "#endif\n"
                      "pass_3\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_3\n";

    preprocess(str, expected);
}

TEST_F(IfTest, If_1_Elif)
{
    const char* str = "pass_1\n"
                      "#if 1\n"
                      "pass_2\n"
                      "#elif 0\n"
                      "fail_1\n"
                      "#elif 1\n"
                      "fail_2\n"
                      "#else\n"
                      "fail_4\n"
                      "#endif\n"
                      "pass_3\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_3\n";

    preprocess(str, expected);
}

TEST_F(IfTest, If_Elif_Else)
{
    const char* str = "pass_1\n"
                      "#if 0\n"
                      "fail_1\n"
                      "#elif 0\n"
                      "fail_2\n"
                      "#elif 0\n"
                      "fail_3\n"
                      "#else\n"
                      "pass_2\n"
                      "#endif\n"
                      "pass_3\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "pass_3\n";

    preprocess(str, expected);
}

TEST_F(IfTest, If_0_Nested)
{
    const char* str = "pass_1\n"
                      "#if 0\n"
                      "fail_1\n"
                      "#if 1\n"
                      "fail_2\n"
                      "#else\n"
                      "fail_3\n"
                      "#endif\n"
                      "#else\n"
                      "pass_2\n"
                      "#endif\n"
                      "pass_3\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "pass_3\n";

    preprocess(str, expected);
}

TEST_F(IfTest, If_1_Nested)
{
    const char* str = "pass_1\n"
                      "#if 1\n"
                      "pass_2\n"
                      "#if 1\n"
                      "pass_3\n"
                      "#else\n"
                      "fail_1\n"
                      "#endif\n"
                      "#else\n"
                      "fail_2\n"
                      "#endif\n"
                      "pass_4\n";
    const char* expected = "pass_1\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "pass_3\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_4\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorPrecedence)
{
    const char* str = "#if 1 + 2 * 3 + - (26 % 17 - + 4 / 2)\n"
                      "fail_1\n"
                      "#else\n"
                      "pass_1\n"
                      "#endif\n";
    const char* expected = "\n"
                           "\n"
                           "\n"
                           "pass_1\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorDefined)
{
    const char* str = "#if defined foo\n"
                      "fail_1\n"
                      "#else\n"
                      "pass_1\n"
                      "#endif\n"
                      "#define foo\n"
                      "#if defined(foo)\n"
                      "pass_2\n"
                      "#else\n"
                      "fail_2\n"
                      "#endif\n"
                      "#undef foo\n"
                      "#if defined ( foo ) \n"
                      "fail_3\n"
                      "#else\n"
                      "pass_3\n"
                      "#endif\n";
    const char* expected = "\n"
                           "\n"
                           "\n"
                           "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_3\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorEQ)
{
    const char* str = "#if 4 - 1 == 2 + 1\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorNE)
{
    const char* str = "#if 1 != 2\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorLess)
{
    const char* str = "#if 1 < 2\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorGreater)
{
    const char* str = "#if 2 > 1\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorLE)
{
    const char* str = "#if 1 <= 2\n"
                      "pass_1\n"
                      "#else\n"
                      "fail_1\n"
                      "#endif\n"
                      "#if 2 <= 2\n"
                      "pass_2\n"
                      "#else\n"
                      "fail_2\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorGE)
{
    const char* str = "#if 2 >= 1\n"
                      "pass_1\n"
                      "#else\n"
                      "fail_1\n"
                      "#endif\n"
                      "#if 2 >= 2\n"
                      "pass_2\n"
                      "#else\n"
                      "fail_2\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorBitwiseOR)
{
    const char* str = "#if (0xaaaaaaaa | 0x55555555) == 0xffffffff\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorBitwiseAND)
{
    const char* str = "#if (0xaaaaaaa & 0x5555555) == 0\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorBitwiseXOR)
{
    const char* str = "#if (0xaaaaaaa ^ 0x5555555) == 0xfffffff\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorBitwiseComplement)
{
    const char* str = "#if (~ 0xdeadbeef) == -3735928560\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorLeft)
{
    const char* str = "#if (1 << 12) == 4096\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, OperatorRight)
{
    const char* str = "#if (31762 >> 8) == 124\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, ExpressionWithMacros)
{
    const char* str = "#define one 1\n"
                      "#define two 2\n"
                      "#define three 3\n"
                      "#if one + two == three\n"
                      "pass\n"
                      "#else\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, JunkInsideExcludedBlockIgnored)
{
    const char* str = "#if 0\n"
                      "foo !@#$%^&* .1bar\n"
                      "#foo\n"
                      "#if bar\n"
                      "fail\n"
                      "#endif\n"
                      "#else\n"
                      "pass\n"
                      "#endif\n";
    const char* expected = "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, Ifdef)
{
    const char* str = "#define foo\n"
                      "#ifdef foo\n"
                      "pass_1\n"
                      "#else\n"
                      "fail_1\n"
                      "#endif\n"
                      "#undef foo\n"
                      "#ifdef foo\n"
                      "fail_2\n"
                      "#else\n"
                      "pass_2\n"
                      "#endif\n";
    const char* expected = "\n"
                           "\n"
                           "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, Ifndef)
{
    const char* str = "#define foo\n"
                      "#ifndef foo\n"
                      "fail_1\n"
                      "#else\n"
                      "pass_1\n"
                      "#endif\n"
                      "#undef foo\n"
                      "#ifndef foo\n"
                      "pass_2\n"
                      "#else\n"
                      "fail_2\n"
                      "#endif\n";
    const char* expected = "\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_1\n"
                           "\n"
                           "\n"
                           "\n"
                           "pass_2\n"
                           "\n"
                           "\n"
                           "\n";

    preprocess(str, expected);
}

TEST_F(IfTest, MissingExpression)
{
    const char* str = "#if\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::INVALID_EXPRESSION,
                      pp::SourceLocation(0, 1),
                      "syntax error"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, DivisionByZero)
{
    const char* str = "#if 1 / (3 - (1 + 2))\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::DIVISION_BY_ZERO,
                      pp::SourceLocation(0, 1), "1 / 0"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, ModuloByZero)
{
    const char* str = "#if 1 % (3 - (1 + 2))\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::DIVISION_BY_ZERO,
                      pp::SourceLocation(0, 1), "1 % 0"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, DecIntegerOverflow)
{
    const char* str = "#if 4294967296\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::INTEGER_OVERFLOW,
                      pp::SourceLocation(0, 1), "4294967296"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, OctIntegerOverflow)
{
    const char* str = "#if 077777777777\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::INTEGER_OVERFLOW,
                      pp::SourceLocation(0, 1), "077777777777"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, HexIntegerOverflow)
{
    const char* str = "#if 0xfffffffff\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::INTEGER_OVERFLOW,
                      pp::SourceLocation(0, 1), "0xfffffffff"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, UndefinedMacro)
{
    const char* str = "#if UNDEFINED\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::INVALID_EXPRESSION,
                      pp::SourceLocation(0, 1),
                      "syntax error"));
    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::CONDITIONAL_UNEXPECTED_TOKEN,
                      pp::SourceLocation(0, 1),
                      "UNDEFINED"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, InvalidExpressionIgnoredForExcludedElif)
{
    const char* str = "#if 1\n"
                      "pass\n"
                      "#elif UNDEFINED\n"
                      "fail\n"
                      "#endif\n";
    const char* expected = "\n"
                           "pass\n"
                           "\n"
                           "\n"
                           "\n";

    // No error or warning.
    using testing::_;
    EXPECT_CALL(mDiagnostics, print(_, _, _)).Times(0);

    preprocess(str, expected);
}

TEST_F(IfTest, ElseWithoutIf)
{
    const char* str = "#else\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::CONDITIONAL_ELSE_WITHOUT_IF,
                      pp::SourceLocation(0, 1),
                      "else"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, ElifWithoutIf)
{
    const char* str = "#elif 1\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::CONDITIONAL_ELIF_WITHOUT_IF,
                      pp::SourceLocation(0, 1),
                      "elif"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, EndifWithoutIf)
{
    const char* str = "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::CONDITIONAL_ENDIF_WITHOUT_IF,
                      pp::SourceLocation(0, 1),
                      "endif"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, ElseAfterElse)
{
    const char* str = "#if 1\n"
                      "#else\n"
                      "#else\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::CONDITIONAL_ELSE_AFTER_ELSE,
                      pp::SourceLocation(0, 3),
                      "else"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, ElifAfterElse)
{
    const char* str = "#if 1\n"
                      "#else\n"
                      "#elif 0\n"
                      "#endif\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::CONDITIONAL_ELIF_AFTER_ELSE,
                      pp::SourceLocation(0, 3),
                      "elif"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, UnterminatedIf)
{
    const char* str = "#if 1\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::CONDITIONAL_UNTERMINATED,
                      pp::SourceLocation(0, 1),
                      "if"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

TEST_F(IfTest, UnterminatedIfdef)
{
    const char* str = "#ifdef foo\n";
    ASSERT_TRUE(mPreprocessor.init(1, &str, 0));

    EXPECT_CALL(mDiagnostics,
                print(pp::Diagnostics::CONDITIONAL_UNTERMINATED,
                      pp::SourceLocation(0, 1),
                      "ifdef"));

    pp::Token token;
    mPreprocessor.lex(&token);
}

