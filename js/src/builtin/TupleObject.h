/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TupleObject_h
#define builtin_TupleObject_h

#include "vm/NativeObject.h"
#include "vm/TupleType.h"

namespace js {

class TupleObject : public NativeObject {
  enum { PrimitiveValueSlot, SlotCount };

 public:
  static const JSClass class_;

  static TupleObject* create(JSContext* cx, Handle<TupleType*> record);

  JS::TupleType* unbox() const;

  static bool maybeUnbox(JSObject* obj, MutableHandle<TupleType*> tupp);
};

}  // namespace js

#endif
