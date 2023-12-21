/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "MimeType.h"
#include "nsString.h"

using mozilla::UniquePtr;

TEST(MimeType, EmptyString)
{
  const auto in = u""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Empty string";
}

TEST(MimeType, JustWhitespace)
{
  const auto in = u" \t\r\n "_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Just whitespace";
}

TEST(MimeType, JustBackslash)
{
  const auto in = u"\\"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Just backslash";
}

TEST(MimeType, JustForwardslash)
{
  const auto in = u"/"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Just forward slash";
}

TEST(MimeType, MissingType1)
{
  const auto in = u"/bogus"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Missing type #1";
}

TEST(MimeType, MissingType2)
{
  const auto in = u" \r\n\t/bogus"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Missing type #2";
}

TEST(MimeType, MissingSubtype1)
{
  const auto in = u"bogus"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Missing subtype #1";
}

TEST(MimeType, MissingSubType2)
{
  const auto in = u"bogus/"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Missing subtype #2";
}

TEST(MimeType, MissingSubType3)
{
  const auto in = u"bogus;"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Missing subtype #3";
}

TEST(MimeType, MissingSubType4)
{
  const auto in = u"bogus; \r\n\t"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Missing subtype #3";
}

TEST(MimeType, ExtraForwardSlash)
{
  const auto in = u"bogus/bogus/;"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Extra forward slash";
}

TEST(MimeType, WhitespaceInType)
{
  const auto in = u"t\re\nx\tt /html"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Type with whitespace";
}

TEST(MimeType, WhitespaceInSubtype)
{
  const auto in = u"text/ h\rt\nm\tl"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Subtype with whitespace";
}

TEST(MimeType, NonAlphanumericMediaType1)
{
  const auto in = u"</>"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Non-alphanumeric media type #1";
}

TEST(MimeType, NonAlphanumericMediaType2)
{
  const auto in = u"(/)"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Non-alphanumeric media type #2";
}

TEST(MimeType, NonAlphanumericMediaType3)
{
  const auto in = u"{/}"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Non-alphanumeric media type #3";
}

TEST(MimeType, NonAlphanumericMediaType4)
{
  const auto in = u"\"/\""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Non-alphanumeric media type #4";
}

TEST(MimeType, NonAlphanumericMediaType5)
{
  const auto in = u"\0/\0"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Non-alphanumeric media type #5";
}

TEST(MimeType, NonAlphanumericMediaType6)
{
  const auto in = u"text/html(;doesnot=matter"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Non-alphanumeric media type #6";
}

TEST(MimeType, NonLatin1MediaType1)
{
  const auto in = u"ÿ/ÿ"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Non-latin1 media type #1";
}

TEST(MimeType, NonLatin1MediaType2)
{
  const auto in = u"\x0100/\x0100"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_FALSE(parsed)
  << "Non-latin1 media type #2";
}

TEST(MimeType, MultipleParameters)
{
  const auto in = u"text/html;charset=gbk;no=1;charset_=gbk_;yes=2"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.Equals(u"text/html;charset=gbk;no=1;charset_=gbk_;yes=2"_ns))
  << "Multiple parameters";
}

TEST(MimeType, DuplicateParameter1)
{
  const auto in = u"text/html;charset=gbk;charset=windows-1255"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.Equals(u"text/html;charset=gbk"_ns))
  << "Duplicate parameter #1";
}

TEST(MimeType, DuplicateParameter2)
{
  const auto in = u"text/html;charset=();charset=GBK"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.Equals(u"text/html;charset=\"()\""_ns))
  << "Duplicate parameter #2";
}

TEST(MimeType, CString)
{
  const auto in = "text/html;charset=();charset=GBK"_ns;
  UniquePtr<CMimeType> parsed = CMimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsCString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.Equals("text/html;charset=\"()\""_ns))
  << "Duplicate parameter #2";
}

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4819)
#endif
TEST(MimeType, NonAlphanumericParametersAreQuoted)
{
  const auto in = u"text/html;test=\x00FF\\;charset=gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.Equals(u"text/html;test=\"\x00FF\\\\\";charset=gbk"_ns))
  << "Non-alphanumeric parameters are quoted";
}
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

TEST(MimeType, ParameterQuotedIfHasLeadingWhitespace1)
{
  const auto in = u"text/html;charset= g\\\"bk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\" g\\\\\\\"bk\""))
  << "Parameter is quoted if has leading whitespace #1";
}

TEST(MimeType, ParameterQuotedIfHasLeadingWhitespace2)
{
  const auto in = u"text/html;charset= \"g\\bk\""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\" \\\"g\\\\bk\\\"\""))
  << "Parameter is quoted if has leading whitespace #2";
}

