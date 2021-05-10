/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS symbol tables. */

#include "vm/Shape-inl.h"

#include "mozilla/MathAlgorithms.h"
#include "mozilla/PodOperations.h"

#include "gc/FreeOp.h"
#include "gc/HashUtil.h"
#include "gc/Policy.h"
#include "gc/PublicIterators.h"
#include "js/friend/WindowProxy.h"  // js::IsWindow
#include "js/HashTable.h"
#include "js/UniquePtr.h"
#include "util/Text.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"

#include "vm/Caches-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Realm-inl.h"

using namespace js;

using mozilla::CeilingLog2Size;
using mozilla::PodZero;

using JS::AutoCheckCannotGC;

bool ShapeIC::init(JSContext* cx) {
  size_ = MAX_SIZE;
  entries_.reset(cx->pod_calloc<Entry>(size_));
  return (!entries_) ? false : true;
}

bool ShapeTable::init(JSContext* cx, Shape* lastProp) {
  if (!set_.reserve(lastProp->entryCount())) {
    ReportOutOfMemory(cx);
    return false;
  }

  for (Shape::Range<NoGC> r(lastProp); !r.empty(); r.popFront()) {
    Shape& shape = r.front();
    set_.putNewInfallible(shape.propidRaw(), &shape);
  }

  return true;
}

void Shape::removeFromDictionary(NativeObject* obj) {
  MOZ_ASSERT(inDictionary());
  MOZ_ASSERT(obj->inDictionaryMode());

  MOZ_ASSERT(obj->shape()->inDictionary());
  MOZ_ASSERT(obj->shape()->dictNext.isNone());

  // Update parent's next pointer.
  if (parent) {
    parent->setDictionaryNextPtr(dictNext);
  }

  // Update next shape's parent pointer, or change the object's shape if this is
  // the last property.
  if (dictNext.isShape()) {
    dictNext.toShape()->setParent(parent);
  } else {
    MOZ_ASSERT(obj->lastProperty() == this);
    obj->setShape(parent);
  }

  clearDictionaryNextPtr();

  obj->shape()->clearCachedBigEnoughForShapeTable();
}

void Shape::initDictionaryShapeAtEnd(const StackShape& child,
                                     NativeObject* obj) {
  new (this) Shape(child, obj->numFixedSlots());
  this->immutableFlags |= IN_DICTIONARY;

  MOZ_ASSERT(!parent);
  MOZ_ASSERT(dictNext.isNone());

  Shape* prev = obj->lastProperty();
  MOZ_ASSERT(prev->inDictionary());
  MOZ_ASSERT(zone() == prev->zone());
  MOZ_ASSERT(prev->dictNext.isNone());

  this->setParent(prev);
  prev->setNextDictionaryShape(this);
  obj->setShape(this);
}

void Shape::initDictionaryShapeAtFront(const StackShape& child, uint32_t nfixed,
                                       Shape* next) {
  new (this) Shape(child, nfixed);
  this->immutableFlags |= IN_DICTIONARY;

  // Note: this is used by toDictionaryMode where we don't want to change
  // obj->shape until we've allocated all dictionary shapes.

  MOZ_ASSERT(!parent);
  MOZ_ASSERT(dictNext.isNone());

  if (next) {
    MOZ_ASSERT(next->inDictionary());
    MOZ_ASSERT(zone() == next->zone());
    MOZ_ASSERT(!next->parent);
    this->setNextDictionaryShape(next);
    next->setParent(this);
  }
}

void Shape::handoffTableTo(Shape* shape) {
  MOZ_ASSERT(inDictionary() && shape->inDictionary());
  MOZ_ASSERT(this != shape);

  ShapeTable* table = cache_.getTablePointer();
  shape->setTable(table);
  cache_ = ShapeCachePtr();

  // Note: for shape tables only sizeof(ShapeTable) is tracked. See the TODO in
  // Shape::hashify.
  RemoveCellMemory(this, sizeof(ShapeTable), MemoryUse::ShapeCache);
  AddCellMemory(shape, sizeof(ShapeTable), MemoryUse::ShapeCache);
}

/* static */
bool Shape::hashify(JSContext* cx, Shape* shape) {
  MOZ_ASSERT(!shape->hasTable());

  UniquePtr<ShapeTable> table = cx->make_unique<ShapeTable>();
  if (!table) {
    return false;
  }

  if (!table->init(cx, shape)) {
    return false;
  }

  shape->maybePurgeCache(cx->defaultFreeOp());
  shape->setTable(table.release());
  // TODO: The contents of ShapeTable is not currently tracked, only the object
  // itself.
  AddCellMemory(shape, sizeof(ShapeTable), MemoryUse::ShapeCache);
  return true;
}

void ShapeCachePtr::maybePurgeCache(JSFreeOp* fop, Shape* shape) {
  if (isTable()) {
    ShapeTable* table = getTablePointer();
    if (table->freeList() == SHAPE_INVALID_SLOT) {
      fop->delete_(shape, getTablePointer(), MemoryUse::ShapeCache);
      p = 0;
    } else {
      // We can't purge this table because that would lose the slot freeList,
      // but we can compact its HashSet.
      table->compact();
    }
  } else if (isIC()) {
    fop->delete_<ShapeIC>(shape, getICPointer(), MemoryUse::ShapeCache);
    p = 0;
  }
}

/* static */
bool Shape::cachify(JSContext* cx, Shape* shape) {
  MOZ_ASSERT(!shape->hasTable() && !shape->hasIC());

  UniquePtr<ShapeIC> ic = cx->make_unique<ShapeIC>();
  if (!ic) {
    return false;
  }

  if (!ic->init(cx)) {
    return false;
  }

  shape->setIC(ic.release());
  AddCellMemory(shape, sizeof(ShapeIC), MemoryUse::ShapeCache);
  return true;
}

void ShapeCachePtr::trace(JSTracer* trc) {
  if (isIC()) {
    getICPointer()->trace(trc);
  } else if (isTable()) {
    getTablePointer()->trace(trc);
  }
}

void ShapeIC::trace(JSTracer* trc) {
  for (size_t i = 0; i < entryCount(); i++) {
    Entry& entry = entries_[i];
    if (entry.shape_) {
      TraceManuallyBarrieredEdge(trc, &entry.shape_, "ShapeIC shape");
    }
  }
}

void ShapeTable::trace(JSTracer* trc) {
  for (Set::Enum e(set_); !e.empty(); e.popFront()) {
    Shape* shape = e.front();
    MOZ_ASSERT(shape);
    TraceManuallyBarrieredEdge(trc, &shape, "ShapeTable shape");
    if (shape != e.front()) {
      e.mutableFront() = shape;
    }
  }
}

inline void ShapeCachePtr::destroy(JSFreeOp* fop, Shape* shape) {
  if (isTable()) {
    fop->delete_(shape, getTablePointer(), MemoryUse::ShapeCache);
  } else if (isIC()) {
    fop->delete_(shape, getICPointer(), MemoryUse::ShapeCache);
  }
  p = 0;
}

#ifdef JSGC_HASH_TABLE_CHECKS

void ShapeCachePtr::checkAfterMovingGC() {
  if (isIC()) {
    getICPointer()->checkAfterMovingGC();
  } else if (isTable()) {
    getTablePointer()->checkAfterMovingGC();
  }
}

void ShapeIC::checkAfterMovingGC() {
  for (size_t i = 0; i < entryCount(); i++) {
    Entry& entry = entries_[i];
    Shape* shape = entry.shape_;
    if (shape) {
      CheckGCThingAfterMovingGC(shape);
    }
  }
}

void ShapeTable::checkAfterMovingGC() {
  for (Set::Enum e(set_); !e.empty(); e.popFront()) {
    Shape* shape = e.front();
    MOZ_ASSERT(shape);
    CheckGCThingAfterMovingGC(shape);
  }
}

#endif

