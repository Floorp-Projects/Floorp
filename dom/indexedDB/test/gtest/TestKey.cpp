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
using JS::Rooted;

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

static JSObject* ExpectArrayBufferObject(const JS::Value& aValue) {
  EXPECT_TRUE(aValue.isObject());
  auto& object = aValue.toObject();
  EXPECT_TRUE(JS::IsArrayBufferObject(&object));
  return &object;
}

static JSObject* ExpectArrayObject(JSContext* const aContext,
                                   JS::Handle<JS::Value> aValue) {
  EXPECT_TRUE(aValue.isObject());
  bool rv;
  EXPECT_TRUE(JS::IsArrayObject(aContext, aValue, &rv));
  EXPECT_TRUE(rv);
  return &aValue.toObject();
}

static void CheckArrayBuffer(const nsCString& aExpected,
                             const JS::Value& aActual) {
  auto obj = ExpectArrayBufferObject(aActual);
  uint32_t length;
  bool isSharedMemory;
  uint8_t* data;
  JS::GetArrayBufferLengthAndData(obj, &length, &isSharedMemory, &data);

  EXPECT_EQ(aExpected.Length(), length);
  EXPECT_EQ(0, memcmp(aExpected.get(), data, length));
}

static void CheckString(JSContext* const aContext, const nsString& aExpected,
                        JS::Handle<JS::Value> aActual) {
  EXPECT_TRUE(aActual.isString());
  int32_t rv;
  EXPECT_TRUE(JS_CompareStrings(aContext,
                                JS_NewUCStringCopyZ(aContext, aExpected.get()),
                                aActual.toString(), &rv));
  EXPECT_EQ(0, rv);
}

namespace {
// This is modeled after dom/base/test/gtest/TestContentUtils.cpp
struct AutoTestJSContext {
  AutoTestJSContext()
      : mGlobalObject(
            mozilla::dom::RootingCx(),
            mozilla::dom::SimpleGlobalObject::Create(
                mozilla::dom::SimpleGlobalObject::GlobalType::BindingDetail)) {
    EXPECT_TRUE(mJsAPI.Init(mGlobalObject));
    mContext = mJsAPI.cx();
  }

  operator JSContext*() const { return mContext; }

