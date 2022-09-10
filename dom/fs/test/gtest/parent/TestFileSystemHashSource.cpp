/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemHashSource.h"
#include "TestHelpers.h"
#include "gtest/gtest.h"
#include "mozilla/Array.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "nsContentUtils.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTHashSet.h"

namespace mozilla::dom::fs::test {

using mozilla::dom::fs::data::FileSystemHashSource;

namespace {

constexpr size_t sha256ByteLength = 32u;

constexpr size_t kExpectedLength = 52u;

std::wstring asWide(const nsString& aStr) {
  std::wstring result;
  result.reserve(aStr.Length());
  for (const auto* it = aStr.BeginReading(); it != aStr.EndReading(); ++it) {
    result.push_back(static_cast<wchar_t>(*it));
  }
  return result;
}

}  // namespace

TEST(TestFileSystemHashSource, isHashLengthAsExpected)
{
  EntryId parent = "a"_ns;
  Name name = u"b"_ns;
  TEST_TRY_UNWRAP(EntryId result,
                  FileSystemHashSource::GenerateHash(parent, name));
  ASSERT_EQ(sha256ByteLength, result.Length());
};

TEST(TestFileSystemHashSource, areNestedNameHashesValidAndUnequal)
{
  EntryId emptyParent = ""_ns;
  Name name = u"a"_ns;
  const size_t nestingNumber = 500u;

  nsTHashSet<EntryId> results;
  nsTHashSet<Name> names;

  auto previousParent = emptyParent;
  for (size_t i = 0; i < nestingNumber; ++i) {
    TEST_TRY_UNWRAP(EntryId result,
                    FileSystemHashSource::GenerateHash(previousParent, name));

    TEST_TRY_UNWRAP(Name encoded, FileSystemHashSource::EncodeHash(result));

    // Validity checks
    ASSERT_TRUE(mozilla::IsAscii(encoded))
    << encoded;
    Name upperCaseVersion;
    nsContentUtils::ASCIIToUpper(encoded, upperCaseVersion);
    ASSERT_STREQ(asWide(upperCaseVersion).c_str(), asWide(encoded).c_str());

    // Is the same hash encountered?
    ASSERT_FALSE(results.Contains(result));
    ASSERT_TRUE(results.Insert(result, mozilla::fallible));

    // Is the same name encountered?
    ASSERT_FALSE(names.Contains(encoded));
    ASSERT_TRUE(names.Insert(encoded, mozilla::fallible));

    previousParent = result;
  }
};

TEST(TestFileSystemHashSource, areNameCombinationHashesUnequal)
{
  EntryId emptyParent = ""_ns;

  mozilla::Array<Name, 2> inputs = {u"a"_ns, u"b"_ns};
  nsTArray<EntryId> results;
  nsTArray<Name> names;

  for (const auto& name : inputs) {
    TEST_TRY_UNWRAP(EntryId result,
                    FileSystemHashSource::GenerateHash(emptyParent, name));
    TEST_TRY_UNWRAP(Name encoded, FileSystemHashSource::EncodeHash(result));

    // Validity checks
    ASSERT_TRUE(mozilla::IsAscii(encoded))
    << encoded;
    Name upperCaseVersion;
    nsContentUtils::ASCIIToUpper(encoded, upperCaseVersion);
    ASSERT_STREQ(asWide(upperCaseVersion).c_str(), asWide(encoded).c_str());

    results.AppendElement(result);
    names.AppendElement(encoded);
  }

  nsTArray<EntryId> more_results;
  nsTArray<Name> more_names;
  for (const auto& parent : results) {
    for (const auto& name : inputs) {
      TEST_TRY_UNWRAP(EntryId result,
                      FileSystemHashSource::GenerateHash(parent, name));
      TEST_TRY_UNWRAP(Name encoded, FileSystemHashSource::EncodeHash(result));

      // Validity checks
      ASSERT_TRUE(mozilla::IsAscii(encoded))
      << encoded;
      Name upperCaseVersion;
      nsContentUtils::ASCIIToUpper(encoded, upperCaseVersion);
      ASSERT_STREQ(asWide(upperCaseVersion).c_str(), asWide(encoded).c_str());

      more_results.AppendElement(result);
      more_names.AppendElement(encoded);
    }
  }

  results.AppendElements(more_results);
  names.AppendElements(more_names);

  // Is the same hash encountered?
  for (size_t i = 0; i < results.Length(); ++i) {
    for (size_t j = i + 1; j < results.Length(); ++j) {
      ASSERT_STRNE(results[i].get(), results[j].get());
    }
  }

  // Is the same name encountered?
  for (size_t i = 0; i < names.Length(); ++i) {
    for (size_t j = i + 1; j < names.Length(); ++j) {
      ASSERT_STRNE(asWide(names[i]).c_str(), asWide(names[j]).c_str());
    }
  }
};

TEST(TestFileSystemHashSource, encodeGeneratedHash)
{
  Name expected = u"HF6FOFV72G3NMDEJKYMVRIFJO4X5ZNZCF2GM7Q4Y5Q3E7NPQKSLA"_ns;
  ASSERT_EQ(kExpectedLength, expected.Length());

  EntryId parent = "a"_ns;
  Name name = u"b"_ns;
  TEST_TRY_UNWRAP(EntryId entry,
                  FileSystemHashSource::GenerateHash(parent, name));
  ASSERT_EQ(sha256ByteLength, entry.Length());

  TEST_TRY_UNWRAP(Name result, FileSystemHashSource::EncodeHash(entry));
  ASSERT_EQ(kExpectedLength, result.Length());
  ASSERT_STREQ(asWide(expected).c_str(), asWide(result).c_str());

  // Generate further hashes
  TEST_TRY_UNWRAP(entry, FileSystemHashSource::GenerateHash(entry, result));
  ASSERT_EQ(sha256ByteLength, entry.Length());

  TEST_TRY_UNWRAP(result, FileSystemHashSource::EncodeHash(entry));

  // Always the same length
  ASSERT_EQ(kExpectedLength, result.Length());

  // Encoded versions should differ
  ASSERT_STRNE(asWide(expected).c_str(), asWide(result).c_str());

  // Padding length should have been stripped
  char16_t padding = u"="_ns[0];
  const int32_t paddingStart = result.FindChar(padding);
  ASSERT_EQ(-1, paddingStart);
};

}  // namespace mozilla::dom::fs::test
