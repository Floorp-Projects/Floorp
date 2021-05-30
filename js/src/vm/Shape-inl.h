/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Shape_inl_h
#define vm_Shape_inl_h

#include "vm/Shape.h"

#include "gc/Allocator.h"
#include "vm/Interpreter.h"
#include "vm/JSObject.h"
#include "vm/TypedArrayObject.h"

#include "gc/Marking-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSContext-inl.h"

namespace js {

inline AutoKeepShapeCaches::AutoKeepShapeCaches(JSContext* cx)
    : cx_(cx), prev_(cx->zone()->keepShapeCaches()) {
  cx->zone()->setKeepShapeCaches(true);
}

inline AutoKeepShapeCaches::~AutoKeepShapeCaches() {
  cx_->zone()->setKeepShapeCaches(prev_);
}

MOZ_ALWAYS_INLINE Shape* Shape::search(JSContext* cx, jsid id) {
  return search(cx, this, id);
}

MOZ_ALWAYS_INLINE bool Shape::maybeCreateCacheForLookup(JSContext* cx) {
  if (hasTable() || hasIC()) {
    return true;
  }

  if (!inDictionary() && numLinearSearches() < LINEAR_SEARCHES_MAX) {
    incrementNumLinearSearches();
    return true;
  }

  if (!isBigEnoughForAShapeTable()) {
    return true;
  }

  return Shape::cachify(cx, this);
}

/* static */ inline bool Shape::search(JSContext* cx, Shape* start, jsid id,
                                       const AutoKeepShapeCaches& keep,
                                       Shape** pshape, ShapeTable** ptable,
                                       ShapeTable::Ptr* pptr) {
  if (start->inDictionary()) {
    ShapeTable* table = start->ensureTableForDictionary(cx, keep);
    if (!table) {
      return false;
    }
    *ptable = table;
    *pptr = table->search(id, keep);
    *pshape = *pptr ? **pptr : nullptr;
    return true;
  }

  *ptable = nullptr;
  *pptr = ShapeTable::Ptr();
  *pshape = Shape::search(cx, start, id);
  return true;
}

/* static */ MOZ_ALWAYS_INLINE Shape* Shape::search(JSContext* cx, Shape* start,
                                                    jsid id) {
  Shape* foundShape = nullptr;
  if (start->maybeCreateCacheForLookup(cx)) {
    JS::AutoCheckCannotGC nogc;
    ShapeCachePtr cache = start->getCache(nogc);
    if (cache.search(id, start, &foundShape)) {
      return foundShape;
    }
  } else {
    // Just do a linear search.
    cx->recoverFromOutOfMemory();
  }

  foundShape = start->searchLinear(id);
  if (start->hasIC()) {
    JS::AutoCheckCannotGC nogc;
    if (!start->appendShapeToIC(id, foundShape, nogc)) {
      // Failure indicates that the cache is full, which means we missed
      // the cache ShapeIC::MAX_SIZE times. This indicates the cache is no
      // longer useful, so convert it into a ShapeTable.
      if (!Shape::hashify(cx, start)) {
        cx->recoverFromOutOfMemory();
      }
    }
  }
  return foundShape;
}

inline Shape* Shape::new_(JSContext* cx, Handle<StackShape> other,
                          uint32_t nfixed) {
  Shape* shape = js::Allocate<Shape>(cx);
  if (!shape) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return new (shape) Shape(other, nfixed);
}

inline void Shape::setNextDictionaryShape(Shape* shape) {
  setDictionaryNextPtr(DictionaryShapeLink(shape));
}

inline void Shape::clearDictionaryNextPtr() {
  setDictionaryNextPtr(DictionaryShapeLink());
}

inline void Shape::setDictionaryNextPtr(DictionaryShapeLink next) {
  MOZ_ASSERT(inDictionary());
  // Note: we don't need a pre-barrier here because this field isn't traced.
  dictNext = next;
}

template <class ObjectSubclass>
/* static */ inline bool EmptyShape::ensureInitialCustomShape(
    JSContext* cx, Handle<ObjectSubclass*> obj) {
  static_assert(std::is_base_of_v<JSObject, ObjectSubclass>,
                "ObjectSubclass must be a subclass of JSObject");

  // If the provided object has a non-empty shape, it was given the cached
  // initial shape when created: nothing to do.
  if (!obj->empty()) {
    return true;
  }

  // If no initial shape was assigned, do so.
  RootedShape shape(cx, ObjectSubclass::assignInitialShape(cx, obj));
  if (!shape) {
    return false;
  }
  MOZ_ASSERT(!obj->empty());

  // Cache the initial shape, so that future instances will begin life with that
  // shape.
  EmptyShape::insertInitialShape(cx, shape);
  return true;
}

static inline JS::PropertyAttributes GetPropertyAttributes(
    JSObject* obj, PropertyResult prop) {
  MOZ_ASSERT(obj->is<NativeObject>());

  if (prop.isDenseElement()) {
    return obj->as<NativeObject>().getElementsHeader()->elementAttributes();
  }
  if (prop.isTypedArrayElement()) {
    return {JS::PropertyAttribute::Configurable,
            JS::PropertyAttribute::Enumerable, JS::PropertyAttribute::Writable};
  }

  return prop.propertyInfo().propAttributes();
}

/*
 * Keep this function in sync with search. It neither hashifies the start
 * shape nor increments linear search count.
 */
MOZ_ALWAYS_INLINE Shape* Shape::searchNoHashify(Shape* start, jsid id) {
  /*
   * If we have a cache, search in the shape cache, else do a linear
   * search. We never hashify or cachify into a table in parallel.
   */
  Shape* foundShape;
  JS::AutoCheckCannotGC nogc;
  ShapeCachePtr cache = start->getCache(nogc);
  if (!cache.search(id, start, &foundShape)) {
    foundShape = start->searchLinear(id);
  }

  return foundShape;
}

} /* namespace js */

#endif /* vm_Shape_inl_h */
