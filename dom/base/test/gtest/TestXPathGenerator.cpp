/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "XPathGenerator.h"
#include "nsString.h"

TEST(TestXPathGenerator, TestQuoteArgumentWithoutQuote)
{
  nsAutoString arg;
  arg.AssignLiteral(u"testing");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(u"\'testing\'");

  nsAutoString result;
  XPathGenerator::QuoteArgument(arg, result);

  ASSERT_TRUE(expectedResult.Equals(result));
}

TEST(TestXPathGenerator, TestQuoteArgumentWithSingleQuote)
{
  nsAutoString arg;
  arg.AssignLiteral(u"\'testing\'");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(u"\"\'testing\'\"");

  nsAutoString result;
  XPathGenerator::QuoteArgument(arg, result);

  ASSERT_TRUE(expectedResult.Equals(result));
}

TEST(TestXPathGenerator, TestQuoteArgumentWithDoubleQuote)
{
  nsAutoString arg;
  arg.AssignLiteral(u"\"testing\"");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(u"\'\"testing\"\'");

  nsAutoString result;
  XPathGenerator::QuoteArgument(arg, result);

  ASSERT_TRUE(expectedResult.Equals(result));
}

TEST(TestXPathGenerator, TestQuoteArgumentWithSingleAndDoubleQuote)
{
  nsAutoString arg;
  arg.AssignLiteral(u"\'testing\"");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(u"concat(\'\',\"\'\",\'testing\"\')");

  nsAutoString result;
  XPathGenerator::QuoteArgument(arg, result);
  printf("Result: %s\nExpected: %s\n", NS_ConvertUTF16toUTF8(result).get(),
         NS_ConvertUTF16toUTF8(expectedResult).get());

  ASSERT_TRUE(expectedResult.Equals(result));
}

TEST(TestXPathGenerator,
     TestQuoteArgumentWithDoubleQuoteAndASequenceOfSingleQuote)
{
  nsAutoString arg;
  arg.AssignLiteral(u"\'\'\'\'testing\"");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(u"concat(\'\',\"\'\'\'\'\",\'testing\"\')");

  nsAutoString result;
  XPathGenerator::QuoteArgument(arg, result);
  printf("Result: %s\nExpected: %s\n", NS_ConvertUTF16toUTF8(result).get(),
         NS_ConvertUTF16toUTF8(expectedResult).get());

  ASSERT_TRUE(expectedResult.Equals(result));
}

TEST(TestXPathGenerator,
     TestQuoteArgumentWithDoubleQuoteAndTwoSequencesOfSingleQuote)
{
  nsAutoString arg;
  arg.AssignLiteral(u"\'\'\'\'testing\'\'\'\'\'\'\"");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(
      u"concat(\'\',\"\'\'\'\'\",\'testing\',\"\'\'\'\'\'\'\",\'\"\')");

  nsAutoString result;
  XPathGenerator::QuoteArgument(arg, result);
  printf("Result: %s\nExpected: %s\n", NS_ConvertUTF16toUTF8(result).get(),
         NS_ConvertUTF16toUTF8(expectedResult).get());

  ASSERT_TRUE(expectedResult.Equals(result));
}

TEST(TestXPathGenerator,
     TestQuoteArgumentWithDoubleQuoteAndTwoSequencesOfSingleQuoteInMiddle)
{
  nsAutoString arg;
  arg.AssignLiteral(u"t\'\'\'\'estin\'\'\'\'\'\'\"g");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(
      u"concat(\'t\',\"\'\'\'\'\",\'estin\',\"\'\'\'\'\'\'\",\'\"g\')");

  nsAutoString result;
  XPathGenerator::QuoteArgument(arg, result);
  printf("Result: %s\nExpected: %s\n", NS_ConvertUTF16toUTF8(result).get(),
         NS_ConvertUTF16toUTF8(expectedResult).get());

  ASSERT_TRUE(expectedResult.Equals(result));
}

TEST(TestXPathGenerator, TestEscapeNameWithNormalCharacters)
{
  nsAutoString arg;
  arg.AssignLiteral(u"testing");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(u"testing");

  nsAutoString result;
  XPathGenerator::EscapeName(arg, result);

  ASSERT_TRUE(expectedResult.Equals(result));
}

TEST(TestXPathGenerator, TestEscapeNameWithSpecialCharacters)
{
  nsAutoString arg;
  arg.AssignLiteral(u"^testing!");

  nsAutoString expectedResult;
  expectedResult.AssignLiteral(u"*[local-name()=\'^testing!\']");

  nsAutoString result;
  XPathGenerator::EscapeName(arg, result);
  printf("Result: %s\nExpected: %s\n", NS_ConvertUTF16toUTF8(result).get(),
         NS_ConvertUTF16toUTF8(expectedResult).get());

  ASSERT_TRUE(expectedResult.Equals(result));
}
