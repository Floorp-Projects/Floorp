// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/values.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

class ValuesTest: public testing::Test {
};

TEST(ValuesTest, Basic) {
  // Test basic dictionary getting/setting
  DictionaryValue settings;
  std::wstring homepage = L"http://google.com";
  ASSERT_FALSE(
    settings.GetString(L"global.homepage", &homepage));
  ASSERT_EQ(std::wstring(L"http://google.com"), homepage);

  ASSERT_FALSE(settings.Get(L"global", NULL));
  ASSERT_TRUE(settings.Set(L"global", Value::CreateBooleanValue(true)));
  ASSERT_TRUE(settings.Get(L"global", NULL));
  ASSERT_TRUE(settings.SetString(L"global.homepage", L"http://scurvy.com"));
  ASSERT_TRUE(settings.Get(L"global", NULL));
  homepage = L"http://google.com";
  ASSERT_TRUE(settings.GetString(L"global.homepage", &homepage));
  ASSERT_EQ(std::wstring(L"http://scurvy.com"), homepage);

  // Test storing a dictionary in a list.
  ListValue* toolbar_bookmarks;
  ASSERT_FALSE(
    settings.GetList(L"global.toolbar.bookmarks", &toolbar_bookmarks));

  toolbar_bookmarks = new ListValue;
  settings.Set(L"global.toolbar.bookmarks", toolbar_bookmarks);
  ASSERT_TRUE(
    settings.GetList(L"global.toolbar.bookmarks", &toolbar_bookmarks));

  DictionaryValue* new_bookmark = new DictionaryValue;
  new_bookmark->SetString(L"name", L"Froogle");
  new_bookmark->SetString(L"url", L"http://froogle.com");
  toolbar_bookmarks->Append(new_bookmark);

  ListValue* bookmark_list;
  ASSERT_TRUE(settings.GetList(L"global.toolbar.bookmarks", &bookmark_list));
  DictionaryValue* bookmark;
  ASSERT_EQ(1U, bookmark_list->GetSize());
  ASSERT_TRUE(bookmark_list->GetDictionary(0, &bookmark));
  std::wstring bookmark_name = L"Unnamed";
  ASSERT_TRUE(bookmark->GetString(L"name", &bookmark_name));
  ASSERT_EQ(std::wstring(L"Froogle"), bookmark_name);
  std::wstring bookmark_url;
  ASSERT_TRUE(bookmark->GetString(L"url", &bookmark_url));
  ASSERT_EQ(std::wstring(L"http://froogle.com"), bookmark_url);
}

TEST(ValuesTest, List) {
  scoped_ptr<ListValue> mixed_list(new ListValue());
  mixed_list->Set(0, Value::CreateBooleanValue(true));
  mixed_list->Set(1, Value::CreateIntegerValue(42));
  mixed_list->Set(2, Value::CreateRealValue(88.8));
  mixed_list->Set(3, Value::CreateStringValue("foo"));
  ASSERT_EQ(4u, mixed_list->GetSize());

  Value *value = NULL;
  bool bool_value = false;
  int int_value = 0;
  double double_value = 0.0;
  std::string string_value;

  ASSERT_FALSE(mixed_list->Get(4, &value));

  ASSERT_FALSE(mixed_list->GetInteger(0, &int_value));
  ASSERT_EQ(0, int_value);
  ASSERT_FALSE(mixed_list->GetReal(1, &double_value));
  ASSERT_EQ(0.0, double_value);
  ASSERT_FALSE(mixed_list->GetString(2, &string_value));
  ASSERT_EQ("", string_value);
  ASSERT_FALSE(mixed_list->GetBoolean(3, &bool_value));
  ASSERT_EQ(false, bool_value);

  ASSERT_TRUE(mixed_list->GetBoolean(0, &bool_value));
  ASSERT_EQ(true, bool_value);
  ASSERT_TRUE(mixed_list->GetInteger(1, &int_value));
  ASSERT_EQ(42, int_value);
  ASSERT_TRUE(mixed_list->GetReal(2, &double_value));
  ASSERT_EQ(88.8, double_value);
  ASSERT_TRUE(mixed_list->GetString(3, &string_value));
  ASSERT_EQ("foo", string_value);
}

