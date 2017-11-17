/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JSONFormatter_h
#define JSONFormatter_h

#include <memory>
#include <string>

// A very basic JSON formatter that records key/value pairs and outputs a JSON
// object that contains only non-object data.
class JSONFormatter {
  // Of these fields, only mEscapedStringValue is owned by this class. All the
  // others are expected to outlive the class (which is typically allocated
  // on-stack).
  struct Property {
    const char *Name;
    std::string StringValue;
    int IntValue;
    bool IsString;

    Property() {}

    Property(const char* Name, std::string String)
      : Name(Name), StringValue(std::move(String)), IsString(true) {}

    Property(const char* Name, int Int)
      : Name(Name), IntValue(Int), IsString(false) {}
  };

  static const int kMaxProperties = 32;

  Property Properties[kMaxProperties];
  int PropertyCount;

  // Length of the generated JSON output.
  size_t Length;

  std::string escape(std::string Input);

public:
  JSONFormatter() : PropertyCount(0), Length(0) {}

  void add(const char *Name, const char *Value);
  void add(const char *Name, std::string Value);
  void add(const char *Name, int Value);

  void format(std::string &Result);
};

#endif