/* static */
Shape* Shape::replaceLastProperty(JSContext* cx, ObjectFlags objectFlags,
                                  TaggedProto proto, HandleShape shape) {
  MOZ_ASSERT(!shape->inDictionary());

  if (!shape->parent) {
    /* Treat as resetting the initial property of the shape hierarchy. */
    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
    return EmptyShape::getInitialShape(
        cx, shape->getObjectClass(), shape->realm(), proto, kind, objectFlags);
  }

  Rooted<StackShape> child(cx, StackShape(shape));
  child.setObjectFlags(objectFlags);

  if (proto != shape->proto()) {
    Rooted<StackBaseShape> base(
        cx, StackBaseShape(shape->getObjectClass(), shape->realm(), proto));
    BaseShape* nbase = BaseShape::get(cx, base);
    if (!nbase) {
      return nullptr;
    }
    child.setBase(nbase);
  }

  return cx->zone()->propertyTree().getChild(cx, shape->parent, child);
}

/*
 * Get or create a property-tree or dictionary child property of |parent|,
 * which must be lastProperty() if inDictionaryMode(), else parent must be
 * one of lastProperty() or lastProperty()->parent.
 */
/* static */ MOZ_ALWAYS_INLINE Shape* NativeObject::getChildProperty(
    JSContext* cx, HandleNativeObject obj, HandleShape parent,
    MutableHandle<StackShape> child) {
  MOZ_ASSERT(!child.isCustomDataProperty());

  if (child.hasMissingSlot()) {
    uint32_t slot;
    if (obj->inDictionaryMode()) {
      if (!allocDictionarySlot(cx, obj, &slot)) {
        return nullptr;
      }
    } else {
      slot = obj->slotSpan();
      MOZ_ASSERT(slot >= JSSLOT_FREE(obj->getClass()));
      // Objects with many properties are converted to dictionary
      // mode, so we can't overflow SHAPE_MAXIMUM_SLOT here.
      MOZ_ASSERT(slot <
                 JSSLOT_FREE(obj->getClass()) + PropertyTree::MAX_HEIGHT);
      MOZ_ASSERT(slot < SHAPE_MAXIMUM_SLOT);
    }
    child.setSlot(slot);
  } else {
    // If the caller passes in a slot number, it must be either a reserved slot
    // or the slot of a property we're changing. In both cases the slot must be
    // valid for our slot span.
    MOZ_ASSERT(child.slot() < obj->slotSpan());

    /*
     * Slots can only be allocated out of order on objects in
     * dictionary mode.  Otherwise the child's slot must be after the
     * parent's slot (if it has one), because slot number determines
     * slot span for objects with that shape.  Usually child slot
     * *immediately* follows parent slot, but there may be a slot gap
     * when the object uses some -- but not all -- of its reserved
     * slots to store properties.
     */
    MOZ_ASSERT(obj->inDictionaryMode() || parent->hasMissingSlot() ||
               child.slot() == parent->maybeSlot() + 1 ||
               (parent->maybeSlot() + 1 < JSSLOT_FREE(obj->getClass()) &&
                child.slot() == JSSLOT_FREE(obj->getClass())));
  }

  if (obj->inDictionaryMode()) {
    AutoKeepShapeCaches keep(cx);
    ShapeTable* table = obj->lastProperty()->ensureTableForDictionary(cx, keep);
    if (!table) {
      return nullptr;
    }

    MOZ_ASSERT(parent == obj->lastProperty());
    Shape* shape = Allocate<Shape>(cx);
    if (!shape) {
      return nullptr;
    }

    MOZ_ASSERT(child.slot() < obj->slotSpan());

    shape->initDictionaryShapeAtEnd(child, obj);

    if (!table->add(cx, child.propid(), shape)) {
      shape->removeFromDictionary(obj);
      return nullptr;
    }

    // Pass the table along to the new last property.
    MOZ_ASSERT(shape->previous()->maybeTable(keep) == table);
    shape->previous()->handoffTableTo(shape);
    return shape;
  }

  Shape* shape = cx->zone()->propertyTree().inlinedGetChild(cx, parent, child);
  if (!shape) {
    return nullptr;
  }

  MOZ_ASSERT(shape->parent == parent);
  MOZ_ASSERT_IF(parent != obj->lastProperty(),
                parent == obj->lastProperty()->parent);

  if (!obj->setLastProperty(cx, shape)) {
    return nullptr;
  }
  return shape;
}

/* static */ MOZ_ALWAYS_INLINE Shape* NativeObject::getChildCustomDataProperty(
    JSContext* cx, HandleNativeObject obj, HandleShape parent,
    MutableHandle<StackShape> child) {
  MOZ_ASSERT(child.isCustomDataProperty());

  // Custom data properties have no slot, but slot_ will reflect that of parent.
  child.setSlot(parent->maybeSlot());

  if (obj->inDictionaryMode()) {
    AutoKeepShapeCaches keep(cx);
    ShapeTable* table = obj->lastProperty()->ensureTableForDictionary(cx, keep);
    if (!table) {
      return nullptr;
    }

    MOZ_ASSERT(parent == obj->lastProperty());
    Shape* shape = Allocate<Shape>(cx);
    if (!shape) {
      return nullptr;
    }
    shape->initDictionaryShapeAtEnd(child, obj);

    if (!table->add(cx, child.propid(), shape)) {
      shape->removeFromDictionary(obj);
      return nullptr;
    }

    // Pass the table along to the new last property.
    MOZ_ASSERT(shape->previous()->maybeTable(keep) == table);
    shape->previous()->handoffTableTo(shape);
    return shape;
  }

  Shape* shape = cx->zone()->propertyTree().inlinedGetChild(cx, parent, child);
  if (!shape) {
    return nullptr;
  }

  MOZ_ASSERT(shape->parent == parent);
  MOZ_ASSERT_IF(parent != obj->lastProperty(),
                parent == obj->lastProperty()->parent);

  if (!obj->setLastProperty(cx, shape)) {
    return nullptr;
  }
  return shape;
}

/* static */
bool js::NativeObject::toDictionaryMode(JSContext* cx, HandleNativeObject obj) {
  MOZ_ASSERT(!obj->inDictionaryMode());
  MOZ_ASSERT(cx->isInsideCurrentCompartment(obj));

  uint32_t span = obj->slotSpan();

  // Clone the shapes into a new dictionary list. Don't update the last
  // property of this object until done, otherwise a GC triggered while
  // creating the dictionary will get the wrong slot span for this object.
  RootedShape root(cx);
  RootedShape dictionaryShape(cx);

  RootedShape shape(cx, obj->lastProperty());
  while (shape) {
    MOZ_ASSERT(!shape->inDictionary());

    Shape* dprop = Allocate<Shape>(cx);
    if (!dprop) {
      ReportOutOfMemory(cx);
      return false;
    }

    StackShape child(shape);
    dprop->initDictionaryShapeAtFront(child, obj->numFixedSlots(),
                                      dictionaryShape);

    if (!dictionaryShape) {
      root = dprop;
    }

    MOZ_ASSERT(!dprop->hasTable());
    dictionaryShape = dprop;
    shape = shape->previous();
  }

  if (!Shape::hashify(cx, root)) {
    ReportOutOfMemory(cx);
    return false;
  }

  MOZ_ASSERT(root->dictNext.isNone());
  obj->setShape(root);

  MOZ_ASSERT(obj->inDictionaryMode());
  obj->setDictionaryModeSlotSpan(span);

  return true;
}

static bool ShouldConvertToDictionary(NativeObject* obj) {
  /*
   * Use a lower limit if this object is likely a hashmap (SETELEM was used
   * to set properties).
   */
  if (obj->hadElementsAccess()) {
    return obj->lastProperty()->entryCount() >=
           PropertyTree::MAX_HEIGHT_WITH_ELEMENTS_ACCESS;
  }
  return obj->lastProperty()->entryCount() >= PropertyTree::MAX_HEIGHT;
}

