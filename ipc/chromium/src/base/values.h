// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file specifies a recursive data storage class called Value
// intended for storing setting and other persistable data.
// It includes the ability to specify (recursive) lists and dictionaries, so
// it's fairly expressive.  However, the API is optimized for the common case,
// namely storing a hierarchical tree of simple values.  Given a
// DictionaryValue root, you can easily do things like:
//
// root->SetString(L"global.pages.homepage", L"http://goateleporter.com");
// std::wstring homepage = L"http://google.com";  // default/fallback value
// root->GetString(L"global.pages.homepage", &homepage);
//
// where "global" and "pages" are also DictionaryValues, and "homepage"
// is a string setting.  If some elements of the path didn't exist yet,
// the SetString() method would create the missing elements and attach them
// to root before attaching the homepage value.

#ifndef BASE_VALUES_H_
#define BASE_VALUES_H_

#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"

class Value;
class FundamentalValue;
class StringValue;
class BinaryValue;
class DictionaryValue;
class ListValue;

typedef std::vector<Value*> ValueVector;
typedef std::map<std::wstring, Value*> ValueMap;

// The Value class is the base class for Values.  A Value can be
// instantiated via the Create*Value() factory methods, or by directly
// creating instances of the subclasses.
class Value {
 public:
  virtual ~Value();

  // Convenience methods for creating Value objects for various
  // kinds of values without thinking about which class implements them.
  // These can always be expected to return a valid Value*.
  static Value* CreateNullValue();
  static Value* CreateBooleanValue(bool in_value);
  static Value* CreateIntegerValue(int in_value);
  static Value* CreateRealValue(double in_value);
  static Value* CreateStringValue(const std::string& in_value);
  static Value* CreateStringValue(const std::wstring& in_value);

  // This one can return NULL if the input isn't valid.  If the return value
  // is non-null, the new object has taken ownership of the buffer pointer.
  static BinaryValue* CreateBinaryValue(char* buffer, size_t size);

  typedef enum {
    TYPE_NULL = 0,
    TYPE_BOOLEAN,
    TYPE_INTEGER,
    TYPE_REAL,
    TYPE_STRING,
    TYPE_BINARY,
    TYPE_DICTIONARY,
    TYPE_LIST
  } ValueType;

  // Returns the type of the value stored by the current Value object.
  // Each type will be implemented by only one subclass of Value, so it's
  // safe to use the ValueType to determine whether you can cast from
  // Value* to (Implementing Class)*.  Also, a Value object never changes
  // its type after construction.
  ValueType GetType() const { return type_; }

  // Returns true if the current object represents a given type.
  bool IsType(ValueType type) const { return type == type_; }

  // These methods allow the convenient retrieval of settings.
  // If the current setting object can be converted into the given type,
  // the value is returned through the "value" parameter and true is returned;
  // otherwise, false is returned and "value" is unchanged.
  virtual bool GetAsBoolean(bool* out_value) const;
  virtual bool GetAsInteger(int* out_value) const;
  virtual bool GetAsReal(double* out_value) const;
  virtual bool GetAsString(std::string* out_value) const;
  virtual bool GetAsString(std::wstring* out_value) const;

  // This creates a deep copy of the entire Value tree, and returns a pointer
  // to the copy.  The caller gets ownership of the copy, of course.
  virtual Value* DeepCopy() const;

  // Compares if two Value objects have equal contents.
  virtual bool Equals(const Value* other) const;

 protected:
  // This isn't safe for end-users (they should use the Create*Value()
  // static methods above), but it's useful for subclasses.
  Value(ValueType type) : type_(type) {}

 private:
  DISALLOW_EVIL_CONSTRUCTORS(Value);
  Value();

  ValueType type_;
};

// FundamentalValue represents the simple fundamental types of values.
class FundamentalValue : public Value {
 public:
  FundamentalValue(bool in_value)
    : Value(TYPE_BOOLEAN), boolean_value_(in_value) {}
  FundamentalValue(int in_value)
    : Value(TYPE_INTEGER), integer_value_(in_value) {}
  FundamentalValue(double in_value)
    : Value(TYPE_REAL), real_value_(in_value) {}
  ~FundamentalValue();

  // Subclassed methods
  virtual bool GetAsBoolean(bool* out_value) const;
  virtual bool GetAsInteger(int* out_value) const;
  virtual bool GetAsReal(double* out_value) const;
  virtual Value* DeepCopy() const;
  virtual bool Equals(const Value* other) const;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(FundamentalValue);