TEST(ValuesTest, BinaryValue) {
  char* buffer = NULL;
  // Passing a null buffer pointer doesn't yield a BinaryValue
  scoped_ptr<BinaryValue> binary(BinaryValue::Create(buffer, 0));
  ASSERT_FALSE(binary.get());

  // If you want to represent an empty binary value, use a zero-length buffer.
  buffer = new char[1];
  ASSERT_TRUE(buffer);
  binary.reset(BinaryValue::Create(buffer, 0));
  ASSERT_TRUE(binary.get());
  ASSERT_TRUE(binary->GetBuffer());
  ASSERT_EQ(buffer, binary->GetBuffer());
  ASSERT_EQ(0U, binary->GetSize());

  // Test the common case of a non-empty buffer
  buffer = new char[15];
  binary.reset(BinaryValue::Create(buffer, 15));
  ASSERT_TRUE(binary.get());
  ASSERT_TRUE(binary->GetBuffer());
  ASSERT_EQ(buffer, binary->GetBuffer());
  ASSERT_EQ(15U, binary->GetSize());

  char stack_buffer[42];
  memset(stack_buffer, '!', 42);
  binary.reset(BinaryValue::CreateWithCopiedBuffer(stack_buffer, 42));
  ASSERT_TRUE(binary.get());
  ASSERT_TRUE(binary->GetBuffer());
  ASSERT_NE(stack_buffer, binary->GetBuffer());
  ASSERT_EQ(42U, binary->GetSize());
  ASSERT_EQ(0, memcmp(stack_buffer, binary->GetBuffer(), binary->GetSize()));
}

TEST(ValuesTest, StringValue) {
  // Test overloaded CreateStringValue.
  scoped_ptr<Value> narrow_value(Value::CreateStringValue("narrow"));
  ASSERT_TRUE(narrow_value.get());
  ASSERT_TRUE(narrow_value->IsType(Value::TYPE_STRING));
  scoped_ptr<Value> wide_value(Value::CreateStringValue(L"wide"));
  ASSERT_TRUE(wide_value.get());
  ASSERT_TRUE(wide_value->IsType(Value::TYPE_STRING));

  // Test overloaded GetString.
  std::string narrow = "http://google.com";
  std::wstring wide = L"http://google.com";
  ASSERT_TRUE(narrow_value->GetAsString(&narrow));
  ASSERT_TRUE(narrow_value->GetAsString(&wide));
  ASSERT_EQ(std::string("narrow"), narrow);
  ASSERT_EQ(std::wstring(L"narrow"), wide);
  ASSERT_TRUE(wide_value->GetAsString(&narrow));
  ASSERT_TRUE(wide_value->GetAsString(&wide));
  ASSERT_EQ(std::string("wide"), narrow);
  ASSERT_EQ(std::wstring(L"wide"), wide);
}

// This is a Value object that allows us to tell if it's been
// properly deleted by modifying the value of external flag on destruction.
class DeletionTestValue : public Value {
public:
  DeletionTestValue(bool* deletion_flag) : Value(TYPE_NULL) {
    Init(deletion_flag);  // Separate function so that we can use ASSERT_*
  }

  void Init(bool* deletion_flag) {
    ASSERT_TRUE(deletion_flag);
    deletion_flag_ = deletion_flag;
    *deletion_flag_ = false;
  }

  ~DeletionTestValue() {
    *deletion_flag_ = true;
  }

private:
  bool* deletion_flag_;
};

TEST(ValuesTest, ListDeletion) {
  bool deletion_flag = true;

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
  }
  EXPECT_TRUE(deletion_flag);

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    list.Clear();
    EXPECT_TRUE(deletion_flag);
  }

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_TRUE(list.Set(0, Value::CreateNullValue()));
    EXPECT_TRUE(deletion_flag);
  }
}

TEST(ValuesTest, ListRemoval) {
  bool deletion_flag = true;
  Value* removed_item = NULL;

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_EQ(1U, list.GetSize());
    EXPECT_FALSE(list.Remove(std::numeric_limits<size_t>::max(),
                             &removed_item));
    EXPECT_FALSE(list.Remove(1, &removed_item));
    EXPECT_TRUE(list.Remove(0, &removed_item));
    ASSERT_TRUE(removed_item);
    EXPECT_EQ(0U, list.GetSize());
  }
  EXPECT_FALSE(deletion_flag);
  delete removed_item;
  removed_item = NULL;
  EXPECT_TRUE(deletion_flag);

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_TRUE(list.Remove(0, NULL));
    EXPECT_TRUE(deletion_flag);
    EXPECT_EQ(0U, list.GetSize());
  }
}

TEST(ValuesTest, DictionaryDeletion) {
  std::wstring key = L"test";
  bool deletion_flag = true;

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
  }
  EXPECT_TRUE(deletion_flag);

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    dict.Clear();
    EXPECT_TRUE(deletion_flag);
  }

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    dict.Set(key, Value::CreateNullValue());
    EXPECT_TRUE(deletion_flag);
  }
}

