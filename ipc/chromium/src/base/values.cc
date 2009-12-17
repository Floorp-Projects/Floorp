// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"

///////////////////// Value ////////////////////

Value::~Value() {
}

// static
Value* Value::CreateNullValue() {
  return new Value(TYPE_NULL);
}

// static
Value* Value::CreateBooleanValue(bool in_value) {
  return new FundamentalValue(in_value);
}

// static
Value* Value::CreateIntegerValue(int in_value) {
  return new FundamentalValue(in_value);
}

// static
Value* Value::CreateRealValue(double in_value) {
  return new FundamentalValue(in_value);
}

// static
Value* Value::CreateStringValue(const std::string& in_value) {
  return new StringValue(in_value);
}

// static
Value* Value::CreateStringValue(const std::wstring& in_value) {
  return new StringValue(in_value);
}

// static
BinaryValue* Value::CreateBinaryValue(char* buffer, size_t size) {
  return BinaryValue::Create(buffer, size);
}

bool Value::GetAsBoolean(bool* in_value) const {
  return false;
}

bool Value::GetAsInteger(int* in_value) const {
  return false;
}

bool Value::GetAsReal(double* in_value) const {
  return false;
}

bool Value::GetAsString(std::string* in_value) const {
  return false;
}

bool Value::GetAsString(std::wstring* in_value) const {
  return false;
}

Value* Value::DeepCopy() const {
  // This method should only be getting called for null Values--all subclasses
  // need to provide their own implementation;.
  DCHECK(IsType(TYPE_NULL));
  return CreateNullValue();
}

bool Value::Equals(const Value* other) const {
  // This method should only be getting called for null Values--all subclasses
  // need to provide their own implementation;.
  DCHECK(IsType(TYPE_NULL));
  return other->IsType(TYPE_NULL);
}

///////////////////// FundamentalValue ////////////////////

FundamentalValue::~FundamentalValue() {
}

bool FundamentalValue::GetAsBoolean(bool* out_value) const {
  if (out_value && IsType(TYPE_BOOLEAN))
    *out_value = boolean_value_;
  return (IsType(TYPE_BOOLEAN));
}

bool FundamentalValue::GetAsInteger(int* out_value) const {
  if (out_value && IsType(TYPE_INTEGER))
    *out_value = integer_value_;
  return (IsType(TYPE_INTEGER));
}

bool FundamentalValue::GetAsReal(double* out_value) const {
  if (out_value && IsType(TYPE_REAL))
    *out_value = real_value_;
  return (IsType(TYPE_REAL));
}

Value* FundamentalValue::DeepCopy() const {
  switch (GetType()) {
    case TYPE_BOOLEAN:
      return CreateBooleanValue(boolean_value_);

    case TYPE_INTEGER:
      return CreateIntegerValue(integer_value_);

    case TYPE_REAL:
      return CreateRealValue(real_value_);

    default:
      NOTREACHED();
      return NULL;
  }
}

bool FundamentalValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;

  switch (GetType()) {
    case TYPE_BOOLEAN: {
      bool lhs, rhs;
      return GetAsBoolean(&lhs) && other->GetAsBoolean(&rhs) && lhs == rhs;
    }
    case TYPE_INTEGER: {
      int lhs, rhs;
      return GetAsInteger(&lhs) && other->GetAsInteger(&rhs) && lhs == rhs;
    }
    case TYPE_REAL: {
      double lhs, rhs;
      return GetAsReal(&lhs) && other->GetAsReal(&rhs) && lhs == rhs;
    }
    default:
      NOTREACHED();
      return false;
  }
}

///////////////////// StringValue ////////////////////

StringValue::StringValue(const std::string& in_value)
    : Value(TYPE_STRING),
      value_(in_value) {
  DCHECK(IsStringUTF8(in_value));
}

StringValue::StringValue(const std::wstring& in_value)
    : Value(TYPE_STRING),
      value_(WideToUTF8(in_value)) {
}

StringValue::~StringValue() {
}