  union {
    bool boolean_value_;
    int integer_value_;
    double real_value_;
  };
};

class StringValue : public Value {
 public:
  // Initializes a StringValue with a UTF-8 narrow character string.
  StringValue(const std::string& in_value);

  // Initializes a StringValue with a wide character string.
  StringValue(const std::wstring& in_value);

  ~StringValue();

  // Subclassed methods
  bool GetAsString(std::string* out_value) const;
  bool GetAsString(std::wstring* out_value) const;
  Value* DeepCopy() const;
  virtual bool Equals(const Value* other) const;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(StringValue);

  std::string value_;
};

class BinaryValue: public Value {
public:
  // Creates a Value to represent a binary buffer.  The new object takes
  // ownership of the pointer passed in, if successful.
  // Returns NULL if buffer is NULL.
  static BinaryValue* Create(char* buffer, size_t size);

  // For situations where you want to keep ownership of your buffer, this
  // factory method creates a new BinaryValue by copying the contents of the
  // buffer that's passed in.
  // Returns NULL if buffer is NULL.
  static BinaryValue* CreateWithCopiedBuffer(char* buffer, size_t size);

  ~BinaryValue();

  // Subclassed methods
  Value* DeepCopy() const;
  virtual bool Equals(const Value* other) const;

  size_t GetSize() const { return size_; }
  char* GetBuffer() { return buffer_; }

private:
  DISALLOW_EVIL_CONSTRUCTORS(BinaryValue);

  // Constructor is private so that only objects with valid buffer pointers
  // and size values can be created.
  BinaryValue(char* buffer, size_t size);

  char* buffer_;
  size_t size_;
};

class DictionaryValue : public Value {
 public:
  DictionaryValue() : Value(TYPE_DICTIONARY) {}
  ~DictionaryValue();

  // Subclassed methods
  Value* DeepCopy() const;
  virtual bool Equals(const Value* other) const;

  // Returns true if the current dictionary has a value for the given key.
  bool HasKey(const std::wstring& key) const;

  // Returns the number of Values in this dictionary.
  size_t GetSize() const { return dictionary_.size(); }

  // Clears any current contents of this dictionary.
  void Clear();

  // Sets the Value associated with the given path starting from this object.
  // A path has the form "<key>" or "<key>.<key>.[...]", where "." indexes
  // into the next DictionaryValue down.  Obviously, "." can't be used
  // within a key, but there are no other restrictions on keys.
  // If the key at any step of the way doesn't exist, or exists but isn't
  // a DictionaryValue, a new DictionaryValue will be created and attached
  // to the path in that location.
  // Note that the dictionary takes ownership of the value referenced by
  // |in_value|.
  bool Set(const std::wstring& path, Value* in_value);

  // Convenience forms of Set().  These methods will replace any existing
  // value at that path, even if it has a different type.
  bool SetBoolean(const std::wstring& path, bool in_value);
  bool SetInteger(const std::wstring& path, int in_value);
  bool SetReal(const std::wstring& path, double in_value);
  bool SetString(const std::wstring& path, const std::string& in_value);
  bool SetString(const std::wstring& path, const std::wstring& in_value);

  // Gets the Value associated with the given path starting from this object.
  // A path has the form "<key>" or "<key>.<key>.[...]", where "." indexes
  // into the next DictionaryValue down.  If the path can be resolved
  // successfully, the value for the last key in the path will be returned
  // through the "value" parameter, and the function will return true.
  // Otherwise, it will return false and "value" will be untouched.
  // Note that the dictionary always owns the value that's returned.
  bool Get(const std::wstring& path, Value** out_value) const;

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the path is valid and the value at
  // the end of the path can be returned in the form specified.
  bool GetBoolean(const std::wstring& path, bool* out_value) const;
  bool GetInteger(const std::wstring& path, int* out_value) const;
  bool GetReal(const std::wstring& path, double* out_value) const;
  bool GetString(const std::wstring& path, std::string* out_value) const;
  bool GetString(const std::wstring& path, std::wstring* out_value) const;
  bool GetBinary(const std::wstring& path, BinaryValue** out_value) const;
  bool GetDictionary(const std::wstring& path,
                     DictionaryValue** out_value) const;
  bool GetList(const std::wstring& path, ListValue** out_value) const;

