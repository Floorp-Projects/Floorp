/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_ParseRecordObject_h
#define builtin_ParseRecordObject_h

#include "js/TraceKind.h"
#include "vm/JSContext.h"

namespace js {

using JSONParseNode = JSString;

class ParseRecordObject {
 public:
  JSONParseNode* parseNode;
  JS::PropertyKey key;
  Value value;

  ParseRecordObject();
  ParseRecordObject(Handle<js::JSONParseNode*> parseNode, const Value& val);
  ParseRecordObject(ParseRecordObject&& other)
      : parseNode(std::move(other.parseNode)),
        key(std::move(other.key)),
        value(std::move(other.value)) {}

  bool isEmpty() const { return value.isUndefined(); }

  // move assignment
  ParseRecordObject& operator=(ParseRecordObject&& other) noexcept {
    parseNode = other.parseNode;
    key = other.key;
    value = other.value;
    return *this;
  }

  void trace(JSTracer* trc);
};

}  // namespace js

#endif
