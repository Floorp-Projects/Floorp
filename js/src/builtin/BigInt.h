/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_BigInt_h
#define builtin_BigInt_h

#include "js/Class.h"
#include "js/RootingAPI.h"
#include "vm/BigIntType.h"
#include "vm/NativeObject.h"

namespace js {

class GlobalObject;

class BigIntObject : public NativeObject
{
    static const unsigned PRIMITIVE_VALUE_SLOT = 0;
    static const unsigned RESERVED_SLOTS = 1;

  public:
    static const ClassSpec classSpec_;
    static const Class class_;

    static JSObject* create(JSContext* cx, JS::Handle<JS::BigInt*> bi);

    // Methods defined on BigInt.prototype.
    static bool valueOf_impl(JSContext* cx, const CallArgs& args);
    static bool valueOf(JSContext* cx, unsigned argc, JS::Value* vp);
    static bool toString_impl(JSContext* cx, const CallArgs& args);
    static bool toString(JSContext* cx, unsigned argc, JS::Value* vp);
    static bool toLocaleString_impl(JSContext* cx, const CallArgs& args);
    static bool toLocaleString(JSContext* cx, unsigned argc, JS::Value* vp);

    JS::BigInt* unbox() const;

  private:
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
};

extern JSObject*
InitBigIntClass(JSContext* cx, Handle<GlobalObject*> global);

extern bool
intrinsic_ToBigInt(JSContext* cx, unsigned argc, JS::Value* vp);

} // namespace js

#endif
