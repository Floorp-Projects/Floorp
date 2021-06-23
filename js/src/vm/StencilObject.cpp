/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/StencilObject.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "jsapi.h"                      // JS_NewObject
#include "js/Class.h"                   // JSClassOps, JSClass, JSCLASS_*
#include "js/experimental/JSStencil.h"  // JS::Stencil, JS::StencilAddRef, JS::StencilRelease
#include "js/RootingAPI.h"  // JS::Rooted
#include "vm/JSObject.h"    // JSObject

using namespace js;

/*static */ const JSClassOps StencilObject::classOps_ = {
    nullptr,                  // addProperty
    nullptr,                  // delProperty
    nullptr,                  // enumerate
    nullptr,                  // newEnumerate
    nullptr,                  // resolve
    nullptr,                  // mayResolve
    StencilObject::finalize,  // finalize
    nullptr,                  // call
    nullptr,                  // hasInstance
    nullptr,                  // construct
    nullptr,                  // trace
};

/*static */ const JSClass StencilObject::class_ = {
    "StencilObject",
    JSCLASS_HAS_RESERVED_SLOTS(StencilObject::ReservedSlots) |
        JSCLASS_BACKGROUND_FINALIZE,
    &StencilObject::classOps_};

bool StencilObject::hasStencil() const {
  // The stencil may not be present yet if we GC during initialization.
  return !getSlot(StencilSlot).isUndefined();
}

JS::Stencil* StencilObject::stencil() const {
  void* ptr = getSlot(StencilSlot).toPrivate();
  MOZ_ASSERT(ptr);
  return static_cast<JS::Stencil*>(ptr);
}

/* static */ StencilObject* StencilObject::create(JSContext* cx,
                                                  RefPtr<JS::Stencil> stencil) {
  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, &class_));
  if (!obj) {
    return nullptr;
  }

  obj->as<StencilObject>().setReservedSlot(
      StencilSlot, PrivateValue(stencil.forget().take()));

  return &obj->as<StencilObject>();
}

/* static */ void StencilObject::finalize(JSFreeOp* fop, JSObject* obj) {
  if (obj->as<StencilObject>().hasStencil()) {
    JS::StencilRelease(obj->as<StencilObject>().stencil());
  }
}