TEST(MimeType, ParameterQuotedIfHasInternalWhitespace)
{
  const auto in = u"text/html;charset=g \\b\"k"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\"g \\\\b\\\"k\""))
  << "Parameter is quoted if has internal whitespace";
}

TEST(MimeType, ImproperlyQuotedParameter1)
{
  const auto in = u"x/x;test=\""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("x/x;test=\"\""))
  << "Improperly-quoted parameter is handled properly #1";
}

TEST(MimeType, ImproperlyQuotedParameter2)
{
  const auto in = u"x/x;test=\"\\"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("x/x;test=\"\\\\\""))
  << "Improperly-quoted parameter is handled properly #2";
}

TEST(MimeType, NonLatin1ParameterIgnored)
{
  const auto in = u"x/x;test=\xFFFD;x=x"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("x/x;x=x"))
  << "Non latin-1 parameters are ignored";
}

TEST(MimeType, ParameterIgnoredIfWhitespaceInName1)
{
  const auto in = u"text/html;charset =gbk;charset=123"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=123"))
  << "Parameter ignored if whitespace in name #1";
}

TEST(MimeType, ParameterIgnoredIfWhitespaceInName2)
{
  const auto in = u"text/html;cha rset =gbk;charset=123"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=123"))
  << "Parameter ignored if whitespace in name #2";
}

TEST(MimeType, WhitespaceTrimmed)
{
  const auto in = u"\n\r\t  text/plain\n\r\t  ;\n\r\t  charset=123\n\r\t "_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/plain;charset=123"))
  << "Whitespace appropriately ignored";
}

TEST(MimeType, WhitespaceOnlyParameterIgnored)
{
  const auto in = u"x/x;x= \r\n\t"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("x/x"))
  << "Whitespace-only parameter is ignored";
}

TEST(MimeType, IncompleteParameterIgnored1)
{
  const auto in = u"x/x;test"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("x/x"))
  << "Incomplete parameter is ignored #1";
}

TEST(MimeType, IncompleteParameterIgnored2)
{
  const auto in = u"x/x;test="_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("x/x"))
  << "Incomplete parameter is ignored #2";
}

TEST(MimeType, IncompleteParameterIgnored3)
{
  const auto in = u"x/x;test= \r\n\t"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("x/x"))
  << "Incomplete parameter is ignored #3";
}

TEST(MimeType, IncompleteParameterIgnored4)
{
  const auto in = u"text/html;test;charset=gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Incomplete parameter is ignored #4";
}

TEST(MimeType, IncompleteParameterIgnored5)
{
  const auto in = u"text/html;test=;charset=gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Incomplete parameter is ignored #5";
}

TEST(MimeType, EmptyParameterIgnored1)
{
  const auto in = u"text/html ; ; charset=gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Empty parameter ignored #1";
}

TEST(MimeType, EmptyParameterIgnored2)
{
  const auto in = u"text/html;;;;charset=gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Empty parameter ignored #2";
}

TEST(MimeType, InvalidParameterIgnored1)
{
  const auto in = u"text/html;';charset=gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Invalid parameter ignored #1";
}

TEST(MimeType, InvalidParameterIgnored2)
{
  const auto in = u"text/html;\";charset=gbk;=123; =321"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Invalid parameter ignored #2";
}

TEST(MimeType, InvalidParameterIgnored3)
{
  const auto in = u"text/html;charset= \"\u007F;charset=GBK"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=GBK"))
  << "Invalid parameter ignored #3";
}

TEST(MimeType, InvalidParameterIgnored4)
{
  const auto in = nsLiteralString(
      u"text/html;charset=\"\u007F;charset=foo\";charset=GBK;charset=");
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=GBK"))
  << "Invalid parameter ignored #4";
}

TEST(MimeType, SingleQuotes1)
{
  const auto in = u"text/html;charset='gbk'"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset='gbk'"))
  << "Single quotes handled properly #1";
}

TEST(MimeType, SingleQuotes2)
{
  const auto in = u"text/html;charset='gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset='gbk"))
  << "Single quotes handled properly #2";
}

TEST(MimeType, SingleQuotes3)
{
  const auto in = u"text/html;charset=gbk'"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk'"))
  << "Single quotes handled properly #3";
}

TEST(MimeType, SingleQuotes4)
{
  const auto in = u"text/html;charset=';charset=GBK"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset='"))
  << "Single quotes handled properly #4";
}

