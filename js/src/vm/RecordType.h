/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_RecordType_h
#define vm_RecordType_h

#include <cstdint>
#include "js/TypeDecls.h"
#include "vm/ArrayObject.h"
#include "vm/NativeObject.h"

namespace JS {
class RecordType;
}

namespace js {

extern JSString* RecordToSource(JSContext* cx, JS::RecordType* rec);

}

namespace JS {

class RecordType final : public js::NativeObject {
  friend JSString* js::RecordToSource(JSContext* cx, RecordType* rec);

  enum { INITIALIZED_LENGTH_SLOT = 0, SORTED_KEYS_SLOT, SLOT_COUNT };

 public:
  static const js::ClassSpec classSpec_;
  static const JSClass class_;

  static RecordType* createUninitialized(JSContext* cx, uint32_t initialLength);
  bool initializeNextProperty(JSContext* cx, Handle<PropertyKey> key,
                              HandleValue value);
  bool finishInitialization(JSContext* cx);

  static bool sameValueZero(JSContext* cx, RecordType* lhs, RecordType* rhs,
                            bool* equal);
  static bool sameValue(JSContext* cx, RecordType* lhs, RecordType* rhs,
                        bool* equal);

  js::ArrayObject* keys() const {
    return &getFixedSlot(SORTED_KEYS_SLOT).toObject().as<js::ArrayObject>();
  }

 private:
  template <bool Comparator(JSContext*, HandleValue, HandleValue, bool*)>
  static bool sameValueWith(JSContext* cx, RecordType* lhs, RecordType* rhs,
                            bool* equal);
};

}  // namespace JS

#endif
