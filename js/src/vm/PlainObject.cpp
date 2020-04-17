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
#include "vm/TaggedProto.h"    // js::TaggedProto
#include "vm/TypeInference.h"  // js::AutoSweepObjectGroup

#include "vm/JSObject-inl.h"  // js::GuessObjectGCKind, js::NewObjectWithGroup, js::NewObjectGCKind, js::NewObjectWithGivenTaggedProto
#include "vm/TypeInference-inl.h"  // js::AutoSweepObjectGroup::AutoSweepObjectGroup, js::TypeNewScript, js::jit::JitScript::MonitorThisType, js::TypeSet::ObjectType

using JS::Handle;
using JS::Rooted;

using js::AutoSweepObjectGroup;
using js::CopyInitializerObject;
using js::GenericObject;
using js::GuessObjectGCKind;
using js::NewObjectGCKind;
using js::NewObjectKind;
using js::NewObjectWithGivenTaggedProto;
using js::NewObjectWithGroup;
using js::ObjectGroup;
using js::PlainObject;
using js::SingletonObject;
using js::TaggedProto;
using js::TenuredObject;
using js::TypeNewScript;

static PlainObject* CreateThisForFunctionWithGroup(JSContext* cx,
                                                   Handle<ObjectGroup*> group,
                                                   NewObjectKind newKind) {
  TypeNewScript* maybeNewScript;
  {
    AutoSweepObjectGroup sweep(group);
    maybeNewScript = group->newScript(sweep);
  }

  if (maybeNewScript) {
    if (maybeNewScript->analyzed()) {
      // The definite properties analysis has been performed for this
      // group, so get the shape and alloc kind to use from the
      // TypeNewScript's template.
      Rooted<PlainObject*> templateObject(cx, maybeNewScript->templateObject());
      MOZ_ASSERT(templateObject->group() == group);

      Rooted<PlainObject*> res(
          cx, CopyInitializerObject(cx, templateObject, newKind));
      if (!res) {
        return nullptr;
      }

      if (newKind == SingletonObject) {
        Rooted<TaggedProto> proto(
            cx, TaggedProto(templateObject->staticPrototype()));
        if (!JSObject::splicePrototype(cx, res, proto)) {
          return nullptr;
        }
      } else {
        res->setGroup(group);
      }
      return res;
    }

    // The initial objects registered with a TypeNewScript can't be in the
    // nursery.
    if (newKind == GenericObject) {
      newKind = TenuredObject;
    }

    // Not enough objects with this group have been created yet, so make a
    // plain object and register it with the group. Use the maximum number
    // of fixed slots, as is also required by the TypeNewScript.
    js::gc::AllocKind allocKind =
        GuessObjectGCKind(PlainObject::MAX_FIXED_SLOTS);
    PlainObject* res =
        NewObjectWithGroup<PlainObject>(cx, group, allocKind, newKind);
    if (!res) {
      return nullptr;
    }

    // Make sure group->newScript is still there.
    AutoSweepObjectGroup sweep(group);
    if (newKind != SingletonObject && group->newScript(sweep)) {
      group->newScript(sweep)->registerNewObject(res);
    }

    return res;
  }

  js::gc::AllocKind allocKind = NewObjectGCKind(&PlainObject::class_);

  if (newKind == SingletonObject) {
    Rooted<TaggedProto> protoRoot(cx, group->proto());
    return NewObjectWithGivenTaggedProto<PlainObject>(cx, protoRoot, allocKind,
                                                      newKind);
  }
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

    {
      AutoSweepObjectGroup sweep(group);
      if (group->newScript(sweep) && !group->newScript(sweep)->analyzed()) {
        bool regenerate;
        if (!group->newScript(sweep)->maybeAnalyze(cx, group, &regenerate)) {
          return nullptr;
        }
        if (regenerate) {
          // The script was analyzed successfully and may have changed
          // the new type table, so refetch the group.
          group = ObjectGroup::defaultNewGroup(cx, &PlainObject::class_,
                                               TaggedProto(proto), newTarget);
          AutoSweepObjectGroup sweepNewGroup(group);
          MOZ_ASSERT(group);
          MOZ_ASSERT(group->newScript(sweepNewGroup));
        }
      }
    }

    res = CreateThisForFunctionWithGroup(cx, group, newKind);
  } else {
    res = NewBuiltinClassInstanceWithKind<PlainObject>(cx, newKind);
  }

  if (res) {
    MOZ_ASSERT(res->nonCCWRealm() == callee->realm());
    JSScript* script = JSFunction::getOrCreateScript(cx, callee);
    if (!script) {
      return nullptr;
    }
    jit::JitScript::MonitorThisType(cx, script, TypeSet::ObjectType(res));
  }

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

    JSScript* calleeScript = callee->nonLazyScript();
    jit::JitScript::MonitorThisType(cx, calleeScript,
                                    TypeSet::ObjectType(nobj));

    return nobj;
  }

  return obj;
}
