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
#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject, JS::NewArrayObject
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

#if 0
static void ExpectKeyIsBinary(const Key& aKey) {
  EXPECT_FALSE(aKey.IsUnset());

  EXPECT_FALSE(aKey.IsArray());
  EXPECT_TRUE(aKey.IsBinary());
  EXPECT_FALSE(aKey.IsDate());
  EXPECT_FALSE(aKey.IsFloat());
  EXPECT_FALSE(aKey.IsString());
}
#endif

static void ExpectKeyIsString(const Key& aKey) {
  EXPECT_FALSE(aKey.IsUnset());

  EXPECT_FALSE(aKey.IsArray());
  EXPECT_FALSE(aKey.IsBinary());
  EXPECT_FALSE(aKey.IsDate());
  EXPECT_FALSE(aKey.IsFloat());
  EXPECT_TRUE(aKey.IsString());
}

#if 0
static void ExpectKeyIsArray(const Key& aKey) {
  EXPECT_FALSE(aKey.IsUnset());

  EXPECT_TRUE(aKey.IsArray());
  EXPECT_FALSE(aKey.IsBinary());
  EXPECT_FALSE(aKey.IsDate());
  EXPECT_FALSE(aKey.IsFloat());
  EXPECT_FALSE(aKey.IsString());
}
#endif

#if 0
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
  EXPECT_TRUE(JS::IsArrayObject(aContext, value, &rv));
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
#endif

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

class TestWithParam_LiteralString
    : public ::testing::TestWithParam<nsLiteralString> {};

class TestWithParam_StringArray
    : public ::testing::TestWithParam<std::vector<nsString>> {};

class TestWithParam_ArrayBufferArray
    : public ::testing::TestWithParam<std::vector<nsCString>> {};

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

TEST_P(TestWithParam_LiteralString, SetFromString) {
  auto key = Key{};
  mozilla::ErrorResult error;
  const auto result = key.SetFromString(GetParam(), error);
  EXPECT_FALSE(error.Failed());
  EXPECT_TRUE(result.Is(mozilla::dom::indexedDB::Ok, error));

  ExpectKeyIsString(key);

  nsString rv;
  key.ToString(rv);

  EXPECT_EQ(GetParam(), rv);
}

INSTANTIATE_TEST_CASE_P(DOM_IndexedDB_Key, TestWithParam_LiteralString,
                        ::testing::Values(NS_LITERAL_STRING(""),
                                          NS_LITERAL_STRING(u"abc"),
                                          NS_LITERAL_STRING(u"\u007f"),
                                          NS_LITERAL_STRING(u"\u0080"),
                                          NS_LITERAL_STRING(u"\u1fff"),
                                          NS_LITERAL_STRING(u"\u7fff"),
                                          NS_LITERAL_STRING(u"\u8000"),
                                          NS_LITERAL_STRING(u"\uffff")));

#if 0
static JS::RootedValue CreateArrayBufferValue(JSContext* const aContext,
                                              const size_t aSize,
                                              char* const aData) {
  auto&& arrayBuffer = JS::RootedObject{
      aContext, JS::NewArrayBufferWithContents(aContext, aSize, aData)};
  return {aContext, JS::ObjectValue(*arrayBuffer)};
}

// This tests calling SetFromJSVal with an ArrayBuffer scalar of length 0.
// TODO Probably there should be more test cases for SetFromJSVal with other
// ArrayBuffer scalars, which convert this into a parametrized test as well.
TEST(DOM_IndexedDB_Key, SetFromJSVal_ZeroLengthArrayBuffer)
{
  AutoTestJSContext context;

  auto key = Key{};
  auto&& arrayBuffer = CreateArrayBufferValue(context, 0, nullptr);
  auto rv1 = mozilla::ErrorResult{};
  const auto result = key.SetFromJSVal(context, arrayBuffer, rv1);
  EXPECT_FALSE(rv1.Failed());
  EXPECT_TRUE(result.Is(mozilla::dom::indexedDB::Ok, rv1));

  ExpectKeyIsBinary(key);

  JS::Heap<JS::Value> rv2;
  EXPECT_EQ(NS_OK, key.ToJSVal(context, rv2));

  CheckArrayBuffer(NS_LITERAL_CSTRING(""), rv2);
}

template <typename CheckElement>
static void CheckArray(JSContext* const context,
                       const JS::Heap<JS::Value>& arrayValue,
                       const size_t expectedLength,
                       const CheckElement& checkElement) {
  auto&& actualArray =
      JS::RootedObject(context, &ExpectArrayObject(context, arrayValue));

  uint32_t actualLength;
  EXPECT_TRUE(JS::GetArrayLength(context, actualArray, &actualLength));
  EXPECT_EQ(expectedLength, actualLength);
  for (size_t i = 0; i < expectedLength; ++i) {
    JS::RootedValue element(static_cast<JSContext*>(context));
    EXPECT_TRUE(JS_GetElement(context, actualArray, i, &element));

    checkElement(i, element);
  }
}