bool StringValue::GetAsString(std::string* out_value) const {
  if (out_value)
    *out_value = value_;
  return true;
}

bool StringValue::GetAsString(std::wstring* out_value) const {
  if (out_value)
    *out_value = UTF8ToWide(value_);
  return true;
}

Value* StringValue::DeepCopy() const {
  return CreateStringValue(value_);
}

bool StringValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;
  std::string lhs, rhs;
  return GetAsString(&lhs) && other->GetAsString(&rhs) && lhs == rhs;
}

///////////////////// BinaryValue ////////////////////

//static
BinaryValue* BinaryValue::Create(char* buffer, size_t size) {
  if (!buffer)
    return NULL;

  return new BinaryValue(buffer, size);
}

//static
BinaryValue* BinaryValue::CreateWithCopiedBuffer(char* buffer, size_t size) {
  if (!buffer)
    return NULL;

  char* buffer_copy = new char[size];
  memcpy(buffer_copy, buffer, size);
  return new BinaryValue(buffer_copy, size);
}


BinaryValue::BinaryValue(char* buffer, size_t size)
  : Value(TYPE_BINARY),
    buffer_(buffer),
    size_(size) {
  DCHECK(buffer_);
}

BinaryValue::~BinaryValue() {
  DCHECK(buffer_);
  if (buffer_)
    delete[] buffer_;
}

Value* BinaryValue::DeepCopy() const {
  return CreateWithCopiedBuffer(buffer_, size_);
}

bool BinaryValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;
  const BinaryValue* other_binary = static_cast<const BinaryValue*>(other);
  if (other_binary->size_ != size_)
    return false;
  return !memcmp(buffer_, other_binary->buffer_, size_);
}

///////////////////// DictionaryValue ////////////////////

DictionaryValue::~DictionaryValue() {
  Clear();
}

void DictionaryValue::Clear() {
  ValueMap::iterator dict_iterator = dictionary_.begin();
  while (dict_iterator != dictionary_.end()) {
    delete dict_iterator->second;
    ++dict_iterator;
  }

  dictionary_.clear();
}

bool DictionaryValue::HasKey(const std::wstring& key) const {
  ValueMap::const_iterator current_entry = dictionary_.find(key);
  DCHECK((current_entry == dictionary_.end()) || current_entry->second);
  return current_entry != dictionary_.end();
}

void DictionaryValue::SetInCurrentNode(const std::wstring& key,
                                       Value* in_value) {
  // If there's an existing value here, we need to delete it, because
  // we own all our children.
  if (HasKey(key)) {
    DCHECK(dictionary_[key] != in_value);  // This would be bogus
    delete dictionary_[key];
  }

  dictionary_[key] = in_value;
}

bool DictionaryValue::Set(const std::wstring& path, Value* in_value) {
  DCHECK(in_value);

  std::wstring key = path;

  size_t delimiter_position = path.find_first_of(L".", 0);
  // If there isn't a dictionary delimiter in the path, we're done.
  if (delimiter_position == std::wstring::npos) {
    SetInCurrentNode(key, in_value);
    return true;
  } else {
    key = path.substr(0, delimiter_position);
  }

  // Assume that we're indexing into a dictionary.
  DictionaryValue* entry = NULL;
  if (!HasKey(key) || (dictionary_[key]->GetType() != TYPE_DICTIONARY)) {
    entry = new DictionaryValue;
    SetInCurrentNode(key, entry);
  } else {
    entry = static_cast<DictionaryValue*>(dictionary_[key]);
  }

  std::wstring remaining_path = path.substr(delimiter_position + 1);
  return entry->Set(remaining_path, in_value);
}

bool DictionaryValue::SetBoolean(const std::wstring& path, bool in_value) {
  return Set(path, CreateBooleanValue(in_value));
}

bool DictionaryValue::SetInteger(const std::wstring& path, int in_value) {
  return Set(path, CreateIntegerValue(in_value));
}

bool DictionaryValue::SetReal(const std::wstring& path, double in_value) {
  return Set(path, CreateRealValue(in_value));
}

