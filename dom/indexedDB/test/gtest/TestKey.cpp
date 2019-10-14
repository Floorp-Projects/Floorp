/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/indexedDB/Key.h"

#include "jsapi.h"
#include "js/ArrayBuffer.h"

// TODO: This PrintTo overload is defined in dom/media/gtest/TestGroupId.cpp.
// However, it is not used, probably because of
// https://stackoverflow.com/a/36941270
void PrintTo(const nsString& value, std::ostream* os);

using namespace mozilla;
using namespace mozilla::dom::indexedDB;

// DOM_IndexedDB_Key_Ctor tests test the construction of a Key, and check the
// properties of the constructed key with the const methods afterwards. The
// tested ctors include the default ctor, which constructs an unset key, and the
// ctors that accepts an encoded buffer, which is then decoded using the
// Key::To* method corresponding to its type.
//
// So far, only some cases are tested:
// - scalar binary
// -- empty
// -- with 1-byte encoded representation
// - scalar string
// -- empty
// -- with 1-byte encoded representation
//
// TODO More test cases should be added, including
// - empty (?)
// - scalar binary
// -- containing 0 byte(s)
// -- with 2-byte encoded representation
// - scalar string
// -- with 2-byte and 3-byte encoded representation
// - scalar number
// - scalar date
// - arrays, incl. nested arrays, with various combinations of contained types

TEST(DOM_IndexedDB_Key, Ctor_Default)
{
  auto key = Key{};

  EXPECT_TRUE(key.IsUnset());
}

// TODO does such a helper function already exist?
template <size_t N>
static auto BufferAsCString(const uint8_t (&aBuffer)[N]) {
  return nsCString{reinterpret_cast<const char*>(
                       static_cast<std::decay_t<const uint8_t[]>>(aBuffer)),
                   N};
}

static void ExpectKeyIsBinary(const Key& aKey) {
  EXPECT_FALSE(aKey.IsUnset());

  EXPECT_FALSE(aKey.IsArray());
  EXPECT_TRUE(aKey.IsBinary());
  EXPECT_FALSE(aKey.IsDate());
  EXPECT_FALSE(aKey.IsFloat());
  EXPECT_FALSE(aKey.IsString());
}

static void ExpectKeyIsString(const Key& aKey) {
  EXPECT_FALSE(aKey.IsUnset());

  EXPECT_FALSE(aKey.IsArray());
  EXPECT_FALSE(aKey.IsBinary());
  EXPECT_FALSE(aKey.IsDate());
  EXPECT_FALSE(aKey.IsFloat());
  EXPECT_TRUE(aKey.IsString());
}

static void ExpectKeyIsArray(const Key& aKey) {
  EXPECT_FALSE(aKey.IsUnset());

  EXPECT_TRUE(aKey.IsArray());
  EXPECT_FALSE(aKey.IsBinary());
  EXPECT_FALSE(aKey.IsDate());
  EXPECT_FALSE(aKey.IsFloat());
  EXPECT_FALSE(aKey.IsString());
}

static JSObject& ExpectArrayBufferObject(const JS::Value& aValue) {
  EXPECT_TRUE(aValue.isObject());
  auto& object = aValue.toObject();
  EXPECT_TRUE(JS::IsArrayBufferObject(&object));
  return object;
}

static JSObject& ExpectArrayObject(JSContext* const aContext,
                                   const JS::Heap<JS::Value>& aValue) {
  EXPECT_TRUE(aValue.get().isObject());
  auto&& value = JS::RootedValue(aContext, aValue.get());
  bool rv;
  EXPECT_TRUE(JS_IsArrayObject(aContext, value, &rv));
  EXPECT_TRUE(rv);
  return value.toObject();
}

static void CheckArrayBuffer(const nsCString& aExpected,
                             const JS::Value& aActual) {
  auto& obj = ExpectArrayBufferObject(aActual);
  uint32_t length;
  bool isSharedMemory;
  uint8_t* data;
  JS::GetArrayBufferLengthAndData(&obj, &length, &isSharedMemory, &data);

  EXPECT_EQ(aExpected.Length(), length);
  EXPECT_EQ(0, memcmp(aExpected.get(), data, length));
}