  // Removes the Value with the specified path from this dictionary (or one
  // of its child dictionaries, if the path is more than just a local key).
  // If |out_value| is non-NULL, the removed Value AND ITS OWNERSHIP will be
  // passed out via out_value.  If |out_value| is NULL, the removed value will
  // be deleted.  This method returns true if |path| is a valid path; otherwise
  // it will return false and the DictionaryValue object will be unchanged.
  bool Remove(const std::wstring& path, Value** out_value);

  // This class provides an iterator for the keys in the dictionary.
  // It can't be used to modify the dictionary.
  class key_iterator
    : private std::iterator<std::input_iterator_tag, const std::wstring> {
   public:
    key_iterator(ValueMap::const_iterator itr) { itr_ = itr; }
    key_iterator operator++() { ++itr_; return *this; }
    const std::wstring& operator*() { return itr_->first; }
    bool operator!=(const key_iterator& other) { return itr_ != other.itr_; }
    bool operator==(const key_iterator& other) { return itr_ == other.itr_; }

   private:
    ValueMap::const_iterator itr_;
  };

  key_iterator begin_keys() const { return key_iterator(dictionary_.begin()); }
  key_iterator end_keys() const { return key_iterator(dictionary_.end()); }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DictionaryValue);

  // Associates the value |in_value| with the |key|.  This method should be
  // used instead of "dictionary_[key] = foo" so that any previous value can
  // be properly deleted.
  void SetInCurrentNode(const std::wstring& key, Value* in_value);

  ValueMap dictionary_;
};

// This type of Value represents a list of other Value values.
class ListValue : public Value {
 public:
  ListValue() : Value(TYPE_LIST) {}
  ~ListValue();

  // Subclassed methods
  Value* DeepCopy() const;
  virtual bool Equals(const Value* other) const;

  // Clears the contents of this ListValue
  void Clear();

  // Returns the number of Values in this list.
  size_t GetSize() const { return list_.size(); }

  // Sets the list item at the given index to be the Value specified by
  // the value given.  If the index beyond the current end of the list, null
  // Values will be used to pad out the list.
  // Returns true if successful, or false if the index was negative or
  // the value is a null pointer.
  bool Set(size_t index, Value* in_value);

  // Gets the Value at the given index.  Modifies value (and returns true)
  // only if the index falls within the current list range.
  // Note that the list always owns the Value passed out via out_value.
  bool Get(size_t index, Value** out_value) const;

  // Convenience forms of Get().  Modifies value (and returns true) only if
  // the index is valid and the Value at that index can be returned in
  // the specified form.
  bool GetBoolean(size_t index, bool* out_value) const;
  bool GetInteger(size_t index, int* out_value) const;
  bool GetReal(size_t index, double* out_value) const;
  bool GetString(size_t index, std::string* out_value) const;
  bool GetBinary(size_t index, BinaryValue** out_value) const;
  bool GetDictionary(size_t index, DictionaryValue** out_value) const;
  bool GetList(size_t index, ListValue** out_value) const;

  // Removes the Value with the specified index from this list.
  // If |out_value| is non-NULL, the removed Value AND ITS OWNERSHIP will be
  // passed out via |out_value|.  If |out_value| is NULL, the removed value will
  // be deleted.  This method returns true if |index| is valid; otherwise
  // it will return false and the ListValue object will be unchanged.
  bool Remove(size_t index, Value** out_value);

  // Appends a Value to the end of the list.
  void Append(Value* in_value);

  // Iteration
  typedef ValueVector::iterator iterator;
  typedef ValueVector::const_iterator const_iterator;

  ListValue::iterator begin() { return list_.begin(); }
  ListValue::iterator end() { return list_.end(); }

  ListValue::const_iterator begin() const { return list_.begin(); }
  ListValue::const_iterator end() const { return list_.end(); }

  ListValue::iterator Erase(iterator item) {
    return list_.erase(item);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ListValue);

  ValueVector list_;
};

// This interface is implemented by classes that know how to serialize and
// deserialize Value objects.
class ValueSerializer {
 public:
  virtual ~ValueSerializer() {}

  virtual bool Serialize(const Value& root) = 0;

  // This method deserializes the subclass-specific format into a Value object.
  // If the return value is non-NULL, the caller takes ownership of returned
  // Value. If the return value is NULL, and if error_message is non-NULL,
  // error_message should be filled with a message describing the error.
  virtual Value* Deserialize(std::string* error_message) = 0;
};

#endif  // BASE_VALUES_H_