namespace js {

class MOZ_RAII AutoCheckShapeConsistency {
#ifdef DEBUG
  HandleNativeObject obj_;
#endif

 public:
  explicit AutoCheckShapeConsistency(HandleNativeObject obj)
#ifdef DEBUG
      : obj_(obj)
#endif
  {
  }

#ifdef DEBUG
  ~AutoCheckShapeConsistency() { obj_->checkShapeConsistency(); }
#endif
};

}  // namespace js

/* static */ MOZ_ALWAYS_INLINE bool
NativeObject::maybeConvertToDictionaryForAdd(JSContext* cx,
                                             HandleNativeObject obj) {
  if (obj->inDictionaryMode()) {
    return true;
  }
  if (!ShouldConvertToDictionary(obj)) {
    return true;
  }
  return toDictionaryMode(cx, obj);
}

static void AssertValidCustomDataProp(NativeObject* obj, unsigned attrs) {
  // We only support custom data properties on ArrayObject and ArgumentsObject.
  // The mechanism is deprecated so we don't want to add new uses.
  MOZ_ASSERT(attrs & JSPROP_CUSTOM_DATA_PROP);
  MOZ_ASSERT(obj->is<ArrayObject>() || obj->is<ArgumentsObject>());
  MOZ_ASSERT((attrs & (JSPROP_GETTER | JSPROP_SETTER)) == 0);
}

/* static */
bool NativeObject::addCustomDataProperty(JSContext* cx, HandleNativeObject obj,
                                         HandleId id, unsigned attrs) {
  MOZ_ASSERT(!JSID_IS_VOID(id));
  MOZ_ASSERT(!id.isPrivateName());
  MOZ_ASSERT(!obj->containsPure(id));

  AutoCheckShapeConsistency check(obj);
  AssertValidCustomDataProp(obj, attrs);

  if (!maybeConvertToDictionaryForAdd(cx, obj)) {
    return false;
  }

  // Find or create a property tree node labeled by our arguments.
  RootedShape shape(cx);
  {
    RootedShape last(cx, obj->lastProperty());
    ObjectFlags objectFlags = GetObjectFlagsForNewProperty(last, id, attrs, cx);

    Rooted<StackShape> child(cx, StackShape(last->base(), objectFlags, id,
                                            SHAPE_INVALID_SLOT, attrs));
    shape = getChildCustomDataProperty(cx, obj, last, &child);
    if (!shape) {
      return false;
    }
  }

  MOZ_ASSERT(shape == obj->lastProperty());

  return true;
}

/* static */
bool NativeObject::addProperty(JSContext* cx, HandleNativeObject obj,
                               HandleId id, uint32_t slot, unsigned attrs,
                               uint32_t* slotOut) {
  AutoCheckShapeConsistency check(obj);
  MOZ_ASSERT(!(attrs & JSPROP_CUSTOM_DATA_PROP),
             "Use addCustomDataProperty for custom data properties");

  // The slot, if any, must be a reserved slot.
  MOZ_ASSERT(slot == SHAPE_INVALID_SLOT ||
             slot < JSCLASS_RESERVED_SLOTS(obj->getClass()));

  // The object must not contain a property named |id|. The object must be
  // extensible, but allow private fields and sparsifying dense elements.
  MOZ_ASSERT(!JSID_IS_VOID(id));
  MOZ_ASSERT(!obj->containsPure(id));
  MOZ_ASSERT_IF(
      !id.isPrivateName(),
      obj->isExtensible() ||
          (JSID_IS_INT(id) && obj->containsDenseElement(JSID_TO_INT(id))));

  if (!maybeConvertToDictionaryForAdd(cx, obj)) {
    return false;
  }

  // Find or create a property tree node labeled by our arguments.
  RootedShape last(cx, obj->lastProperty());
  ObjectFlags objectFlags = GetObjectFlagsForNewProperty(last, id, attrs, cx);

  Rooted<StackShape> child(
      cx, StackShape(last->base(), objectFlags, id, slot, attrs));
  Shape* shape = getChildProperty(cx, obj, last, &child);
  if (!shape) {
    return false;
  }

  MOZ_ASSERT(shape == obj->lastProperty());

  *slotOut = shape->slot();
  return true;
}

static MOZ_ALWAYS_INLINE Shape* PropertyTreeReadBarrier(JSContext* cx,
                                                        Shape* parent,
                                                        Shape* shape) {
  JS::Zone* zone = shape->zone();
  if (zone->needsIncrementalBarrier()) {
    // We need a read barrier for the shape tree, since these are weak
    // pointers.
    ReadBarrier(shape);
    return shape;
  }

  if (MOZ_LIKELY(!zone->isGCSweepingOrCompacting() ||
                 !IsAboutToBeFinalizedUnbarriered(&shape))) {
    if (shape->isMarkedGray()) {
      UnmarkGrayShapeRecursively(shape);
    }
    return shape;
  }

  // The shape we've found is unreachable and due to be finalized, so
  // remove our weak reference to it and don't use it.
  MOZ_ASSERT(parent->isMarkedAny());
  parent->removeChild(cx->defaultFreeOp(), shape);

  return nullptr;
}

/* static */
bool NativeObject::addEnumerableDataProperty(JSContext* cx,
                                             HandleNativeObject obj,
                                             HandleId id, uint32_t* slotOut) {
  // Like addProperty(Internal), but optimized for the common case of adding a
  // new enumerable data property.

  AutoCheckShapeConsistency check(obj);

  constexpr unsigned attrs = JSPROP_ENUMERATE;
  ObjectFlags objectFlags =
      GetObjectFlagsForNewProperty(obj->lastProperty(), id, attrs, cx);

  // Fast path for non-dictionary shapes with a single child.
  do {
    AutoCheckCannotGC nogc;

    Shape* lastProperty = obj->lastProperty();
    if (lastProperty->inDictionary()) {
      break;
    }

    ShapeChildren* childp = &lastProperty->children;
    if (!childp->isSingleShape()) {
      break;
    }

    Shape* child = childp->toSingleShape();
    MOZ_ASSERT(!child->inDictionary());

    if (child->propidRaw() != id || child->objectFlags() != objectFlags ||
        child->attributes() != attrs || child->base() != lastProperty->base()) {
      break;
    }

    MOZ_ASSERT(child->property().isDataProperty());

    child = PropertyTreeReadBarrier(cx, lastProperty, child);
    if (!child) {
      break;
    }

    *slotOut = child->slot();
    return obj->setLastPropertyForNewDataProperty(cx, child);
  } while (0);

  if (!maybeConvertToDictionaryForAdd(cx, obj)) {
    return false;
  }

  /* Find or create a property tree node labeled by our arguments. */

  Shape* shape;
  if (obj->inDictionaryMode()) {
    AutoKeepShapeCaches keep(cx);
    ShapeTable* table = obj->lastProperty()->ensureTableForDictionary(cx, keep);
    if (!table) {
      return false;
    }

    uint32_t slot;
    if (!allocDictionarySlot(cx, obj, &slot)) {
      return false;
    }

    Rooted<StackShape> child(
        cx, StackShape(obj->lastProperty()->base(), objectFlags, id, slot,
                       JSPROP_ENUMERATE));

    shape = Allocate<Shape>(cx);
    if (!shape) {
      return false;
    }

    MOZ_ASSERT(slot < obj->slotSpan());

    shape->initDictionaryShapeAtEnd(child, obj);

    if (!table->add(cx, id, shape)) {
      shape->removeFromDictionary(obj);
      return false;
    }

    // Pass the table along to the new last property.
    MOZ_ASSERT(shape->previous()->maybeTable(keep) == table);
    shape->previous()->handoffTableTo(shape);
  } else {
    uint32_t slot = obj->slotSpan();
    MOZ_ASSERT(slot >= JSSLOT_FREE(obj->getClass()));
    // Objects with many properties are converted to dictionary
    // mode, so we can't overflow SHAPE_MAXIMUM_SLOT here.
    MOZ_ASSERT(slot < JSSLOT_FREE(obj->getClass()) + PropertyTree::MAX_HEIGHT);
    MOZ_ASSERT(slot < SHAPE_MAXIMUM_SLOT);

    Shape* last = obj->lastProperty();
    Rooted<StackShape> child(
        cx, StackShape(last->base(), objectFlags, id, slot, JSPROP_ENUMERATE));
    shape = cx->zone()->propertyTree().inlinedGetChild(cx, last, child);
    if (!shape) {
      return false;
    }
    if (!obj->setLastPropertyForNewDataProperty(cx, shape)) {
      return false;
    }
  }

  MOZ_ASSERT(shape == obj->lastProperty());

  *slotOut = shape->slot();
  return true;
}

