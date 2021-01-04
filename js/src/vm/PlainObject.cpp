/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS object implementation.
 */

#include "vm/PlainObject-inl.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Maybe.h"       // mozilla::Maybe

#include "jspubtd.h"  // JSProto_Object

#include "gc/AllocKind.h"   // js::gc::AllocKind
#include "vm/JSContext.h"   // JSContext
#include "vm/JSFunction.h"  // JSFunction
#include "vm/JSObject.h"    // JSObject, js::GetPrototypeFromConstructor
#include "vm/ObjectGroup.h"  // js::ObjectGroup, js::{Generic,Singleton,Tenured}Object
#include "vm/TaggedProto.h"  // js::TaggedProto

#include "vm/JSObject-inl.h"  // js::GuessObjectGCKind, js::NewObjectWithGroup, js::NewObjectGCKind

using JS::Handle;
using JS::Rooted;

using js::GenericObject;
using js::GuessObjectGCKind;
using js::NewObjectGCKind;
using js::NewObjectKind;
using js::NewObjectWithGroup;
using js::ObjectGroup;
using js::PlainObject;
using js::SingletonObject;
using js::TaggedProto;
using js::TenuredObject;

static PlainObject* CreateThisForFunctionWithGroup(JSContext* cx,
                                                   Handle<ObjectGroup*> group,
                                                   NewObjectKind newKind) {
  js::gc::AllocKind allocKind = NewObjectGCKind(&PlainObject::class_);

  MOZ_ASSERT(newKind != SingletonObject);

  return NewObjectWithGroup<PlainObject>(cx, group, allocKind, newKind);
}

PlainObject* js::CreateThisForFunctionWithProto(
    JSContext* cx, Handle<JSFunction*> callee, Handle<JSObject*> newTarget,
    Handle<JSObject*> proto, NewObjectKind newKind /* = GenericObject */) {
  MOZ_ASSERT(!callee->constructorNeedsUninitializedThis());

  Rooted<PlainObject*> res(cx);

  // Ion may call this with a cross-realm callee.
  mozilla::Maybe<AutoRealm> ar;
  if (cx->realm() != callee->realm()) {
    MOZ_ASSERT(cx->compartment() == callee->compartment());
    ar.emplace(cx, callee);
  }

  if (proto) {
    Rooted<ObjectGroup*> group(
        cx, ObjectGroup::defaultNewGroup(cx, &PlainObject::class_,
                                         TaggedProto(proto), newTarget));
    if (!group) {
      return nullptr;
    }
    res = CreateThisForFunctionWithGroup(cx, group, newKind);
  } else {
    res = NewBuiltinClassInstanceWithKind<PlainObject>(cx, newKind);
  }

  MOZ_ASSERT_IF(res, res->nonCCWRealm() == callee->realm());

  return res;
}

PlainObject* js::CreateThisForFunction(JSContext* cx,
                                       Handle<JSFunction*> callee,
                                       Handle<JSObject*> newTarget,
                                       NewObjectKind newKind) {
  MOZ_ASSERT(!callee->constructorNeedsUninitializedThis());

  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromConstructor(cx, newTarget, JSProto_Object, &proto)) {
    return nullptr;
  }

  PlainObject* obj =
      CreateThisForFunctionWithProto(cx, callee, newTarget, proto, newKind);

  if (obj && newKind == SingletonObject) {
    Rooted<PlainObject*> nobj(cx, obj);

    /* Reshape the singleton before passing it as the 'this' value. */
    NativeObject::clear(cx, nobj);

    return nobj;
  }

  return obj;
}