TEST(MimeType, SingleQuotes5)
{
  const auto in = u"text/html;charset=''';charset=GBK"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset='''"))
  << "Single quotes handled properly #5";
}

TEST(MimeType, DoubleQuotes1)
{
  const auto in = u"text/html;charset=\"gbk\""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Double quotes handled properly #1";
}

TEST(MimeType, DoubleQuotes2)
{
  const auto in = u"text/html;charset=\"gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Double quotes handled properly #2";
}

TEST(MimeType, DoubleQuotes3)
{
  const auto in = u"text/html;charset=gbk\""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\"gbk\\\"\""))
  << "Double quotes handled properly #3";
}

TEST(MimeType, DoubleQuotes4)
{
  const auto in = u"text/html;charset=\" gbk\""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\" gbk\""))
  << "Double quotes handled properly #4";
}

TEST(MimeType, DoubleQuotes5)
{
  const auto in = u"text/html;charset=\"gbk \""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\"gbk \""))
  << "Double quotes handled properly #5";
}

TEST(MimeType, DoubleQuotes6)
{
  const auto in = u"text/html;charset=\"\\ gbk\""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\" gbk\""))
  << "Double quotes handled properly #6";
}

TEST(MimeType, DoubleQuotes7)
{
  const auto in = u"text/html;charset=\"\\g\\b\\k\""_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Double quotes handled properly #7";
}

TEST(MimeType, DoubleQuotes8)
{
  const auto in = u"text/html;charset=\"gbk\"x"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=gbk"))
  << "Double quotes handled properly #8";
}

TEST(MimeType, DoubleQuotes9)
{
  const auto in = u"text/html;charset=\"\";charset=GBK"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\"\""))
  << "Double quotes handled properly #9";
}

TEST(MimeType, DoubleQuotes10)
{
  const auto in = u"text/html;charset=\";charset=GBK"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\";charset=GBK\""))
  << "Double quotes handled properly #10";
}

TEST(MimeType, UnexpectedCodePoints)
{
  const auto in = u"text/html;charset={gbk}"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\"{gbk}\""))
  << "Unexpected code points handled properly";
}

TEST(MimeType, LongTypesSubtypesAccepted)
{
  const auto in = nsLiteralString(
      u"01234567890123456789012345678901234567890123456789012345678901234567890"
      u"1"
      "2345678901234567890123456789012345678901234567890123456789/"
      "012345678901234567890123456789012345678901234567890123456789012345678901"
      "2345678901234567890123456789012345678901234567890123456789");
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.Equals(in))
  << "Long type/subtype accepted";
}

TEST(MimeType, LongParametersAccepted)
{
  const auto in = nsLiteralString(
      u"text/"
      "html;"
      "012345678901234567890123456789012345678901234567890123456789012345678901"
      "2345678901234567890123456789012345678901234567890123456789=x;charset="
      "gbk");
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.Equals(in))
  << "Long parameters accepted";
}

TEST(MimeType, AllValidCharactersAccepted1)
{
  const auto in = nsLiteralString(
      u"x/x;x=\"\t "
      u"!\\\"#$%&'()*+,-./"
      u"0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]^_`"
      u"abcdefghijklmnopqrstuvwxyz{|}~"
      u"\u0080\u0081\u0082\u0083\u0084\u0085\u0086\u0087\u0088\u0089\u008A"
      u"\u008B\u008C\u008D\u008E\u008F\u0090\u0091\u0092\u0093\u0094\u0095"
      u"\u0096\u0097\u0098\u0099\u009A\u009B\u009C\u009D\u009E\u009F\u00A0"
      u"\u00A1\u00A2\u00A3\u00A4\u00A5\u00A6\u00A7\u00A8\u00A9\u00AA\u00AB"
      u"\u00AC\u00AD\u00AE\u00AF\u00B0\u00B1\u00B2\u00B3\u00B4\u00B5\u00B6"
      u"\u00B7\u00B8\u00B9\u00BA\u00BB\u00BC\u00BD\u00BE\u00BF\u00C0\u00C1"
      u"\u00C2\u00C3\u00C4\u00C5\u00C6\u00C7\u00C8\u00C9\u00CA\u00CB\u00CC"
      u"\u00CD\u00CE\u00CF\u00D0\u00D1\u00D2\u00D3\u00D4\u00D5\u00D6\u00D7"
      u"\u00D8\u00D9\u00DA\u00DB\u00DC\u00DD\u00DE\u00DF\u00E0\u00E1\u00E2"
      u"\u00E3\u00E4\u00E5\u00E6\u00E7\u00E8\u00E9\u00EA\u00EB\u00EC\u00ED"
      u"\u00EE\u00EF\u00F0\u00F1\u00F2\u00F3\u00F4\u00F5\u00F6\u00F7\u00F8"
      u"\u00F9\u00FA\u00FB\u00FC\u00FD\u00FE\u00FF\"");
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.Equals(in))
  << "All valid characters accepted #1";
}

TEST(MimeType, CaseNormalization1)
{
  const auto in = u"TEXT/PLAIN;CHARSET=TEST"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/plain;charset=TEST"))
  << "Case normalized properly #1";
}

TEST(MimeType, CaseNormalization2)
{
  const auto in = nsLiteralString(
      u"!#$%&'*+-.^_`|~"
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz/"
      "!#$%&'*+-.^_`|~"
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz;!#$%&'*+-"
      ".^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz=!#$"
      "%&'*+-.^_`|~"
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral(
      "!#$%&'*+-.^_`|~"
      "0123456789abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz/"
      "!#$%&'*+-.^_`|~"
      "0123456789abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz;!#$%&'*+-"
      ".^_`|~0123456789abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz=!#$"
      "%&'*+-.^_`|~"
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"))
  << "Case normalized properly #2";
}

TEST(MimeType, LegacyCommentSyntax1)
{
  const auto in = u"text/html;charset=gbk("_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;charset=\"gbk(\""))
  << "Legacy comment syntax #1";
}

TEST(MimeType, LegacyCommentSyntax2)
{
  const auto in = u"text/html;x=(;charset=gbk"_ns;
  UniquePtr<MimeType> parsed = MimeType::Parse(in);
  ASSERT_TRUE(parsed)
  << "Parsing succeeded";
  nsAutoString out;
  parsed->Serialize(out);
  ASSERT_TRUE(out.EqualsLiteral("text/html;x=\"(\";charset=gbk"))
  << "Legacy comment syntax #2";
}

TEST(MimeTypeParsing, contentTypes1)
{
  const nsAutoCString val(",text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_FALSE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral(""));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes2)
{
  const nsAutoCString val("text/plain,");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/plain"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes3)
{
  const nsAutoCString val("text/html,text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/plain"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes4)
{
  const nsAutoCString val("text/plain;charset=gbk,text/html");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes5)
{
  const nsAutoCString val(
      "text/plain;charset=gbk,text/html;charset=windows-1254");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral("windows-1254"));
}

TEST(MimeTypeParsing, contentTypes6)
{
  const nsAutoCString val("text/plain;charset=gbk,text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/plain"));
  ASSERT_TRUE(contentCharset.EqualsLiteral("gbk"));
}

TEST(MimeTypeParsing, contentTypes7)
{
  const nsAutoCString val(
      "text/plain;charset=gbk,text/plain;charset=windows-1252");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/plain"));
  ASSERT_TRUE(contentCharset.EqualsLiteral("windows-1252"));
}

TEST(MimeTypeParsing, contentTypes8)
{
  const nsAutoCString val("text/html;charset=gbk,text/html;x=\",text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral("gbk"));
}

TEST(MimeTypeParsing, contentTypes9)
{
  const nsAutoCString val("text/plain;charset=gbk;x=foo,text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/plain"));
  ASSERT_TRUE(contentCharset.EqualsLiteral("gbk"));
}

TEST(MimeTypeParsing, contentTypes10)
{
  const nsAutoCString val("text/html;charset=gbk,text/plain,text/html");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes11)
{
  const nsAutoCString val("text/plain,*/*");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/plain"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes12)
{
  const nsAutoCString val("text/html,*/*");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes13)
{
  const nsAutoCString val("*/*,text/html");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes14)
{
  const nsAutoCString val("text/plain,*/*;charset=gbk");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/plain"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes15)
{
  const nsAutoCString val("text/html,*/*;charset=gbk");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes16)
{
  const nsAutoCString val("text/html;x=\",text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes17)
{
  const nsAutoCString val("text/html;\",text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes18)
{
  const nsAutoCString val("text/html;\",\\\",text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}

TEST(MimeTypeParsing, contentTypes19)
{
  const nsAutoCString val("text/html;\",\\\",text/plain,\";charset=GBK");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/html"));
  ASSERT_TRUE(contentCharset.EqualsLiteral("GBK"));
}

TEST(MimeTypeParsing, contentTypes20)
{
  const nsAutoCString val("text/html;\",\",text/plain");
  nsCString contentType;
  nsCString contentCharset;

  bool parsed = CMimeType::Parse(val, contentType, contentCharset);

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(contentType.EqualsLiteral("text/plain"));
  ASSERT_TRUE(contentCharset.EqualsLiteral(""));
}
