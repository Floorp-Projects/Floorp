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

#include "jspubtd.h"  // JSProto_Object

#include "ds/IdValuePair.h"  // js::IdValuePair
#include "gc/AllocKind.h"    // js::gc::AllocKind
#include "vm/JSContext.h"    // JSContext
#include "vm/JSFunction.h"   // JSFunction
#include "vm/JSObject.h"     // JSObject, js::GetPrototypeFromConstructor
#include "vm/TaggedProto.h"  // js::TaggedProto

#include "vm/JSObject-inl.h"  // js::NewObjectWithGroup, js::NewObjectGCKind

using namespace js;

using JS::Handle;
using JS::Rooted;

static MOZ_ALWAYS_INLINE Shape* GetPlainObjectShapeWithProto(
    JSContext* cx, JSObject* proto, gc::AllocKind kind) {
  MOZ_ASSERT(JSCLASS_RESERVED_SLOTS(&PlainObject::class_) == 0,
             "all slots can be used for properties");
  uint32_t nfixed = GetGCKindSlots(kind);
  return SharedShape::getInitialShape(cx, &PlainObject::class_, cx->realm(),
                                      TaggedProto(proto), nfixed);
}

Shape* js::ThisShapeForFunction(JSContext* cx, Handle<JSFunction*> callee,
                                Handle<JSObject*> newTarget) {
  MOZ_ASSERT(cx->realm() == callee->realm());
  MOZ_ASSERT(!callee->constructorNeedsUninitializedThis());

  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromConstructor(cx, newTarget, JSProto_Object, &proto)) {
    return nullptr;
  }

  js::gc::AllocKind allocKind = NewObjectGCKind();

  Shape* res;
  if (proto && proto != cx->global()->maybeGetPrototype(JSProto_Object)) {
    res = GetPlainObjectShapeWithProto(cx, proto, allocKind);
  } else {
    res = GlobalObject::getPlainObjectShapeWithDefaultProto(cx, allocKind);
  }

  MOZ_ASSERT_IF(res, res->realm() == callee->realm());

  return res;
}

#ifdef DEBUG
void PlainObject::assertHasNoNonWritableOrAccessorPropExclProto() const {
  // Check the most recent MaxCount properties to not slow down debug builds too
  // much.
  static constexpr size_t MaxCount = 8;

  size_t count = 0;
  PropertyName* protoName = runtimeFromMainThread()->commonNames->proto;

  for (ShapePropertyIter<NoGC> iter(shape()); !iter.done(); iter++) {
    // __proto__ is always allowed.
    if (iter->key().isAtom(protoName)) {
      continue;
    }

    MOZ_ASSERT(iter->isDataProperty());
    MOZ_ASSERT(iter->writable());

    count++;
    if (count > MaxCount) {
      return;
    }
  }
}
#endif

// static
PlainObject* PlainObject::createWithTemplateFromDifferentRealm(
    JSContext* cx, Handle<PlainObject*> templateObject) {
  MOZ_ASSERT(cx->realm() != templateObject->realm(),
             "Use createWithTemplate() for same-realm objects");

  // Currently only implemented for null-proto.
  MOZ_ASSERT(templateObject->staticPrototype() == nullptr);

  // The object mustn't be in dictionary mode.
  MOZ_ASSERT(!templateObject->shape()->isDictionary());

  TaggedProto proto = TaggedProto(nullptr);
  Shape* templateShape = templateObject->shape();
  Rooted<SharedPropMap*> map(cx, templateShape->propMap()->asShared());

  Rooted<Shape*> shape(
      cx, SharedShape::getInitialOrPropMapShape(
              cx, &PlainObject::class_, cx->realm(), proto,
              templateShape->numFixedSlots(), map,
              templateShape->propMapLength(), templateShape->objectFlags()));
  if (!shape) {
    return nullptr;
  }
  return createWithShape(cx, shape);
}