static JS::RootedValue CreateArrayBufferArray(
    JSContext* const context, const std::vector<nsCString>& elements) {
  auto* const array = JS::NewArrayObject(context, elements.size());
  auto&& arrayObject = JS::RootedObject{context, array};

  for (size_t i = 0; i < elements.size(); ++i) {
    // TODO strdup only works if the element is actually 0-terminated
    auto&& arrayBuffer = CreateArrayBufferValue(
        context, elements[i].Length(),
        elements[i].Length() ? strdup(elements[i].get()) : nullptr);
    EXPECT_TRUE(JS_SetElement(context, arrayObject, i, arrayBuffer));
  }

  return {context, JS::ObjectValue(*array)};
}

TEST_P(TestWithParam_ArrayBufferArray, SetFromJSVal) {
  const auto& elements = GetParam();

  AutoTestJSContext context;
  auto&& arrayValue = CreateArrayBufferArray(context, elements);

  auto rv1 = mozilla::ErrorResult{};
  auto key = Key{};
  const auto result = key.SetFromJSVal(context, arrayValue, rv1);
  EXPECT_FALSE(rv1.Failed());
  EXPECT_TRUE(result.Is(mozilla::dom::indexedDB::Ok, rv1));

  ExpectKeyIsArray(key);

  JS::Heap<JS::Value> rv2;
  EXPECT_EQ(NS_OK, key.ToJSVal(context, rv2));

  CheckArray(context, rv2, elements.size(),
             [&elements](const size_t i, const JS::HandleValue& element) {
               CheckArrayBuffer(elements[i], element);
             });
}

const uint8_t element2[] = "foo";
INSTANTIATE_TEST_CASE_P(
    DOM_IndexedDB_Key, TestWithParam_ArrayBufferArray,
    testing::Values(std::vector<nsCString>{},
                    std::vector<nsCString>{NS_LITERAL_CSTRING("")},
                    std::vector<nsCString>{NS_LITERAL_CSTRING(""),
                                           BufferAsCString(element2)}));

static JS::RootedValue CreateStringValue(JSContext* const context,
                                         const nsString& string) {
  return {context, JS::StringValue(JS_NewUCStringCopyZ(context, string.get()))};
}

static JS::RootedValue CreateStringArray(
    JSContext* const context, const std::vector<nsString>& elements) {
  auto* const array = JS::NewArrayObject(context, elements.size());
  auto&& arrayObject = JS::RootedObject{context, array};

  for (size_t i = 0; i < elements.size(); ++i) {
    auto&& string = CreateStringValue(context, elements[i]);
    EXPECT_TRUE(JS_SetElement(context, arrayObject, i, string));
  }

  return {context, JS::ObjectValue(*array)};
}

TEST_P(TestWithParam_StringArray, SetFromJSVal) {
  const auto& elements = GetParam();

  AutoTestJSContext context;
  auto&& arrayValue = CreateStringArray(context, elements);

  auto rv1 = mozilla::ErrorResult{};
  auto key = Key{};
  const auto result = key.SetFromJSVal(context, arrayValue, rv1);
  EXPECT_FALSE(rv1.Failed());
  EXPECT_TRUE(result.Is(mozilla::dom::indexedDB::Ok, rv1));

  ExpectKeyIsArray(key);

  JS::Heap<JS::Value> rv2;
  EXPECT_EQ(NS_OK, key.ToJSVal(context, rv2));

  CheckArray(
      context, rv2, elements.size(),
      [&elements, &context](const size_t i, const JS::HandleValue& element) {
        CheckString(context, elements[i], element);
      });
}

INSTANTIATE_TEST_CASE_P(
    DOM_IndexedDB_Key, TestWithParam_StringArray,
    testing::Values(std::vector<nsString>{NS_LITERAL_STRING(""),
                                          NS_LITERAL_STRING("abc\u0080\u1fff")},
                    std::vector<nsString>{
                        NS_LITERAL_STRING("abc\u0080\u1fff"),
                        NS_LITERAL_STRING("abc\u0080\u1fff")}));

TEST(DOM_IndexedDB_Key, CompareKeys_NonZeroLengthArrayBuffer)
{
  AutoTestJSContext context;
  const char buf[] = "abc\x80";

  auto first = Key{};
  auto rv1 = mozilla::ErrorResult{};
  auto&& arrayBuffer1 =
      CreateArrayBufferValue(context, sizeof buf, strdup(buf));
  const auto result1 = first.SetFromJSVal(context, arrayBuffer1, rv1);
  EXPECT_FALSE(rv1.Failed());
  EXPECT_TRUE(result1.Is(mozilla::dom::indexedDB::Ok, rv1));

  auto second = Key{};
  auto rv2 = mozilla::ErrorResult{};
  auto&& arrayBuffer2 =
      CreateArrayBufferValue(context, sizeof buf, strdup(buf));
  const auto result2 = second.SetFromJSVal(context, arrayBuffer2, rv2);
  EXPECT_FALSE(rv2.Failed());
  EXPECT_TRUE(result2.Is(mozilla::dom::indexedDB::Ok, rv2));

  EXPECT_EQ(0, Key::CompareKeys(first, second));
}
#endif