/*
 * Assert some invariants that should hold when changing properties. It's the
 * responsibility of the callers to ensure these hold.
 */
static void AssertCanChangeAttrs(Shape* shape, unsigned attrs) {
#ifdef DEBUG
  ShapeProperty prop = shape->property();
  if (prop.configurable()) {
    return;
  }

  // A non-configurable property must stay non-configurable.
  MOZ_ASSERT(attrs & JSPROP_PERMANENT);

  // Reject attempts to turn a non-configurable data property into an accessor
  // or custom data property.
  MOZ_ASSERT_IF(
      prop.isDataProperty(),
      !(attrs & (JSPROP_GETTER | JSPROP_SETTER | JSPROP_CUSTOM_DATA_PROP)));

  // Reject attempts to turn a non-configurable accessor property into a data
  // property or custom data property.
  MOZ_ASSERT_IF(prop.isAccessorProperty(),
                attrs & (JSPROP_GETTER | JSPROP_SETTER));
#endif
}

static void AssertValidArrayIndex(NativeObject* obj, jsid id) {
#ifdef DEBUG
  if (obj->is<ArrayObject>()) {
    ArrayObject* arr = &obj->as<ArrayObject>();
    uint32_t index;
    if (IdIsIndex(id, &index)) {
      MOZ_ASSERT(index < arr->length() || arr->lengthIsWritable());
    }
  }
#endif
}

/* static */
bool NativeObject::maybeToDictionaryModeForChange(JSContext* cx,
                                                  HandleNativeObject obj,
                                                  MutableHandleShape shape) {
  // Overwriting a non-last property requires switching to dictionary mode.
  // The shape tree is shared immutable, and we can't removeProperty and then
  // addAccessorPropertyInternal because a failure under add would lose data.

  if (shape == obj->lastProperty() || obj->inDictionaryMode()) {
    return true;
  }

  if (!toDictionaryMode(cx, obj)) {
    return false;
  }

  AutoCheckCannotGC nogc;
  ShapeTable* table = obj->lastProperty()->maybeTable(nogc);
  MOZ_ASSERT(table);
  shape.set(*table->search(shape->propid(), nogc));
  return true;
}

/* static */
bool NativeObject::changeProperty(JSContext* cx, HandleNativeObject obj,
                                  HandleId id, unsigned attrs,
                                  uint32_t* slotOut) {
  MOZ_ASSERT(!JSID_IS_VOID(id));

  AutoCheckShapeConsistency check(obj);
  AssertValidArrayIndex(obj, id);
  MOZ_ASSERT(!(attrs & JSPROP_CUSTOM_DATA_PROP),
             "Use changeCustomDataPropAttributes for custom data properties");

  RootedShape shape(cx, obj->lastProperty()->search(cx, id));
  MOZ_ASSERT(shape);

  AssertCanChangeAttrs(shape, attrs);

  // If the caller wants to allocate a slot, but doesn't care which slot,
  // copy the existing shape's slot into slot so we can match shape, if all
  // other members match.
  uint32_t slot = shape->hasSlot() ? shape->slot() : SHAPE_INVALID_SLOT;

  ObjectFlags objectFlags =
      GetObjectFlagsForNewProperty(obj->lastProperty(), id, attrs, cx);

  if (shape->property().isAccessorProperty()) {
    objectFlags.setFlag(ObjectFlag::HadGetterSetterChange);
  }

  // Now that we've possibly preserved slot, check whether the property info and
  // object flags match. If so, this is a redundant "change" and we can return
  // without more work.
  if (shape->matchesPropertyParamsAfterId(slot, attrs) &&
      obj->lastProperty()->objectFlags() == objectFlags) {
    MOZ_ASSERT(slot != SHAPE_INVALID_SLOT);
    *slotOut = slot;
    return true;
  }

  if (!maybeToDictionaryModeForChange(cx, obj, &shape)) {
    return false;
  }

  MOZ_ASSERT_IF(shape->hasSlot(), shape->slot() == slot);

  if (obj->inDictionaryMode()) {
    // Updating some property in a dictionary-mode object. Generate a new shape
    // for the last property of the dictionary.
    bool updateLast = (shape == obj->lastProperty());
    if (!NativeObject::generateOwnShape(cx, obj)) {
      return false;
    }

    // Use the newly generated shape when changing the last property.
    if (updateLast) {
      shape = obj->lastProperty();
    }

    if (slot == SHAPE_INVALID_SLOT) {
      if (!allocDictionarySlot(cx, obj, &slot)) {
        return false;
      }
    }

    // Update the last property's object flags. This is fine because we just
    // generated a new shape for the object. Note that |shape|'s object flags
    // aren't used anywhere if it's not the last property.
    obj->lastProperty()->setObjectFlags(objectFlags);

    MOZ_ASSERT(shape->inDictionary());
    shape->setSlot(slot);
    shape->attrs = uint8_t(attrs);
  } else {
    // Updating the last property in a non-dictionary-mode object. Find an
    // alternate shared child of the last property's previous shape.

    MOZ_ASSERT(shape == obj->lastProperty());

    // Find or create a property tree node labeled by our arguments.
    Rooted<StackShape> child(cx, StackShape(obj->lastProperty()->base(),
                                            objectFlags, id, slot, attrs));
    RootedShape parent(cx, shape->parent);
    shape = getChildProperty(cx, obj, parent, &child);
    if (!shape) {
      return false;
    }
  }

  MOZ_ASSERT(obj->lastProperty()->objectFlags() == objectFlags);
  *slotOut = shape->slot();
  return true;
}

/* static */
bool NativeObject::changeCustomDataPropAttributes(JSContext* cx,
                                                  HandleNativeObject obj,
                                                  HandleId id, unsigned attrs) {
  MOZ_ASSERT(!JSID_IS_VOID(id));

  AutoCheckShapeConsistency check(obj);
  AssertValidArrayIndex(obj, id);
  AssertValidCustomDataProp(obj, attrs);

  RootedShape shape(cx, obj->lastProperty()->search(cx, id));
  MOZ_ASSERT(shape);
  MOZ_ASSERT(shape->isCustomDataProperty());

  AssertCanChangeAttrs(shape, attrs);

  ObjectFlags objectFlags =
      GetObjectFlagsForNewProperty(obj->lastProperty(), id, attrs, cx);

  // Check whether the property info and object flags match. If so, this is a
  // redundant "change" and we can return without more work.
  if (shape->matchesPropertyParamsAfterId(SHAPE_INVALID_SLOT, attrs) &&
      obj->lastProperty()->objectFlags() == objectFlags) {
    return true;
  }

  if (!maybeToDictionaryModeForChange(cx, obj, &shape)) {
    return false;
  }

  if (obj->inDictionaryMode()) {
    // Updating some property in a dictionary-mode object. Generate a new shape
    // for the last property of the dictionary.
    bool updateLast = (shape == obj->lastProperty());
    if (!NativeObject::generateOwnShape(cx, obj)) {
      return false;
    }

    // Use the newly generated shape when changing the last property.
    if (updateLast) {
      shape = obj->lastProperty();
    }

    // Update the last property's object flags. This is fine because we just
    // generated a new shape for the object. Note that |shape|'s object flags
    // aren't used anywhere if it's not the last property.
    obj->lastProperty()->setObjectFlags(objectFlags);

    MOZ_ASSERT(shape->inDictionary());
    shape->setSlot(SHAPE_INVALID_SLOT);
    shape->attrs = uint8_t(attrs);
  } else {
    // Updating the last property in a non-dictionary-mode object. Find an
    // alternate shared child of the last property's previous shape.

    MOZ_ASSERT(shape == obj->lastProperty());

    // Find or create a property tree node labeled by our arguments.
    Rooted<StackShape> child(
        cx, StackShape(obj->lastProperty()->base(), objectFlags, id,
                       SHAPE_INVALID_SLOT, attrs));
    RootedShape parent(cx, shape->parent);
    shape = getChildCustomDataProperty(cx, obj, parent, &child);
    if (!shape) {
      return false;
    }
  }

  MOZ_ASSERT(obj->lastProperty()->objectFlags() == objectFlags);

  MOZ_ASSERT(shape->isCustomDataProperty());
  return true;
}