bool DictionaryValue::SetString(const std::wstring& path,
                                const std::string& in_value) {
  return Set(path, CreateStringValue(in_value));
}

bool DictionaryValue::SetString(const std::wstring& path,
                                const std::wstring& in_value) {
  return Set(path, CreateStringValue(in_value));
}

bool DictionaryValue::Get(const std::wstring& path, Value** out_value) const {
  std::wstring key = path;

  size_t delimiter_position = path.find_first_of(L".", 0);
  if (delimiter_position != std::wstring::npos) {
    key = path.substr(0, delimiter_position);
  }

  ValueMap::const_iterator entry_iterator = dictionary_.find(key);
  if (entry_iterator == dictionary_.end())
    return false;
  Value* entry = entry_iterator->second;

  if (delimiter_position == std::wstring::npos) {
    if (out_value)
      *out_value = entry;
    return true;
  }

  if (entry->IsType(TYPE_DICTIONARY)) {
    DictionaryValue* dictionary = static_cast<DictionaryValue*>(entry);
    return dictionary->Get(path.substr(delimiter_position + 1), out_value);
  }

  return false;
}

bool DictionaryValue::GetBoolean(const std::wstring& path,
                                 bool* bool_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsBoolean(bool_value);
}

bool DictionaryValue::GetInteger(const std::wstring& path,
                                 int* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsInteger(out_value);
}

bool DictionaryValue::GetReal(const std::wstring& path,
                              double* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsReal(out_value);
}

bool DictionaryValue::GetString(const std::wstring& path,
                                std::string* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsString(out_value);
}

bool DictionaryValue::GetString(const std::wstring& path,
                                std::wstring* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsString(out_value);
}

bool DictionaryValue::GetBinary(const std::wstring& path,
                                BinaryValue** out_value) const {
  Value* value;
  bool result = Get(path, &value);
  if (!result || !value->IsType(TYPE_BINARY))
    return false;

  if (out_value)
    *out_value = static_cast<BinaryValue*>(value);

  return true;
}

bool DictionaryValue::GetDictionary(const std::wstring& path,
                                    DictionaryValue** out_value) const {
  Value* value;
  bool result = Get(path, &value);
  if (!result || !value->IsType(TYPE_DICTIONARY))
    return false;

  if (out_value)
    *out_value = static_cast<DictionaryValue*>(value);

  return true;
}

bool DictionaryValue::GetList(const std::wstring& path,
                              ListValue** out_value) const {
  Value* value;
  bool result = Get(path, &value);
  if (!result || !value->IsType(TYPE_LIST))
    return false;

  if (out_value)
    *out_value = static_cast<ListValue*>(value);

  return true;
}

bool DictionaryValue::Remove(const std::wstring& path, Value** out_value) {
  std::wstring key = path;

  size_t delimiter_position = path.find_first_of(L".", 0);
  if (delimiter_position != std::wstring::npos) {
    key = path.substr(0, delimiter_position);
  }

  ValueMap::iterator entry_iterator = dictionary_.find(key);
  if (entry_iterator == dictionary_.end())
    return false;
  Value* entry = entry_iterator->second;

  if (delimiter_position == std::wstring::npos) {
    if (out_value)
      *out_value = entry;
    else
      delete entry;

    dictionary_.erase(entry_iterator);
    return true;
  }

  if (entry->IsType(TYPE_DICTIONARY)) {
    DictionaryValue* dictionary = static_cast<DictionaryValue*>(entry);
    return dictionary->Remove(path.substr(delimiter_position + 1), out_value);
  }

  return false;
}

Value* DictionaryValue::DeepCopy() const {
  DictionaryValue* result = new DictionaryValue;

  ValueMap::const_iterator current_entry = dictionary_.begin();
  while (current_entry != dictionary_.end()) {
    result->Set(current_entry->first, current_entry->second->DeepCopy());
    ++current_entry;
  }

  return result;
}

