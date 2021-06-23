/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_StencilObject_h
#define vm_StencilObject_h

#include "mozilla/RefPtr.h"  // RefPtr

#include <stddef.h>  // size_t

#include "js/Class.h"                   // JSClassOps, JSClass
#include "js/experimental/JSStencil.h"  // JS::Stencil
#include "vm/NativeObject.h"            // NativeObject

class JSFreeOp;
class JSObject;

namespace js {

// Object that holds JS::Stencil.
//
// This is a testing-only feature which can only be produced by testing
// functions.
class StencilObject : public NativeObject {
  static constexpr size_t StencilSlot = 0;
  static constexpr size_t ReservedSlots = 1;

 public:
  static const JSClassOps classOps_;
  static const JSClass class_;

  bool hasStencil() const;
  JS::Stencil* stencil() const;

  static StencilObject* create(JSContext* cx, RefPtr<JS::Stencil> stencil);
  static void finalize(JSFreeOp* fop, JSObject* obj);
};

} /* namespace js */

#endif /* vm_StencilObject_h */