void NativeObject::removeDictionaryPropertyWithoutReshape(ShapeTable* table,
                                                          ShapeTable::Ptr ptr,
                                                          Shape* shape) {
  // Removes a property from a dictionary object. The caller is responsible for
  // generating a new shape for the object.
  //
  // A dictionary-mode object owns mutable, unique shapes on a non-circular
  // doubly linked list, hashed by lastProperty()->table. So we can edit the
  // list and table in place.

  AutoCheckCannotGC nogc;
  MOZ_ASSERT(inDictionaryMode());
  MOZ_ASSERT(lastProperty()->maybeTable(nogc) == table);
  MOZ_ASSERT(*ptr == shape);

  // If shape has a slot, free its slot number.
  if (shape->hasSlot()) {
    freeDictionarySlot(table, shape->slot());
  }

  // Remove shape from its non-circular doubly linked list and from the table.
  bool removingLastProperty = (shape == lastProperty());
  shape->removeFromDictionary(this);
  table->remove(ptr);

  // If we just removed the object's last property, move its ShapeTable,
  // BaseShape and object flags to the new last property. Information in
  // (Base)Shapes for non-last properties may be out of sync with the
  // object's state. Updating the shape's BaseShape is sound because we
  // generate a new shape for the object right after this.
  if (removingLastProperty) {
    MOZ_ASSERT(lastProperty() != shape);
    shape->handoffTableTo(lastProperty());
    lastProperty()->setBase(shape->base());
    lastProperty()->setObjectFlags(shape->objectFlags());
  }
}

/* static */
bool NativeObject::removeProperty(JSContext* cx, HandleNativeObject obj,
                                  HandleId id) {
  AutoCheckShapeConsistency check(obj);

  AutoKeepShapeCaches keep(cx);
  ShapeTable* table;
  ShapeTable::Ptr ptr;
  RootedShape shape(cx);
  if (!Shape::search(cx, obj->lastProperty(), id, keep, shape.address(), &table,
                     &ptr)) {
    return false;
  }

  if (!shape) {
    return true;
  }

  // If we're removing an accessor property, ensure the HadGetterSetterChange
  // object flag is set. This is necessary because the slot holding the
  // GetterSetter can be changed indirectly by removing the property and then
  // adding it back with a different GetterSetter value but the same shape.
  if (shape->property().isAccessorProperty() && !obj->hadGetterSetterChange()) {
    if (!NativeObject::setHadGetterSetterChange(cx, obj)) {
      return false;
    }
    // Relookup shape/table/ptr in case setHadGetterSetterChange changed them.
    if (!Shape::search(cx, obj->lastProperty(), id, keep, shape.address(),
                       &table, &ptr)) {
      return false;
    }
  }

  if (!obj->inDictionaryMode()) {
    if (shape == obj->lastProperty() && obj->canRemoveLastProperty()) {
      // Non-dictionary-mode shape tables are shared immutables, so all we
      // need do is retract the last property and we'll either get (or else
      // lazily make via a later hashify) the exact table for the new property
      // lineage.
      if (shape->hasSlot()) {
        obj->setSlot(shape->slot(), UndefinedValue());
      }
      obj->removeLastProperty(cx);
      return true;
    }

    // Shape is not the last property added, or the last property cannot be
    // removed, so switch to dictionary mode.
    if (!toDictionaryMode(cx, obj)) {
      return false;
    }
    table = obj->lastProperty()->maybeTable(keep);
    MOZ_ASSERT(table);
    ptr = table->search(shape->propid(), keep);
    shape = *ptr;
  }

  MOZ_ASSERT(obj->inDictionaryMode());

  // If in dictionary mode, get a new shape for the last property after the
  // removal. We need a fresh shape for all dictionary deletions, even of
  // the last property. Otherwise, a shape could replay and caches might
  // return deleted DictionaryShapes! See bug 595365. Do this before changing
  // the object or table, so the remaining removal is infallible.
  Shape* spare = Allocate<Shape>(cx);
  if (!spare) {
    return false;
  }
  new (spare) Shape(shape->base(), ObjectFlags(), 0);

  obj->removeDictionaryPropertyWithoutReshape(table, ptr, shape);

  // Generate a new shape for the object, infallibly.
  MOZ_ALWAYS_TRUE(NativeObject::generateOwnShape(cx, obj, spare));
  return true;
}

/* static */
bool NativeObject::densifySparseElements(JSContext* cx,
                                         HandleNativeObject obj) {
  AutoCheckShapeConsistency check(obj);
  MOZ_ASSERT(obj->inDictionaryMode());

  // First allocate a new Shape, because this function needs to be infallible
  // after we start removing properties. See also removeProperty.
  Shape* spare = Allocate<Shape>(cx);
  if (!spare) {
    return false;
  }
  new (spare) Shape(obj->lastProperty()->base(), ObjectFlags(), 0);

  // Convert all sparse elements to dense elements.
  {
    AutoCheckCannotGC nogc;
    ShapeTable* table = obj->lastProperty()->ensureTableForDictionary(cx, nogc);
    if (!table) {
      return false;
    }

    Shape* shape = obj->lastProperty();
    while (!shape->isEmptyShape()) {
      jsid id = shape->propid();
      uint32_t index;
      if (!IdIsIndex(id, &index)) {
        shape = shape->previous();
        continue;
      }

      Value value = obj->getSlot(shape->slot());
      obj->setDenseElement(index, value);

      Shape* previous = shape->previous();
      ShapeTable::Ptr ptr = table->search(id, nogc);
      obj->removeDictionaryPropertyWithoutReshape(table, ptr, shape);
      shape = previous;
    }

    MOZ_ASSERT(obj->lastProperty()->maybeTable(nogc) == table);
  }

  // Generate a new shape for the object, infallibly.
  MOZ_ALWAYS_TRUE(NativeObject::generateOwnShape(cx, obj, spare));
  return true;
}

/* static */
bool NativeObject::generateOwnShape(JSContext* cx, HandleNativeObject obj,
                                    Shape* newShape) {
  if (!obj->inDictionaryMode()) {
    RootedShape newRoot(cx, newShape);
    if (!toDictionaryMode(cx, obj)) {
      return false;
    }
    newShape = newRoot;
  }

  if (!newShape) {
    newShape = Allocate<Shape>(cx);
    if (!newShape) {
      return false;
    }

    new (newShape) Shape(obj->lastProperty()->base(), ObjectFlags(), 0);
  }

  Shape* oldShape = obj->lastProperty();

  AutoCheckCannotGC nogc;
  ShapeTable* table = oldShape->ensureTableForDictionary(cx, nogc);
  if (!table) {
    return false;
  }

  // Replace the old last-property shape with the new one.
  StackShape nshape(oldShape);
  newShape->initDictionaryShapeAtEnd(nshape, obj);

  MOZ_ASSERT(newShape->parent == oldShape);
  oldShape->removeFromDictionary(obj);

  MOZ_ASSERT(newShape == obj->lastProperty());
  oldShape->handoffTableTo(newShape);

  if (!newShape->isEmptyShape()) {
    ShapeTable::Ptr ptr = table->search(oldShape->propidRef(), nogc);
    MOZ_ASSERT(*ptr == oldShape);
    table->replaceShape(ptr, newShape->propidRaw(), newShape);
  }

  return true;
}

