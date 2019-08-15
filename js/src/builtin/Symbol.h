/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Symbol_h
#define builtin_Symbol_h

#include "vm/NativeObject.h"
#include "vm/SymbolType.h"

namespace js {

class GlobalObject;

class SymbolObject : public NativeObject {
  /* Stores this Symbol object's [[PrimitiveValue]]. */
  static const unsigned PRIMITIVE_VALUE_SLOT = 0;

 public:
  static const unsigned RESERVED_SLOTS = 1;

  static const JSClass class_;

  static JSObject* initClass(JSContext* cx, Handle<GlobalObject*> global,
                             bool defineMembers);

  /*
   * Creates a new Symbol object boxing the given primitive Symbol.  The
   * object's [[Prototype]] is determined from context.
   */
  static SymbolObject* create(JSContext* cx, JS::HandleSymbol symbol);

  JS::Symbol* unbox() const {
    return getFixedSlot(PRIMITIVE_VALUE_SLOT).toSymbol();
  }

 private:
  inline void setPrimitiveValue(JS::Symbol* symbol) {
    setFixedSlot(PRIMITIVE_VALUE_SLOT, SymbolValue(symbol));
  }

  static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

  // Static methods.
  static MOZ_MUST_USE bool for_(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool keyFor(JSContext* cx, unsigned argc, Value* vp);

  // Methods defined on Symbol.prototype.
  static MOZ_MUST_USE bool toString_impl(JSContext* cx, const CallArgs& args);
  static MOZ_MUST_USE bool toString(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool valueOf_impl(JSContext* cx, const CallArgs& args);
  static MOZ_MUST_USE bool valueOf(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool toPrimitive(JSContext* cx, unsigned argc, Value* vp);

  // Properties defined on Symbol.prototype.
  static MOZ_MUST_USE bool descriptionGetter_impl(JSContext* cx,
                                                  const CallArgs& args);
  static MOZ_MUST_USE bool descriptionGetter(JSContext* cx, unsigned argc,
                                             Value* vp);

  static const JSPropertySpec properties[];
  static const JSFunctionSpec methods[];
  static const JSFunctionSpec staticMethods[];
};

extern JSObject* InitSymbolClass(JSContext* cx, Handle<GlobalObject*> global);

extern JSObject* InitBareSymbolCtor(JSContext* cx,
                                    Handle<GlobalObject*> global);

} /* namespace js */

#endif /* builtin_Symbol_h */