TEST(ValuesTest, DictionaryRemoval) {
  std::wstring key = L"test";
  bool deletion_flag = true;
  Value* removed_item = NULL;

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_TRUE(dict.HasKey(key));
    EXPECT_FALSE(dict.Remove(L"absent key", &removed_item));
    EXPECT_TRUE(dict.Remove(key, &removed_item));
    EXPECT_FALSE(dict.HasKey(key));
    ASSERT_TRUE(removed_item);
  }
  EXPECT_FALSE(deletion_flag);
  delete removed_item;
  removed_item = NULL;
  EXPECT_TRUE(deletion_flag);

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_TRUE(dict.HasKey(key));
    EXPECT_TRUE(dict.Remove(key, NULL));
    EXPECT_TRUE(deletion_flag);
    EXPECT_FALSE(dict.HasKey(key));
  }
}

TEST(ValuesTest, DeepCopy) {
  DictionaryValue original_dict;
  Value* original_null = Value::CreateNullValue();
  original_dict.Set(L"null", original_null);
  Value* original_bool = Value::CreateBooleanValue(true);
  original_dict.Set(L"bool", original_bool);
  Value* original_int = Value::CreateIntegerValue(42);
  original_dict.Set(L"int", original_int);
  Value* original_real = Value::CreateRealValue(3.14);
  original_dict.Set(L"real", original_real);
  Value* original_string = Value::CreateStringValue("hello");
  original_dict.Set(L"string", original_string);
  Value* original_wstring = Value::CreateStringValue(L"peek-a-boo");
  original_dict.Set(L"wstring", original_wstring);

  char* original_buffer = new char[42];
  memset(original_buffer, '!', 42);
  BinaryValue* original_binary = Value::CreateBinaryValue(original_buffer, 42);
  original_dict.Set(L"binary", original_binary);

  ListValue* original_list = new ListValue();
  Value* original_list_element_0 = Value::CreateIntegerValue(0);
  original_list->Append(original_list_element_0);
  Value* original_list_element_1 = Value::CreateIntegerValue(1);
  original_list->Append(original_list_element_1);
  original_dict.Set(L"list", original_list);

  DictionaryValue* copy_dict =
    static_cast<DictionaryValue*>(original_dict.DeepCopy());
  ASSERT_TRUE(copy_dict);
  ASSERT_NE(copy_dict, &original_dict);

  Value* copy_null = NULL;
  ASSERT_TRUE(copy_dict->Get(L"null", &copy_null));
  ASSERT_TRUE(copy_null);
  ASSERT_NE(copy_null, original_null);
  ASSERT_TRUE(copy_null->IsType(Value::TYPE_NULL));

  Value* copy_bool = NULL;
  ASSERT_TRUE(copy_dict->Get(L"bool", &copy_bool));
  ASSERT_TRUE(copy_bool);
  ASSERT_NE(copy_bool, original_bool);
  ASSERT_TRUE(copy_bool->IsType(Value::TYPE_BOOLEAN));
  bool copy_bool_value = false;
  ASSERT_TRUE(copy_bool->GetAsBoolean(&copy_bool_value));
  ASSERT_TRUE(copy_bool_value);

  Value* copy_int = NULL;
  ASSERT_TRUE(copy_dict->Get(L"int", &copy_int));
  ASSERT_TRUE(copy_int);
  ASSERT_NE(copy_int, original_int);
  ASSERT_TRUE(copy_int->IsType(Value::TYPE_INTEGER));
  int copy_int_value = 0;
  ASSERT_TRUE(copy_int->GetAsInteger(&copy_int_value));
  ASSERT_EQ(42, copy_int_value);

  Value* copy_real = NULL;
  ASSERT_TRUE(copy_dict->Get(L"real", &copy_real));
  ASSERT_TRUE(copy_real);
  ASSERT_NE(copy_real, original_real);
  ASSERT_TRUE(copy_real->IsType(Value::TYPE_REAL));
  double copy_real_value = 0;
  ASSERT_TRUE(copy_real->GetAsReal(&copy_real_value));
  ASSERT_EQ(3.14, copy_real_value);

  Value* copy_string = NULL;
  ASSERT_TRUE(copy_dict->Get(L"string", &copy_string));
  ASSERT_TRUE(copy_string);
  ASSERT_NE(copy_string, original_string);
  ASSERT_TRUE(copy_string->IsType(Value::TYPE_STRING));
  std::string copy_string_value;
  std::wstring copy_wstring_value;
  ASSERT_TRUE(copy_string->GetAsString(&copy_string_value));
  ASSERT_TRUE(copy_string->GetAsString(&copy_wstring_value));
  ASSERT_EQ(std::string("hello"), copy_string_value);
  ASSERT_EQ(std::wstring(L"hello"), copy_wstring_value);

  Value* copy_wstring = NULL;
  ASSERT_TRUE(copy_dict->Get(L"wstring", &copy_wstring));
  ASSERT_TRUE(copy_wstring);
  ASSERT_NE(copy_wstring, original_wstring);
  ASSERT_TRUE(copy_wstring->IsType(Value::TYPE_STRING));
  ASSERT_TRUE(copy_wstring->GetAsString(&copy_string_value));
  ASSERT_TRUE(copy_wstring->GetAsString(&copy_wstring_value));
  ASSERT_EQ(std::string("peek-a-boo"), copy_string_value);
  ASSERT_EQ(std::wstring(L"peek-a-boo"), copy_wstring_value);

  Value* copy_binary = NULL;
  ASSERT_TRUE(copy_dict->Get(L"binary", &copy_binary));
  ASSERT_TRUE(copy_binary);
  ASSERT_NE(copy_binary, original_binary);
  ASSERT_TRUE(copy_binary->IsType(Value::TYPE_BINARY));
  ASSERT_NE(original_binary->GetBuffer(),
    static_cast<BinaryValue*>(copy_binary)->GetBuffer());
  ASSERT_EQ(original_binary->GetSize(),
    static_cast<BinaryValue*>(copy_binary)->GetSize());
  ASSERT_EQ(0, memcmp(original_binary->GetBuffer(),
               static_cast<BinaryValue*>(copy_binary)->GetBuffer(),
               original_binary->GetSize()));

  Value* copy_value = NULL;
  ASSERT_TRUE(copy_dict->Get(L"list", &copy_value));
  ASSERT_TRUE(copy_value);
  ASSERT_NE(copy_value, original_list);
  ASSERT_TRUE(copy_value->IsType(Value::TYPE_LIST));
  ListValue* copy_list = static_cast<ListValue*>(copy_value);
  ASSERT_EQ(2U, copy_list->GetSize());

  Value* copy_list_element_0;
  ASSERT_TRUE(copy_list->Get(0, &copy_list_element_0));
  ASSERT_TRUE(copy_list_element_0);
  ASSERT_NE(copy_list_element_0, original_list_element_0);
  int copy_list_element_0_value;
  ASSERT_TRUE(copy_list_element_0->GetAsInteger(&copy_list_element_0_value));
  ASSERT_EQ(0, copy_list_element_0_value);

  Value* copy_list_element_1;
  ASSERT_TRUE(copy_list->Get(1, &copy_list_element_1));
  ASSERT_TRUE(copy_list_element_1);
  ASSERT_NE(copy_list_element_1, original_list_element_1);
  int copy_list_element_1_value;
  ASSERT_TRUE(copy_list_element_1->GetAsInteger(&copy_list_element_1_value));
  ASSERT_EQ(1, copy_list_element_1_value);

  delete copy_dict;
}