/* static */
bool JSObject::setFlag(JSContext* cx, HandleObject obj, ObjectFlag flag,
                       GenerateShape generateShape) {
  MOZ_ASSERT(cx->compartment() == obj->compartment());

  if (obj->hasFlag(flag)) {
    return true;
  }

  if (obj->is<NativeObject>() && obj->as<NativeObject>().inDictionaryMode()) {
    if (generateShape == GENERATE_SHAPE) {
      if (!NativeObject::generateOwnShape(cx, obj.as<NativeObject>())) {
        return false;
      }
    }

    Shape* last = obj->as<NativeObject>().lastProperty();
    ObjectFlags flags = last->objectFlags();
    flags.setFlag(flag);
    last->setObjectFlags(flags);
    return true;
  }

  Shape* newShape = Shape::setObjectFlag(cx, flag, obj->shape());
  if (!newShape) {
    return false;
  }

  obj->setShape(newShape);
  return true;
}

/* static */
bool JSObject::setProtoUnchecked(JSContext* cx, HandleObject obj,
                                 Handle<TaggedProto> proto) {
  MOZ_ASSERT(cx->compartment() == obj->compartment());
  MOZ_ASSERT_IF(proto.isObject(), proto.toObject()->isUsedAsPrototype());

  if (obj->shape()->proto() == proto) {
    return true;
  }

  if (obj->is<NativeObject>() && obj->as<NativeObject>().inDictionaryMode()) {
    Rooted<StackBaseShape> base(
        cx, StackBaseShape(obj->getClass(), obj->nonCCWRealm(), proto));
    Rooted<BaseShape*> nbase(cx, BaseShape::get(cx, base));
    if (!nbase) {
      return false;
    }

    if (!NativeObject::generateOwnShape(cx, obj.as<NativeObject>())) {
      return false;
    }

    Shape* last = obj->as<NativeObject>().lastProperty();
    last->setBase(nbase);
    return true;
  }

  Shape* newShape = Shape::setProto(cx, proto, obj->shape());
  if (!newShape) {
    return false;
  }

  obj->setShape(newShape);
  return true;
}

/* static */
bool NativeObject::clearFlag(JSContext* cx, HandleNativeObject obj,
                             ObjectFlag flag) {
  MOZ_ASSERT(obj->lastProperty()->hasObjectFlag(flag));

  if (!obj->inDictionaryMode()) {
    if (!toDictionaryMode(cx, obj)) {
      return false;
    }
  }

  Shape* last = obj->lastProperty();
  ObjectFlags flags = last->objectFlags();
  flags.clearFlag(flag);
  last->setObjectFlags(flags);
  return true;
}

/* static */
Shape* Shape::setObjectFlag(JSContext* cx, ObjectFlag flag, Shape* last) {
  MOZ_ASSERT(!last->inDictionary());
  MOZ_ASSERT(!last->hasObjectFlag(flag));

  ObjectFlags objectFlags = last->objectFlags();
  objectFlags.setFlag(flag);

  RootedShape lastRoot(cx, last);
  return replaceLastProperty(cx, objectFlags, last->proto(), lastRoot);
}

/* static */
Shape* Shape::setProto(JSContext* cx, TaggedProto proto, Shape* last) {
  MOZ_ASSERT(!last->inDictionary());
  MOZ_ASSERT(last->proto() != proto);

  RootedShape lastRoot(cx, last);
  return replaceLastProperty(cx, last->objectFlags(), proto, lastRoot);
}

inline BaseShape::BaseShape(const StackBaseShape& base)
    : TenuredCellWithNonGCPointer(base.clasp),
      realm_(base.realm),
      proto_(base.proto) {
  MOZ_ASSERT(JS::StringIsASCII(clasp()->name));

  MOZ_ASSERT_IF(proto().isObject(),
                compartment() == proto().toObject()->compartment());
  MOZ_ASSERT_IF(proto().isObject(), proto().toObject()->isUsedAsPrototype());

  // Windows may not appear on prototype chains.
  MOZ_ASSERT_IF(proto().isObject(), !IsWindow(proto().toObject()));

#ifdef DEBUG
  if (GlobalObject* global = realm()->unsafeUnbarrieredMaybeGlobal()) {
    AssertTargetIsNotGray(global);
  }
#endif
}

/* static */
BaseShape* BaseShape::get(JSContext* cx, Handle<StackBaseShape> base) {
  auto& table = cx->zone()->baseShapes();

  auto p = MakeDependentAddPtr(cx, table, base.get());
  if (p) {
    return *p;
  }

  BaseShape* nbase = Allocate<BaseShape>(cx);
  if (!nbase) {
    return nullptr;
  }

  new (nbase) BaseShape(base);

  if (!p.add(cx, table, nbase, nbase)) {
    return nullptr;
  }

  return nbase;
}

#ifdef DEBUG
bool Shape::canSkipMarkingShapeCache() {
  // Check that every shape in the shape table will be marked by marking
  // |lastShape|.
  AutoCheckCannotGC nogc;
  ShapeCachePtr cache = getCache(nogc);
  if (!cache.isTable()) {
    return true;
  }

  uint32_t count = 0;
  for (Shape::Range<NoGC> r(this); !r.empty(); r.popFront()) {
    Shape* shape = &r.front();
    ShapeTable::Ptr p = cache.getTablePointer()->search(shape->propid(), nogc);
    MOZ_ASSERT(*p == shape);
    count++;
  }

  return count == cache.getTablePointer()->entryCount();
}
#endif

#ifdef JSGC_HASH_TABLE_CHECKS

void Zone::checkBaseShapeTableAfterMovingGC() {
  for (auto r = baseShapes().all(); !r.empty(); r.popFront()) {
    BaseShape* base = r.front().unbarrieredGet();
    CheckGCThingAfterMovingGC(base);

    BaseShapeSet::Ptr ptr = baseShapes().lookup(base);
    MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
  }
}

#endif  // JSGC_HASH_TABLE_CHECKS

inline InitialShapeEntry::InitialShapeEntry() : shape(nullptr) {}

inline InitialShapeEntry::InitialShapeEntry(Shape* shape) : shape(shape) {}

#ifdef JSGC_HASH_TABLE_CHECKS

void Zone::checkInitialShapesTableAfterMovingGC() {
  /*
   * Assert that the postbarriers have worked and that nothing is left in
   * initialShapes that points into the nursery, and that the hash table
   * entries are discoverable.
   */
  for (auto r = initialShapes().all(); !r.empty(); r.popFront()) {
    InitialShapeEntry entry = r.front();
    Shape* shape = entry.shape.unbarrieredGet();

    CheckGCThingAfterMovingGC(shape);

    using Lookup = InitialShapeEntry::Lookup;
    Lookup lookup(shape->getObjectClass(), shape->realm(), shape->proto(),
                  shape->numFixedSlots(), shape->objectFlags());
    InitialShapeSet::Ptr ptr = initialShapes().lookup(lookup);
    MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
  }
}

#endif  // JSGC_HASH_TABLE_CHECKS

Shape* EmptyShape::new_(JSContext* cx, Handle<BaseShape*> base,
                        ObjectFlags objectFlags, uint32_t nfixed) {
  Shape* shape = Allocate<Shape>(cx);
  if (!shape) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  new (shape) EmptyShape(base, objectFlags, nfixed);
  return shape;
}

