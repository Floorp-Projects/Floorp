/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TypeData_h
#define jit_TypeData_h

#include "js/Value.h"

namespace js {
namespace jit {

class TypeData {
  JSValueType type_;

 public:
  TypeData() : type_(JSVAL_TYPE_UNKNOWN) {}
  explicit TypeData(JSValueType type) : type_(type) {}

  JSValueType type() const { return type_; }
  bool hasData() const { return type_ != JSVAL_TYPE_UNKNOWN; }
};

}  // namespace jit
}  // namespace js

#endif /* jit_TypeData_h */