static void CheckString(JSContext* const aContext, const nsString& aExpected,
                        const JS::Value& aActual) {
  EXPECT_TRUE(aActual.isString());
  int32_t rv;
  EXPECT_TRUE(JS_CompareStrings(aContext,
                                JS_NewUCStringCopyZ(aContext, aExpected.get()),
                                aActual.toString(), &rv));
  EXPECT_EQ(0, rv);
}

namespace {
// TODO Using AutoTestJSContext causes rooting hazards. Disabling for now.
// See https://phabricator.services.mozilla.com/D38360#inline-272833
#if 0
// TODO cf. AutoJSContext, which doesn't work here. Should it?
// This is modeled after dom/base/test/gtest/TestContentUtils.cpp
struct AutoTestJSContext {
  AutoTestJSContext()
      : mGlobalObject{mozilla::dom::SimpleGlobalObject::Create(
            mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail)} {
    EXPECT_TRUE(mJsAPI.Init(mGlobalObject));
    mContext = mJsAPI.cx();
  }

  operator JSContext*() { return mContext; }

 private:
  JSObject* mGlobalObject;
  mozilla::dom::AutoJSAPI mJsAPI;
  JSContext* mContext;
};
#endif

// The following classes serve as base classes for the parametrized tests below.
// The name of each class reflects the parameter type.

class TestWithParam_CString_ArrayBuffer_Pair
    : public ::testing::TestWithParam<std::pair<nsCString, nsLiteralCString>> {
};

class TestWithParam_CString_String_Pair
    : public ::testing::TestWithParam<std::pair<nsCString, nsLiteralString>> {};

}  // namespace

#if 0
TEST_P(TestWithParam_CString_ArrayBuffer_Pair, Ctor_EncodedBinary) {
  const auto key = Key{GetParam().first};

  ExpectKeyIsBinary(key);

  AutoTestJSContext context;

  JS::Heap<JS::Value> rv;
  EXPECT_EQ(NS_OK, key.ToJSVal(context, rv));

  CheckArrayBuffer(GetParam().second, rv);
}

static const uint8_t zeroLengthBinaryEncodedBuffer[] = {Key::eBinary};
static const uint8_t nonZeroLengthBinaryEncodedBuffer[] = {Key::eBinary,
                                                           'a' + 1, 'b' + 1};
INSTANTIATE_TEST_CASE_P(
    DOM_IndexedDB_Key, TestWithParam_CString_ArrayBuffer_Pair,
    ::testing::Values(
        std::make_pair(BufferAsCString(zeroLengthBinaryEncodedBuffer),
                       NS_LITERAL_CSTRING("")),
        std::make_pair(BufferAsCString(nonZeroLengthBinaryEncodedBuffer),
                       NS_LITERAL_CSTRING("ab"))));
#endif

TEST_P(TestWithParam_CString_String_Pair, Ctor_EncodedString) {
  const auto key = Key{GetParam().first};

  ExpectKeyIsString(key);

  nsString rv;
  key.ToString(rv);

  EXPECT_EQ(GetParam().second, rv);
}

static const uint8_t zeroLengthStringEncodedBuffer[] = {Key::eString};
static const uint8_t nonZeroLengthStringEncodedBuffer[] = {Key::eString,
                                                           'a' + 1, 'b' + 1};

INSTANTIATE_TEST_CASE_P(
    DOM_IndexedDB_Key, TestWithParam_CString_String_Pair,
    ::testing::Values(
        std::make_pair(BufferAsCString(zeroLengthStringEncodedBuffer),
                       NS_LITERAL_STRING("")),
        std::make_pair(BufferAsCString(nonZeroLengthStringEncodedBuffer),
                       NS_LITERAL_STRING("ab"))));