MOZ_ALWAYS_INLINE HashNumber ShapeHasher::hash(const Lookup& l) {
  return l.hash();
}

MOZ_ALWAYS_INLINE bool ShapeHasher::match(const Key k, const Lookup& l) {
  return k->matches(l);
}

static ShapeSet* MakeShapeSet(Shape* child1, Shape* child2) {
  auto hash = MakeUnique<ShapeSet>();
  if (!hash || !hash->reserve(2)) {
    return nullptr;
  }

  hash->putNewInfallible(StackShape(child1), child1);
  hash->putNewInfallible(StackShape(child2), child2);
  return hash.release();
}

bool PropertyTree::insertChild(JSContext* cx, Shape* parent, Shape* child) {
  MOZ_ASSERT(!parent->inDictionary());
  MOZ_ASSERT(!child->parent);
  MOZ_ASSERT(!child->inDictionary());
  MOZ_ASSERT(child->zone() == parent->zone());
  MOZ_ASSERT(cx->zone() == zone_);

  ShapeChildren* childp = &parent->children;

  if (childp->isNone()) {
    child->setParent(parent);
    childp->setSingleShape(child);
    return true;
  }

  if (childp->isSingleShape()) {
    Shape* shape = childp->toSingleShape();
    MOZ_ASSERT(shape != child);
    MOZ_ASSERT(!shape->matches(child));

    ShapeSet* hash = MakeShapeSet(shape, child);
    if (!hash) {
      ReportOutOfMemory(cx);
      return false;
    }
    childp->setShapeSet(hash);
    AddCellMemory(parent, sizeof(ShapeSet), MemoryUse::ShapeChildren);
    child->setParent(parent);
    return true;
  }

  if (!childp->toShapeSet()->putNew(StackShape(child), child)) {
    ReportOutOfMemory(cx);
    return false;
  }

  child->setParent(parent);
  return true;
}

void Shape::removeChild(JSFreeOp* fop, Shape* child) {
  MOZ_ASSERT(!child->inDictionary());
  MOZ_ASSERT(child->parent == this);

  ShapeChildren* childp = &children;

  if (childp->isSingleShape()) {
    MOZ_ASSERT(childp->toSingleShape() == child);
    childp->setNone();
    child->parent = nullptr;
    return;
  }

  // There must be at least two shapes in a set otherwise
  // childp->isSingleShape() should be true.
  ShapeSet* set = childp->toShapeSet();
  MOZ_ASSERT(set->count() >= 2);

#ifdef DEBUG
  size_t oldCount = set->count();
#endif

  set->remove(StackShape(child));
  child->parent = nullptr;

  MOZ_ASSERT(set->count() == oldCount - 1);

  if (set->count() == 1) {
    // Convert from set form back to single shape form.
    ShapeSet::Range r = set->all();
    Shape* otherChild = r.front();
    MOZ_ASSERT((r.popFront(), r.empty()));  // No more elements!
    childp->setSingleShape(otherChild);
    fop->delete_(this, set, MemoryUse::ShapeChildren);
  }
}

MOZ_ALWAYS_INLINE Shape* PropertyTree::inlinedGetChild(
    JSContext* cx, Shape* parent, Handle<StackShape> childSpec) {
  MOZ_ASSERT(parent);

  Shape* existingShape = nullptr;

  /*
   * The property tree has extremely low fan-out below its root in
   * popular embeddings with real-world workloads. Patterns such as
   * defining closures that capture a constructor's environment as
   * getters or setters on the new object that is passed in as
   * |this| can significantly increase fan-out below the property
   * tree root -- see bug 335700 for details.
   */
  ShapeChildren* childp = &parent->children;
  if (childp->isSingleShape()) {
    Shape* child = childp->toSingleShape();
    if (child->matches(childSpec)) {
      existingShape = child;
    }
  } else if (childp->isShapeSet()) {
    if (ShapeSet::Ptr p = childp->toShapeSet()->lookup(childSpec)) {
      existingShape = *p;
    }
  } else {
    /* If childp->isNone(), we always insert. */
  }

  if (existingShape) {
    existingShape = PropertyTreeReadBarrier(cx, parent, existingShape);
    if (existingShape) {
      return existingShape;
    }
  }

  RootedShape parentRoot(cx, parent);
  Shape* shape = Shape::new_(cx, childSpec, parentRoot->numFixedSlots());
  if (!shape) {
    return nullptr;
  }

  if (!insertChild(cx, parentRoot, shape)) {
    return nullptr;
  }

  return shape;
}

Shape* PropertyTree::getChild(JSContext* cx, Shape* parent,
                              Handle<StackShape> child) {
  return inlinedGetChild(cx, parent, child);
}

void Shape::sweep(JSFreeOp* fop) {
  /*
   * We detach the child from the parent if the parent is reachable.
   *
   * This test depends on shape arenas not being freed until after we finish
   * incrementally sweeping them. If that were not the case the parent pointer
   * could point to a marked cell that had been deallocated and then
   * reallocated, since allocating a cell in a zone that is being marked will
   * set the mark bit for that cell.
   */

  MOZ_ASSERT(zone()->isGCSweeping());
  MOZ_ASSERT_IF(parent, parent->zone() == zone());

  if (parent && parent->isMarkedAny()) {
    if (inDictionary()) {
      if (parent->dictNext == DictionaryShapeLink(this)) {
        parent->dictNext.setNone();
      }
    } else {
      parent->removeChild(fop, this);
    }
  }
}

void Shape::finalize(JSFreeOp* fop) {
  if (!inDictionary() && children.isShapeSet()) {
    fop->delete_(this, children.toShapeSet(), MemoryUse::ShapeChildren);
  }
  if (cache_.isInitialized()) {
    cache_.destroy(fop, this);
  }
}

void Shape::fixupDictionaryShapeAfterMovingGC() {
  if (dictNext.isShape()) {
    Shape* shape = dictNext.toShape();
    if (gc::IsForwarded(shape)) {
      dictNext.setShape(gc::Forwarded(shape));
    }
  } else {
    MOZ_ASSERT(dictNext.isNone());
  }
}

void Shape::fixupShapeTreeAfterMovingGC() {
  if (children.isNone()) {
    return;
  }

  if (children.isSingleShape()) {
    if (gc::IsForwarded(children.toSingleShape())) {
      children.setSingleShape(gc::Forwarded(children.toSingleShape()));
    }
    return;
  }

  MOZ_ASSERT(children.isShapeSet());
  ShapeSet* set = children.toShapeSet();
  for (ShapeSet::Enum e(*set); !e.empty(); e.popFront()) {
    Shape* key = MaybeForwarded(e.front());
    BaseShape* base = MaybeForwarded(key->base());

    StackShape lookup(base, key->objectFlags(), key->propidRef(),
                      key->immutableFlags & Shape::SLOT_MASK, key->attrs);
    e.rekeyFront(lookup, key);
  }
}

void Shape::fixupAfterMovingGC() {
  if (inDictionary()) {
    fixupDictionaryShapeAfterMovingGC();
  } else {
    fixupShapeTreeAfterMovingGC();
  }
}

#ifdef DEBUG

void ShapeChildren::checkHasChild(Shape* child) const {
  if (isSingleShape()) {
    MOZ_ASSERT(toSingleShape() == child);
  } else {
    MOZ_ASSERT(isShapeSet());
    ShapeSet* set = toShapeSet();
    ShapeSet::Ptr ptr = set->lookup(StackShape(child));
    MOZ_ASSERT(*ptr == child);
  }
}