TEST(ValuesTest, Equals) {
  Value* null1 = Value::CreateNullValue();
  Value* null2 = Value::CreateNullValue();
  EXPECT_NE(null1, null2);
  EXPECT_TRUE(null1->Equals(null2));

  Value* boolean = Value::CreateBooleanValue(false);
  EXPECT_FALSE(null1->Equals(boolean));
  delete null1;
  delete null2;
  delete boolean;

  DictionaryValue dv;
  dv.SetBoolean(L"a", false);
  dv.SetInteger(L"b", 2);
  dv.SetReal(L"c", 2.5);
  dv.SetString(L"d1", "string");
  dv.SetString(L"d2", L"string");
  dv.Set(L"e", Value::CreateNullValue());

  DictionaryValue* copy = static_cast<DictionaryValue*>(dv.DeepCopy());
  EXPECT_TRUE(dv.Equals(copy));

  ListValue* list = new ListValue;
  list->Append(Value::CreateNullValue());
  list->Append(new DictionaryValue);
  dv.Set(L"f", list);

  EXPECT_FALSE(dv.Equals(copy));
  copy->Set(L"f", list->DeepCopy());
  EXPECT_TRUE(dv.Equals(copy));

  list->Append(Value::CreateBooleanValue(true));
  EXPECT_FALSE(dv.Equals(copy));
  delete copy;
}
