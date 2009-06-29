// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_JSON_VALUE_SERIALIZER_H_
#define CHROME_COMMON_JSON_VALUE_SERIALIZER_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/values.h"

class JSONStringValueSerializer : public ValueSerializer {
 public:
  // json_string is the string that will be source of the deserialization
  // or the destination of the serialization.  The caller of the constructor
  // retains ownership of the string.
  JSONStringValueSerializer(std::string* json_string)
      : json_string_(json_string),
        initialized_with_const_string_(false),
        pretty_print_(false),
        allow_trailing_comma_(false) {
  }

  // This version allows initialization with a const string reference for
  // deserialization only.
  JSONStringValueSerializer(const std::string& json_string)
      : json_string_(&const_cast<std::string&>(json_string)),
        initialized_with_const_string_(true),
        allow_trailing_comma_(false) {
  }

  ~JSONStringValueSerializer();

  // Attempt to serialize the data structure represented by Value into
  // JSON.  If the return value is true, the result will have been written
  // into the string passed into the constructor.
  bool Serialize(const Value& root);

  // Attempt to deserialize the data structure encoded in the string passed
  // in to the constructor into a structure of Value objects.  If the return
  // value is NULL and |error_message| is non-null, |error_message| will contain
  // a string describing the error.
  Value* Deserialize(std::string* error_message);

  void set_pretty_print(bool new_value) { pretty_print_ = new_value; }
  bool pretty_print() { return pretty_print_; }

  void set_allow_trailing_comma(bool new_value) {
    allow_trailing_comma_ = new_value;
  }

 private:
  std::string* json_string_;
  bool initialized_with_const_string_;
  bool pretty_print_;  // If true, serialization will span multiple lines.
  // If true, deserialization will allow trailing commas.
  bool allow_trailing_comma_;

  DISALLOW_EVIL_CONSTRUCTORS(JSONStringValueSerializer);
};

class JSONFileValueSerializer : public ValueSerializer {
 public:
  // json_file_patch is the path of a file that will be source of the
  // deserialization or the destination of the serialization.
  // When deserializing, the file should exist, but when serializing, the
  // serializer will attempt to create the file at the specified location.
  explicit JSONFileValueSerializer(const FilePath& json_file_path)
    : json_file_path_(json_file_path) {}

  ~JSONFileValueSerializer() {}

  // DO NOT USE except in unit tests to verify the file was written properly.
  // We should never serialize directly to a file since this will block the
  // thread. Instead, serialize to a string and write to the file you want on
  // the file thread.
  //
  // Attempt to serialize the data structure represented by Value into
  // JSON.  If the return value is true, the result will have been written
  // into the file whose name was passed into the constructor.
  bool Serialize(const Value& root);

  // Attempt to deserialize the data structure encoded in the file passed
  // in to the constructor into a structure of Value objects.  If the return
  // value is NULL, and if |error_message| is non-null, |error_message| will
  // contain a string describing the error. The caller takes ownership of the
  // returned value.
  Value* Deserialize(std::string* error_message);

 private:
  FilePath json_file_path_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSONFileValueSerializer);
};

#endif  // CHROME_COMMON_JSON_VALUE_SERIALIZER_H_