void Shape::dump(js::GenericPrinter& out) const {
  jsid propid = this->propid();

  MOZ_ASSERT(!JSID_IS_VOID(propid));

  if (JSID_IS_INT(propid)) {
    out.printf("[%ld]", (long)JSID_TO_INT(propid));
  } else if (JSID_IS_ATOM(propid)) {
    if (JSLinearString* str = propid.toAtom()) {
      EscapedStringPrinter(out, str, '"');
    } else {
      out.put("<error>");
    }
  } else {
    MOZ_ASSERT(JSID_IS_SYMBOL(propid));
    JSID_TO_SYMBOL(propid)->dump(out);
  }

  out.printf(" slot %d attrs %x ", hasSlot() ? int32_t(slot()) : -1, attrs);

  if (attrs) {
    int first = 1;
    out.putChar('(');
#  define DUMP_ATTR(name, display) \
    if (attrs & JSPROP_##name) out.put(&(" " #display)[first]), first = 0
    DUMP_ATTR(ENUMERATE, enumerate);
    DUMP_ATTR(READONLY, readonly);
    DUMP_ATTR(PERMANENT, permanent);
    DUMP_ATTR(GETTER, getter);
    DUMP_ATTR(SETTER, setter);
#  undef DUMP_ATTR
    out.putChar(')');
  }

  out.printf("immutableFlags %x ", immutableFlags);
  if (immutableFlags) {
    int first = 1;
    out.putChar('(');
#  define DUMP_FLAG(name, display) \
    if (immutableFlags & name) out.put(&(" " #display)[first]), first = 0
    DUMP_FLAG(IN_DICTIONARY, in_dictionary);
#  undef DUMP_FLAG
    out.putChar(')');
  }
}

void Shape::dump() const {
  Fprinter out(stderr);
  dump(out);
}

void Shape::dumpSubtree(int level, js::GenericPrinter& out) const {
  if (!parent) {
    MOZ_ASSERT(level == 0);
    MOZ_ASSERT(JSID_IS_EMPTY(propid_));
    out.printf("class %s emptyShape\n", getObjectClass()->name);
  } else {
    out.printf("%*sid ", level, "");
    dump(out);
  }

  if (!children.isNone()) {
    ++level;
    if (children.isSingleShape()) {
      Shape* child = children.toSingleShape();
      MOZ_ASSERT(child->parent == this);
      child->dumpSubtree(level, out);
    } else {
      const ShapeSet& set = *children.toShapeSet();
      for (ShapeSet::Range range = set.all(); !range.empty();
           range.popFront()) {
        Shape* child = range.front();

        MOZ_ASSERT(child->parent == this);
        child->dumpSubtree(level, out);
      }
    }
  }
}

#endif

/* static */
Shape* EmptyShape::getInitialShape(JSContext* cx, const JSClass* clasp,
                                   JS::Realm* realm, TaggedProto proto,
                                   size_t nfixed, ObjectFlags objectFlags) {
  MOZ_ASSERT(cx->compartment() == realm->compartment());
  MOZ_ASSERT_IF(proto.isObject(),
                cx->isInsideCurrentCompartment(proto.toObject()));

  if (proto.isObject() && !proto.toObject()->isUsedAsPrototype()) {
    RootedObject protoObj(cx, proto.toObject());
    if (!JSObject::setIsUsedAsPrototype(cx, protoObj)) {
      return nullptr;
    }
    proto = TaggedProto(protoObj);
  }

  auto& table = realm->zone()->initialShapes();

  using Lookup = InitialShapeEntry::Lookup;
  auto protoPointer = MakeDependentAddPtr(
      cx, table, Lookup(clasp, realm, proto, nfixed, objectFlags));
  if (protoPointer) {
    return protoPointer->shape;
  }

  Rooted<TaggedProto> protoRoot(cx, proto);
  Rooted<StackBaseShape> base(cx, StackBaseShape(clasp, realm, proto));
  Rooted<BaseShape*> nbase(cx, BaseShape::get(cx, base));
  if (!nbase) {
    return nullptr;
  }

  RootedShape shape(cx, EmptyShape::new_(cx, nbase, objectFlags, nfixed));
  if (!shape) {
    return nullptr;
  }

  Lookup lookup(clasp, realm, protoRoot, nfixed, objectFlags);
  if (!protoPointer.add(cx, table, lookup, InitialShapeEntry(shape))) {
    return nullptr;
  }

  return shape;
}

/* static */
Shape* EmptyShape::getInitialShape(JSContext* cx, const JSClass* clasp,
                                   JS::Realm* realm, TaggedProto proto,
                                   gc::AllocKind kind,
                                   ObjectFlags objectFlags) {
  return getInitialShape(cx, clasp, realm, proto, GetGCKindSlots(kind, clasp),
                         objectFlags);
}

void NewObjectCache::invalidateEntriesForShape(Shape* shape) {
  const JSClass* clasp = shape->getObjectClass();

  gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
  if (CanChangeToBackgroundAllocKind(kind, clasp)) {
    kind = ForegroundToBackgroundAllocKind(kind);
  }

  EntryIndex entry;
  for (RealmsInZoneIter realm(shape->zone()); !realm.done(); realm.next()) {
    if (GlobalObject* global = realm->unsafeUnbarrieredMaybeGlobal()) {
      if (lookupGlobal(clasp, global, kind, &entry)) {
        PodZero(&entries[entry]);
      }
    }
  }

  JSObject* proto = shape->proto().toObject();
  if (!proto->is<GlobalObject>() && lookupProto(clasp, proto, kind, &entry)) {
    PodZero(&entries[entry]);
  }
}

/* static */
void EmptyShape::insertInitialShape(JSContext* cx, HandleShape shape) {
  using Lookup = InitialShapeEntry::Lookup;
  Lookup lookup(shape->getObjectClass(), shape->realm(), shape->proto(),
                shape->numFixedSlots(), shape->objectFlags());

  InitialShapeSet::Ptr p = cx->zone()->initialShapes().lookup(lookup);
  MOZ_ASSERT(p);

  InitialShapeEntry& entry = const_cast<InitialShapeEntry&>(*p);

  // The metadata callback can end up causing redundant changes of the initial
  // shape.
  if (entry.shape == shape) {
    return;
  }

  // The new shape had better be rooted at the old one.
#ifdef DEBUG
  Shape* nshape = shape;
  while (!nshape->isEmptyShape()) {
    nshape = nshape->previous();
  }
  MOZ_ASSERT(nshape == entry.shape);
#endif

  entry.shape = WeakHeapPtrShape(shape);

  /*
   * This affects the shape that will be produced by the various NewObject
   * methods, so clear any cache entry referring to the old shape. This is
   * not required for correctness: the NewObject must always check for a
   * nativeEmpty() result and generate the appropriate properties if found.
   * Clearing the cache entry avoids this duplicate regeneration.
   *
   * Clearing is not necessary when this context is running off
   * thread, as it will not use the new object cache for allocations.
   */
  if (!cx->isHelperThreadContext()) {
    cx->caches().newObjectCache.invalidateEntriesForShape(shape);
  }
}

void Zone::fixupInitialShapeTable() {
  for (InitialShapeSet::Enum e(initialShapes()); !e.empty(); e.popFront()) {
    // The shape may have been moved, but we can update that in place.
    Shape* shape = e.front().shape.unbarrieredGet();
    if (IsForwarded(shape)) {
      shape = Forwarded(shape);
      e.mutableFront().shape.set(shape);
    }
    shape->updateBaseShapeAfterMovingGC();
  }
}

JS::ubi::Node::Size JS::ubi::Concrete<js::Shape>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  Size size = js::gc::Arena::thingSize(get().asTenured().getAllocKind());

  AutoCheckCannotGC nogc;
  if (ShapeTable* table = get().maybeTable(nogc)) {
    size += table->sizeOfIncludingThis(mallocSizeOf);
  }

  if (!get().inDictionary() && get().children.isShapeSet()) {
    size +=
        get().children.toShapeSet()->shallowSizeOfIncludingThis(mallocSizeOf);
  }

  return size;
}

JS::ubi::Node::Size JS::ubi::Concrete<js::BaseShape>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return js::gc::Arena::thingSize(get().asTenured().getAllocKind());
}