 private:
  Rooted<JSObject*> mGlobalObject;
  mozilla::dom::AutoJSAPI mJsAPI;
  JSContext* mContext;
};

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

TEST_P(TestWithParam_CString_ArrayBuffer_Pair, Ctor_EncodedBinary) {
  const auto key = Key{GetParam().first};

  ExpectKeyIsBinary(key);

  AutoTestJSContext context;

  Rooted<JS::Value> rv(context);
  EXPECT_EQ(NS_OK, key.ToJSVal(context, &rv));

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

static JS::Value CreateArrayBufferValue(JSContext* const aContext,
                                        const size_t aSize, char* const aData) {
  Rooted<JSObject*> arrayBuffer{
      aContext, JS::NewArrayBufferWithContents(aContext, aSize, aData)};
  EXPECT_TRUE(arrayBuffer);
  return JS::ObjectValue(*arrayBuffer);
}

// This tests calling SetFromJSVal with an ArrayBuffer scalar of length 0.
// TODO Probably there should be more test cases for SetFromJSVal with other
// ArrayBuffer scalars, which convert this into a parametrized test as well.
TEST(DOM_IndexedDB_Key, SetFromJSVal_ZeroLengthArrayBuffer)
{
  AutoTestJSContext context;

  auto key = Key{};
  Rooted<JS::Value> arrayBuffer(context,
                                CreateArrayBufferValue(context, 0, nullptr));
  auto rv1 = mozilla::ErrorResult{};
  const auto result = key.SetFromJSVal(context, arrayBuffer, rv1);
  EXPECT_FALSE(rv1.Failed());
  EXPECT_TRUE(result.Is(mozilla::dom::indexedDB::Ok, rv1));

  ExpectKeyIsBinary(key);

  Rooted<JS::Value> rv2(context);
  EXPECT_EQ(NS_OK, key.ToJSVal(context, &rv2));

  CheckArrayBuffer(NS_LITERAL_CSTRING(""), rv2);
}

template <typename CheckElement>
static void CheckArray(JSContext* const context,
                       JS::Handle<JS::Value> arrayValue,
                       const size_t expectedLength,
                       const CheckElement& checkElement) {
  Rooted<JSObject*> actualArray(context,
                                ExpectArrayObject(context, arrayValue));

  uint32_t actualLength;
  EXPECT_TRUE(JS::GetArrayLength(context, actualArray, &actualLength));
  EXPECT_EQ(expectedLength, actualLength);
  for (size_t i = 0; i < expectedLength; ++i) {
    Rooted<JS::Value> element(static_cast<JSContext*>(context));
    EXPECT_TRUE(JS_GetElement(context, actualArray, i, &element));

    checkElement(i, element);
  }
}

static JS::Value CreateArrayBufferArray(
    JSContext* const context, const std::vector<nsCString>& elements) {
  Rooted<JSObject*> arrayObject(context,
                                JS::NewArrayObject(context, elements.size()));
  EXPECT_TRUE(arrayObject);

  Rooted<JS::Value> arrayBuffer(context);
  for (size_t i = 0; i < elements.size(); ++i) {
    // TODO strdup only works if the element is actually 0-terminated
    arrayBuffer = CreateArrayBufferValue(
        context, elements[i].Length(),
        elements[i].Length() ? strdup(elements[i].get()) : nullptr);
    EXPECT_TRUE(JS_SetElement(context, arrayObject, i, arrayBuffer));
  }

  return JS::ObjectValue(*arrayObject);
}

TEST_P(TestWithParam_ArrayBufferArray, SetFromJSVal) {
  const auto& elements = GetParam();

  AutoTestJSContext context;
  Rooted<JS::Value> arrayValue(context);
  arrayValue = CreateArrayBufferArray(context, elements);

  auto rv1 = mozilla::ErrorResult{};
  auto key = Key{};
  const auto result = key.SetFromJSVal(context, arrayValue, rv1);
  EXPECT_FALSE(rv1.Failed());
  EXPECT_TRUE(result.Is(mozilla::dom::indexedDB::Ok, rv1));

  ExpectKeyIsArray(key);

  Rooted<JS::Value> rv2(context);
  EXPECT_EQ(NS_OK, key.ToJSVal(context, &rv2));

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

static JS::Value CreateStringValue(JSContext* const context,
                                   const nsString& string) {
  JSString* str = JS_NewUCStringCopyZ(context, string.get());
  EXPECT_TRUE(str);
  return JS::StringValue(str);
}

static JS::Value CreateStringArray(JSContext* const context,
                                   const std::vector<nsString>& elements) {
  Rooted<JSObject*> array(context,
                          JS::NewArrayObject(context, elements.size()));
  EXPECT_TRUE(array);

  for (size_t i = 0; i < elements.size(); ++i) {
    Rooted<JS::Value> string(context, CreateStringValue(context, elements[i]));
    EXPECT_TRUE(JS_SetElement(context, array, i, string));
  }

  return JS::ObjectValue(*array);
}

TEST_P(TestWithParam_StringArray, SetFromJSVal) {
  const auto& elements = GetParam();

  AutoTestJSContext context;
  Rooted<JS::Value> arrayValue(context, CreateStringArray(context, elements));

  auto rv1 = mozilla::ErrorResult{};
  auto key = Key{};
  const auto result = key.SetFromJSVal(context, arrayValue, rv1);
  EXPECT_FALSE(rv1.Failed());
  EXPECT_TRUE(result.Is(mozilla::dom::indexedDB::Ok, rv1));

  ExpectKeyIsArray(key);

  Rooted<JS::Value> rv2(context);
  EXPECT_EQ(NS_OK, key.ToJSVal(context, &rv2));

  CheckArray(
      context, rv2, elements.size(),
      [&elements, &context](const size_t i, JS::Handle<JS::Value> element) {
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
  Rooted<JS::Value> arrayBuffer1(
      context, CreateArrayBufferValue(context, sizeof buf, strdup(buf)));
  const auto result1 = first.SetFromJSVal(context, arrayBuffer1, rv1);
  EXPECT_FALSE(rv1.Failed());
  EXPECT_TRUE(result1.Is(mozilla::dom::indexedDB::Ok, rv1));

  auto second = Key{};
  auto rv2 = mozilla::ErrorResult{};
  Rooted<JS::Value> arrayBuffer2(
      context, CreateArrayBufferValue(context, sizeof buf, strdup(buf)));
  const auto result2 = second.SetFromJSVal(context, arrayBuffer2, rv2);
  EXPECT_FALSE(rv2.Failed());
  EXPECT_TRUE(result2.Is(mozilla::dom::indexedDB::Ok, rv2));

  EXPECT_EQ(0, Key::CompareKeys(first, second));
}