bool DictionaryValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;

  const DictionaryValue* other_dict =
      static_cast<const DictionaryValue*>(other);
  key_iterator lhs_it(begin_keys());
  key_iterator rhs_it(other_dict->begin_keys());
  while (lhs_it != end_keys() && rhs_it != other_dict->end_keys()) {
    Value* lhs;
    Value* rhs;
    if (!Get(*lhs_it, &lhs) || !other_dict->Get(*rhs_it, &rhs) ||
        !lhs->Equals(rhs)) {
      return false;
    }
    ++lhs_it;
    ++rhs_it;
  }
  if (lhs_it != end_keys() || rhs_it != other_dict->end_keys())
    return false;

  return true;
}

///////////////////// ListValue ////////////////////

ListValue::~ListValue() {
  Clear();
}

void ListValue::Clear() {
  ValueVector::iterator list_iterator = list_.begin();
  while (list_iterator != list_.end()) {
    delete *list_iterator;
    ++list_iterator;
  }
  list_.clear();
}

bool ListValue::Set(size_t index, Value* in_value) {
  if (!in_value)
    return false;

  if (index >= list_.size()) {
    // Pad out any intermediate indexes with null settings
    while (index > list_.size())
      Append(CreateNullValue());
    Append(in_value);
  } else {
    DCHECK(list_[index] != in_value);
    delete list_[index];
    list_[index] = in_value;
  }
  return true;
}

bool ListValue::Get(size_t index, Value** out_value) const {
  if (index >= list_.size())
    return false;

  if (out_value)
    *out_value = list_[index];

  return true;
}

bool ListValue::GetBoolean(size_t index, bool* bool_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsBoolean(bool_value);
}

bool ListValue::GetInteger(size_t index, int* out_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsInteger(out_value);
}

bool ListValue::GetReal(size_t index, double* out_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsReal(out_value);
}

bool ListValue::GetString(size_t index, std::string* out_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsString(out_value);
}

bool ListValue::GetBinary(size_t index, BinaryValue** out_value) const {
  Value* value;
  bool result = Get(index, &value);
  if (!result || !value->IsType(TYPE_BINARY))
    return false;

  if (out_value)
    *out_value = static_cast<BinaryValue*>(value);

  return true;
}

bool ListValue::GetDictionary(size_t index, DictionaryValue** out_value) const {
  Value* value;
  bool result = Get(index, &value);
  if (!result || !value->IsType(TYPE_DICTIONARY))
    return false;

  if (out_value)
    *out_value = static_cast<DictionaryValue*>(value);

  return true;
}

bool ListValue::GetList(size_t index, ListValue** out_value) const {
  Value* value;
  bool result = Get(index, &value);
  if (!result || !value->IsType(TYPE_LIST))
    return false;

  if (out_value)
    *out_value = static_cast<ListValue*>(value);

  return true;
}

bool ListValue::Remove(size_t index, Value** out_value) {
  if (index >= list_.size())
    return false;

  if (out_value)
    *out_value = list_[index];
  else
    delete list_[index];

  ValueVector::iterator entry = list_.begin();
  entry += index;

  list_.erase(entry);
  return true;
}

void ListValue::Append(Value* in_value) {
  DCHECK(in_value);
  list_.push_back(in_value);
}

Value* ListValue::DeepCopy() const {
  ListValue* result = new ListValue;

  ValueVector::const_iterator current_entry = list_.begin();
  while (current_entry != list_.end()) {
    result->Append((*current_entry)->DeepCopy());
    ++current_entry;
  }

  return result;
}

bool ListValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;

  const ListValue* other_list =
      static_cast<const ListValue*>(other);
  const_iterator lhs_it, rhs_it;
  for (lhs_it = begin(), rhs_it = other_list->begin();
       lhs_it != end() && rhs_it != other_list->end();
       ++lhs_it, ++rhs_it) {
    if (!(*lhs_it)->Equals(*rhs_it))
      return false;
  }
  if (lhs_it != end() || rhs_it != other_list->end())
    return false;

  return true;
}