static bool AddPlainObjectProperties(JSContext* cx, Handle<PlainObject*> obj,
                                     IdValuePair* properties,
                                     size_t nproperties) {
  RootedId propid(cx);
  RootedValue value(cx);

  for (size_t i = 0; i < nproperties; i++) {
    propid = properties[i].id;
    value = properties[i].value;
    if (!NativeDefineDataProperty(cx, obj, propid, value, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  return true;
}

// static
Shape* GlobalObject::createPlainObjectShapeWithDefaultProto(
    JSContext* cx, gc::AllocKind kind) {
  PlainObjectSlotsKind slotsKind = PlainObjectSlotsKindFromAllocKind(kind);
  HeapPtr<Shape*>& shapeRef =
      cx->global()->data().plainObjectShapesWithDefaultProto[slotsKind];
  MOZ_ASSERT(!shapeRef);

  JSObject* proto = GlobalObject::getOrCreatePrototype(cx, JSProto_Object);
  if (!proto) {
    return nullptr;
  }

  Shape* shape = GetPlainObjectShapeWithProto(cx, proto, kind);
  if (!shape) {
    return nullptr;
  }

  shapeRef.init(shape);
  return shape;
}

PlainObject* js::NewPlainObject(JSContext* cx, NewObjectKind newKind) {
  constexpr gc::AllocKind allocKind = gc::AllocKind::OBJECT0;
  MOZ_ASSERT(gc::GetGCObjectKind(&PlainObject::class_) == allocKind);

  Rooted<Shape*> shape(
      cx, GlobalObject::getPlainObjectShapeWithDefaultProto(cx, allocKind));
  if (!shape) {
    return nullptr;
  }

  return PlainObject::createWithShape(cx, shape, allocKind, newKind);
}

PlainObject* js::NewPlainObjectWithAllocKind(JSContext* cx,
                                             gc::AllocKind allocKind,
                                             NewObjectKind newKind) {
  Rooted<Shape*> shape(
      cx, GlobalObject::getPlainObjectShapeWithDefaultProto(cx, allocKind));
  if (!shape) {
    return nullptr;
  }

  return PlainObject::createWithShape(cx, shape, allocKind, newKind);
}

PlainObject* js::NewPlainObjectWithProto(JSContext* cx, HandleObject proto,
                                         NewObjectKind newKind) {
  // Use a faster path if |proto| is %Object.prototype% (the common case).
  if (proto && proto == cx->global()->maybeGetPrototype(JSProto_Object)) {
    return NewPlainObject(cx, newKind);
  }

  constexpr gc::AllocKind allocKind = gc::AllocKind::OBJECT0;
  MOZ_ASSERT(gc::GetGCObjectKind(&PlainObject::class_) == allocKind);

  Rooted<Shape*> shape(cx, GetPlainObjectShapeWithProto(cx, proto, allocKind));
  if (!shape) {
    return nullptr;
  }

  return PlainObject::createWithShape(cx, shape, allocKind, newKind);
}

PlainObject* js::NewPlainObjectWithProtoAndAllocKind(JSContext* cx,
                                                     HandleObject proto,
                                                     gc::AllocKind allocKind,
                                                     NewObjectKind newKind) {
  // Use a faster path if |proto| is %Object.prototype% (the common case).
  if (proto && proto == cx->global()->maybeGetPrototype(JSProto_Object)) {
    return NewPlainObjectWithAllocKind(cx, allocKind, newKind);
  }

  Rooted<Shape*> shape(cx, GetPlainObjectShapeWithProto(cx, proto, allocKind));
  if (!shape) {
    return nullptr;
  }

  return PlainObject::createWithShape(cx, shape, allocKind, newKind);
}

PlainObject* js::NewPlainObjectWithProperties(JSContext* cx,
                                              IdValuePair* properties,
                                              size_t nproperties,
                                              NewObjectKind newKind) {
  gc::AllocKind allocKind = gc::GetGCObjectKind(nproperties);
  Rooted<PlainObject*> obj(cx,
                           NewPlainObjectWithAllocKind(cx, allocKind, newKind));
  if (!obj || !AddPlainObjectProperties(cx, obj, properties, nproperties)) {
    return nullptr;
  }
  return obj;
}
