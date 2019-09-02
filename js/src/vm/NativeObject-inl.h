/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_NativeObject_inl_h
#define vm_NativeObject_inl_h

#include "vm/NativeObject.h"

#include "builtin/TypedObject.h"
#include "gc/Allocator.h"
#include "gc/GCTrace.h"
#include "proxy/Proxy.h"
#include "vm/JSContext.h"
#include "vm/ProxyObject.h"
#include "vm/TypedArrayObject.h"

#include "gc/Heap-inl.h"
#include "gc/Marking-inl.h"
#include "gc/ObjectKind-inl.h"
#include "vm/JSObject-inl.h"

namespace js {

inline uint32_t NativeObject::numFixedSlotsMaybeForwarded() const {
  return gc::MaybeForwarded(lastProperty())->numFixedSlots();
}

inline uint8_t* NativeObject::fixedData(size_t nslots) const {
  MOZ_ASSERT(ClassCanHaveFixedData(getClass()));
  MOZ_ASSERT(nslots == numFixedSlotsMaybeForwarded() + (hasPrivate() ? 1 : 0));
  return reinterpret_cast<uint8_t*>(&fixedSlots()[nslots]);
}

inline void NativeObject::removeLastProperty(JSContext* cx) {
  MOZ_ASSERT(canRemoveLastProperty());
  MOZ_ALWAYS_TRUE(setLastProperty(cx, lastProperty()->previous()));
}

inline bool NativeObject::canRemoveLastProperty() {
  /*
   * Check that the information about the object stored in the last
   * property's base shape is consistent with that stored in the previous
   * shape. If not consistent, then the last property cannot be removed as it
   * will induce a change in the object itself, and the object must be
   * converted to dictionary mode instead. See BaseShape comment in jsscope.h
   */
  MOZ_ASSERT(!inDictionaryMode());
  Shape* previous = lastProperty()->previous().get();
  return previous->getObjectFlags() == lastProperty()->getObjectFlags();
}

inline void NativeObject::setShouldConvertDoubleElements() {
  MOZ_ASSERT(is<ArrayObject>() && !hasEmptyElements());
  getElementsHeader()->setShouldConvertDoubleElements();
}

inline void NativeObject::clearShouldConvertDoubleElements() {
  MOZ_ASSERT(is<ArrayObject>() && !hasEmptyElements());
  getElementsHeader()->clearShouldConvertDoubleElements();
}

inline void NativeObject::addDenseElementType(JSContext* cx, uint32_t index,
                                              const Value& val) {
  // Avoid a slow AddTypePropertyId call if the type is the same as the type
  // of the previous element.
  TypeSet::Type thisType = TypeSet::GetValueType(val);
  if (index == 0 || TypeSet::GetValueType(elements_[index - 1]) != thisType) {
    AddTypePropertyId(cx, this, JSID_VOID, thisType);
  }
}

inline void NativeObject::setDenseElementWithType(JSContext* cx, uint32_t index,
                                                  const Value& val) {
  addDenseElementType(cx, index, val);
  setDenseElementMaybeConvertDouble(index, val);
}

inline void NativeObject::initDenseElementWithType(JSContext* cx,
                                                   uint32_t index,
                                                   const Value& val) {
  MOZ_ASSERT(!shouldConvertDoubleElements());
  MOZ_ASSERT(!val.isMagic(JS_ELEMENTS_HOLE));

  addDenseElementType(cx, index, val);
  initDenseElement(index, val);
}

inline void NativeObject::setDenseElementHole(JSContext* cx, uint32_t index) {
  MarkObjectGroupFlags(cx, this, OBJECT_FLAG_NON_PACKED);
  setDenseElement(index, MagicValue(JS_ELEMENTS_HOLE));
}

inline void NativeObject::removeDenseElementForSparseIndex(JSContext* cx,
                                                           uint32_t index) {
  MOZ_ASSERT(containsPure(INT_TO_JSID(index)));
  MarkObjectGroupFlags(cx, this,
                       OBJECT_FLAG_NON_PACKED | OBJECT_FLAG_SPARSE_INDEXES);
  if (containsDenseElement(index)) {
    setDenseElement(index, MagicValue(JS_ELEMENTS_HOLE));
  }
}

inline bool NativeObject::writeToIndexWouldMarkNotPacked(uint32_t index) {
  return getElementsHeader()->initializedLength < index;
}

inline void NativeObject::markDenseElementsNotPacked(JSContext* cx) {
  MOZ_ASSERT(isNative());
  MarkObjectGroupFlags(cx, this, OBJECT_FLAG_NON_PACKED);
}

inline void NativeObject::elementsRangeWriteBarrierPost(uint32_t start,
                                                        uint32_t count) {
  for (size_t i = 0; i < count; i++) {
    const Value& v = elements_[start + i];
    if (v.isGCThing()) {
      if (gc::StoreBuffer* sb = v.toGCThing()->storeBuffer()) {
        sb->putSlot(this, HeapSlot::Element, unshiftedIndex(start + i),
                    count - i);
        return;
      }
    }
  }
}

inline void NativeObject::copyDenseElements(uint32_t dstStart, const Value* src,
                                            uint32_t count) {
  MOZ_ASSERT(dstStart + count <= getDenseCapacity());
  MOZ_ASSERT(!denseElementsAreCopyOnWrite());
  MOZ_ASSERT(isExtensible());
  MOZ_ASSERT_IF(count > 0, src != nullptr);
#ifdef DEBUG
  for (uint32_t i = 0; i < count; ++i) {
    checkStoredValue(src[i]);
  }
#endif
  if (count == 0) {
    return;
  }
  if (zone()->needsIncrementalBarrier()) {
    uint32_t numShifted = getElementsHeader()->numShiftedElements();
    for (uint32_t i = 0; i < count; ++i) {
      elements_[dstStart + i].set(this, HeapSlot::Element,
                                  dstStart + i + numShifted, src[i]);
    }
  } else {
    memcpy(reinterpret_cast<Value*>(&elements_[dstStart]), src,
           count * sizeof(Value));
    elementsRangeWriteBarrierPost(dstStart, count);
  }
}

inline void NativeObject::initDenseElements(NativeObject* src,
                                            uint32_t srcStart, uint32_t count) {
  MOZ_ASSERT(src->getDenseInitializedLength() >= srcStart + count);

  const Value* vp = src->getDenseElements() + srcStart;
  initDenseElements(vp, count);
}

inline void NativeObject::initDenseElements(const Value* src, uint32_t count) {
  MOZ_ASSERT(getDenseInitializedLength() == 0);
  MOZ_ASSERT(count <= getDenseCapacity());
  MOZ_ASSERT(!denseElementsAreCopyOnWrite());
  MOZ_ASSERT(isExtensible());

  setDenseInitializedLength(count);

#ifdef DEBUG
  for (uint32_t i = 0; i < count; ++i) {
    checkStoredValue(src[i]);
  }
#endif

  memcpy(reinterpret_cast<Value*>(elements_), src, count * sizeof(Value));
  elementsRangeWriteBarrierPost(0, count);
}

inline bool NativeObject::tryShiftDenseElements(uint32_t count) {
  MOZ_ASSERT(isExtensible());

  ObjectElements* header = getElementsHeader();
  if (header->initializedLength == count ||
      count > ObjectElements::MaxShiftedElements || header->isCopyOnWrite() ||
      header->hasNonwritableArrayLength()) {
    return false;
  }

  shiftDenseElementsUnchecked(count);
  return true;
}

inline void NativeObject::shiftDenseElementsUnchecked(uint32_t count) {
  MOZ_ASSERT(isExtensible());

  ObjectElements* header = getElementsHeader();
  MOZ_ASSERT(count > 0);
  MOZ_ASSERT(count < header->initializedLength);

  if (MOZ_UNLIKELY(header->numShiftedElements() + count >
                   ObjectElements::MaxShiftedElements)) {
    moveShiftedElements();
    header = getElementsHeader();
  }

  prepareElementRangeForOverwrite(0, count);
  header->addShiftedElements(count);

  elements_ += count;
  ObjectElements* newHeader = getElementsHeader();
  memmove(newHeader, header, sizeof(ObjectElements));
}

inline void NativeObject::moveDenseElements(uint32_t dstStart,
                                            uint32_t srcStart, uint32_t count) {
  MOZ_ASSERT(dstStart + count <= getDenseCapacity());
  MOZ_ASSERT(srcStart + count <= getDenseInitializedLength());
  MOZ_ASSERT(!denseElementsAreCopyOnWrite());
  MOZ_ASSERT(isExtensible());

  /*
   * Using memmove here would skip write barriers. Also, we need to consider
   * an array containing [A, B, C], in the following situation:
   *
   * 1. Incremental GC marks slot 0 of array (i.e., A), then returns to JS code.
   * 2. JS code moves slots 1..2 into slots 0..1, so it contains [B, C, C].
   * 3. Incremental GC finishes by marking slots 1 and 2 (i.e., C).
   *
   * Since normal marking never happens on B, it is very important that the
   * write barrier is invoked here on B, despite the fact that it exists in
   * the array before and after the move.
   */
  if (zone()->needsIncrementalBarrier()) {
    uint32_t numShifted = getElementsHeader()->numShiftedElements();
    if (dstStart < srcStart) {
      HeapSlot* dst = elements_ + dstStart;
      HeapSlot* src = elements_ + srcStart;
      for (uint32_t i = 0; i < count; i++, dst++, src++) {
        dst->set(this, HeapSlot::Element, dst - elements_ + numShifted, *src);
      }
    } else {
      HeapSlot* dst = elements_ + dstStart + count - 1;
      HeapSlot* src = elements_ + srcStart + count - 1;
      for (uint32_t i = 0; i < count; i++, dst--, src--) {
        dst->set(this, HeapSlot::Element, dst - elements_ + numShifted, *src);
      }
    }
  } else {
    memmove(elements_ + dstStart, elements_ + srcStart,
            count * sizeof(HeapSlot));
    elementsRangeWriteBarrierPost(dstStart, count);
  }
}

inline void NativeObject::moveDenseElementsNoPreBarrier(uint32_t dstStart,
                                                        uint32_t srcStart,
                                                        uint32_t count) {
  MOZ_ASSERT(!zone()->needsIncrementalBarrier());

  MOZ_ASSERT(dstStart + count <= getDenseCapacity());
  MOZ_ASSERT(srcStart + count <= getDenseCapacity());
  MOZ_ASSERT(!denseElementsAreCopyOnWrite());
  MOZ_ASSERT(isExtensible());

  memmove(elements_ + dstStart, elements_ + srcStart, count * sizeof(HeapSlot));
  elementsRangeWriteBarrierPost(dstStart, count);
}

inline void NativeObject::reverseDenseElementsNoPreBarrier(uint32_t length) {
  MOZ_ASSERT(!zone()->needsIncrementalBarrier());

  MOZ_ASSERT(!denseElementsAreCopyOnWrite());
  MOZ_ASSERT(isExtensible());

  MOZ_ASSERT(length > 1);
  MOZ_ASSERT(length <= getDenseInitializedLength());

  Value* valLo = reinterpret_cast<Value*>(elements_);
  Value* valHi = valLo + (length - 1);
  MOZ_ASSERT(valLo < valHi);

  do {
    Value origLo = *valLo;
    *valLo = *valHi;
    *valHi = origLo;
    ++valLo;
    --valHi;
  } while (valLo < valHi);

  elementsRangeWriteBarrierPost(0, length);
}

inline void NativeObject::ensureDenseInitializedLengthNoPackedCheck(
    uint32_t index, uint32_t extra) {
  MOZ_ASSERT(!denseElementsAreCopyOnWrite());
  MOZ_ASSERT(!denseElementsAreFrozen());
  MOZ_ASSERT(isExtensible() || (containsDenseElement(index) && extra == 1));

  /*
   * Ensure that the array's contents have been initialized up to index, and
   * mark the elements through 'index + extra' as initialized in preparation
   * for a write.
   */
  MOZ_ASSERT(index + extra <= getDenseCapacity());
  uint32_t& initlen = getElementsHeader()->initializedLength;

  if (initlen < index + extra) {
    MOZ_ASSERT(isExtensible());
    uint32_t numShifted = getElementsHeader()->numShiftedElements();
    size_t offset = initlen;
    for (HeapSlot* sp = elements_ + initlen; sp != elements_ + (index + extra);
         sp++, offset++) {
      sp->init(this, HeapSlot::Element, offset + numShifted,
               MagicValue(JS_ELEMENTS_HOLE));
    }
    initlen = index + extra;
  }
}

inline void NativeObject::ensureDenseInitializedLength(JSContext* cx,
                                                       uint32_t index,
                                                       uint32_t extra) {
  MOZ_ASSERT(isExtensible());

  if (writeToIndexWouldMarkNotPacked(index)) {
    markDenseElementsNotPacked(cx);
  }
  ensureDenseInitializedLengthNoPackedCheck(index, extra);
}

DenseElementResult NativeObject::extendDenseElements(JSContext* cx,
                                                     uint32_t requiredCapacity,
                                                     uint32_t extra) {
  MOZ_ASSERT(!denseElementsAreCopyOnWrite());
  MOZ_ASSERT(isExtensible());

  /*
   * Don't grow elements for objects which already have sparse indexes.
   * This avoids needing to count non-hole elements in willBeSparseElements
   * every time a new index is added.
   */
  if (isIndexed()) {
    return DenseElementResult::Incomplete;
  }

  /*
   * We use the extra argument also as a hint about number of non-hole
   * elements to be inserted.
   */
  if (requiredCapacity > MIN_SPARSE_INDEX &&
      willBeSparseElements(requiredCapacity, extra)) {
    return DenseElementResult::Incomplete;
  }

  if (!growElements(cx, requiredCapacity)) {
    return DenseElementResult::Failure;
  }

  return DenseElementResult::Success;
}

inline DenseElementResult NativeObject::ensureDenseElements(JSContext* cx,
                                                            uint32_t index,
                                                            uint32_t extra) {
  MOZ_ASSERT(isNative());
  MOZ_ASSERT(isExtensible() || (containsDenseElement(index) && extra == 1));

  if (writeToIndexWouldMarkNotPacked(index)) {
    markDenseElementsNotPacked(cx);
  }

  if (!maybeCopyElementsForWrite(cx)) {
    return DenseElementResult::Failure;
  }

  uint32_t currentCapacity = getDenseCapacity();

  uint32_t requiredCapacity;
  if (extra == 1) {
    /* Optimize for the common case. */
    if (index < currentCapacity) {
      ensureDenseInitializedLengthNoPackedCheck(index, 1);
      return DenseElementResult::Success;
    }
    requiredCapacity = index + 1;
    if (requiredCapacity == 0) {
      /* Overflow. */
      return DenseElementResult::Incomplete;
    }
  } else {
    requiredCapacity = index + extra;
    if (requiredCapacity < index) {
      /* Overflow. */
      return DenseElementResult::Incomplete;
    }
    if (requiredCapacity <= currentCapacity) {
      ensureDenseInitializedLengthNoPackedCheck(index, extra);
      return DenseElementResult::Success;
    }
  }

  DenseElementResult result = extendDenseElements(cx, requiredCapacity, extra);
  if (result != DenseElementResult::Success) {
    return result;
  }

  ensureDenseInitializedLengthNoPackedCheck(index, extra);
  return DenseElementResult::Success;
}

inline DenseElementResult NativeObject::setOrExtendDenseElements(
    JSContext* cx, uint32_t start, const Value* vp, uint32_t count,
    ShouldUpdateTypes updateTypes) {
  if (!isExtensible()) {
    return DenseElementResult::Incomplete;
  }

  if (is<ArrayObject>() && !as<ArrayObject>().lengthIsWritable() &&
      start + count >= as<ArrayObject>().length()) {
    return DenseElementResult::Incomplete;
  }

  DenseElementResult result = ensureDenseElements(cx, start, count);
  if (result != DenseElementResult::Success) {
    return result;
  }

  if (is<ArrayObject>() && start + count >= as<ArrayObject>().length()) {
    as<ArrayObject>().setLengthInt32(start + count);
  }

  if (updateTypes == ShouldUpdateTypes::DontUpdate &&
      !shouldConvertDoubleElements()) {
    copyDenseElements(start, vp, count);
  } else {
    for (size_t i = 0; i < count; i++) {
      setDenseElementWithType(cx, start + i, vp[i]);
    }
  }

  return DenseElementResult::Success;
}

template <AllowGC allowGC>
inline bool NativeObject::getDenseOrTypedArrayElement(
    JSContext* cx, uint32_t idx,
    typename MaybeRooted<Value, allowGC>::MutableHandleType val) {
  if (is<TypedArrayObject>()) {
    return as<TypedArrayObject>().getElement<allowGC>(cx, idx, val);
  }
  val.set(getDenseElement(idx));
  return true;
}

MOZ_ALWAYS_INLINE void NativeObject::setSlotWithType(JSContext* cx,
                                                     Shape* shape,
                                                     const Value& value,
                                                     bool overwriting) {
  setSlot(shape->slot(), value);

  if (overwriting) {
    shape->setOverwritten();
  }

  AddTypePropertyId(cx, this, shape->propid(), value);
}

inline bool NativeObject::isInWholeCellBuffer() const {
  const gc::TenuredCell* cell = &asTenured();
  gc::ArenaCellSet* cells = cell->arena()->bufferedCells();
  return cells && cells->hasCell(cell);
}

/* static */ inline JS::Result<NativeObject*, JS::OOM&> NativeObject::create(
    JSContext* cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
    js::HandleShape shape, js::HandleObjectGroup group) {
  debugCheckNewObject(group, shape, kind, heap);

  const JSClass* clasp = group->clasp();
  MOZ_ASSERT(clasp->isNative());
  MOZ_ASSERT(!clasp->isJSFunction(), "should use JSFunction::create");

  size_t nDynamicSlots =
      dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan(), clasp);

  JSObject* obj = js::AllocateObject(cx, kind, nDynamicSlots, heap, clasp);
  if (!obj) {
    return cx->alreadyReportedOOM();
  }

  NativeObject* nobj = static_cast<NativeObject*>(obj);
  nobj->initGroup(group);
  nobj->initShape(shape);
  // NOTE: Dynamic slots are created internally by Allocate<JSObject>.
  if (!nDynamicSlots) {
    nobj->initSlots(nullptr);
  }
  nobj->setEmptyElements();

  if (clasp->hasPrivate()) {
    nobj->initPrivate(nullptr);
  }

  if (size_t span = shape->slotSpan()) {
    nobj->initializeSlotRange(0, span);
  }

  if (clasp->shouldDelayMetadataBuilder()) {
    cx->realm()->setObjectPendingMetadata(cx, nobj);
  } else {
    nobj = SetNewObjectMetadata(cx, nobj);
  }

  js::gc::gcTracer.traceCreateObject(nobj);

  return nobj;
}

/* static */ inline JS::Result<NativeObject*, JS::OOM&>
NativeObject::createWithTemplate(JSContext* cx, HandleObject templateObject) {
  RootedObjectGroup group(cx, templateObject->group());

  gc::InitialHeap heap = GetInitialHeap(GenericObject, group);

  RootedShape shape(cx, templateObject->as<NativeObject>().lastProperty());

  gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
  MOZ_ASSERT(CanChangeToBackgroundAllocKind(kind, shape->getObjectClass()));
  kind = gc::ForegroundToBackgroundAllocKind(kind);

  return create(cx, kind, heap, shape, group);
}

MOZ_ALWAYS_INLINE bool NativeObject::updateSlotsForSpan(JSContext* cx,
                                                        size_t oldSpan,
                                                        size_t newSpan) {
  MOZ_ASSERT(oldSpan != newSpan);

  size_t oldCount = dynamicSlotsCount(numFixedSlots(), oldSpan, getClass());
  size_t newCount = dynamicSlotsCount(numFixedSlots(), newSpan, getClass());

  if (oldSpan < newSpan) {
    if (oldCount < newCount && !growSlots(cx, oldCount, newCount)) {
      return false;
    }

    if (newSpan == oldSpan + 1) {
      initSlotUnchecked(oldSpan, UndefinedValue());
    } else {
      initializeSlotRange(oldSpan, newSpan - oldSpan);
    }
  } else {
    /* Trigger write barriers on the old slots before reallocating. */
    prepareSlotRangeForOverwrite(newSpan, oldSpan);
    invalidateSlotRange(newSpan, oldSpan - newSpan);

    if (oldCount > newCount) {
      shrinkSlots(cx, oldCount, newCount);
    }
  }

  return true;
}

MOZ_ALWAYS_INLINE bool NativeObject::setLastProperty(JSContext* cx,
                                                     Shape* shape) {
  MOZ_ASSERT(!inDictionaryMode());
  MOZ_ASSERT(!shape->inDictionary());
  MOZ_ASSERT(shape->zone() == zone());
  MOZ_ASSERT(shape->numFixedSlots() == numFixedSlots());
  MOZ_ASSERT(shape->getObjectClass() == getClass());

  size_t oldSpan = lastProperty()->slotSpan();
  size_t newSpan = shape->slotSpan();

  if (oldSpan == newSpan) {
    setShape(shape);
    return true;
  }

  if (MOZ_UNLIKELY(!updateSlotsForSpan(cx, oldSpan, newSpan))) {
    return false;
  }

  setShape(shape);
  return true;
}

inline js::gc::AllocKind NativeObject::allocKindForTenure() const {
  using namespace js::gc;
  AllocKind kind = GetGCObjectFixedSlotsKind(numFixedSlots());
  MOZ_ASSERT(!IsBackgroundFinalized(kind));
  if (!CanChangeToBackgroundAllocKind(kind, getClass())) {
    return kind;
  }
  return ForegroundToBackgroundAllocKind(kind);
}

inline js::GlobalObject& NativeObject::global() const { return nonCCWGlobal(); }

inline js::gc::AllocKind PlainObject::allocKindForTenure() const {
  using namespace js::gc;
  AllocKind kind = GetGCObjectFixedSlotsKind(numFixedSlots());
  MOZ_ASSERT(!IsBackgroundFinalized(kind));
  MOZ_ASSERT(CanChangeToBackgroundAllocKind(kind, getClass()));
  return ForegroundToBackgroundAllocKind(kind);
}

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline PlainObject* CopyInitializerObject(
    JSContext* cx, HandlePlainObject baseobj,
    NewObjectKind newKind = GenericObject) {
  MOZ_ASSERT(!baseobj->inDictionaryMode());

  gc::AllocKind allocKind =
      gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
  allocKind = gc::ForegroundToBackgroundAllocKind(allocKind);
  MOZ_ASSERT_IF(baseobj->isTenured(),
                allocKind == baseobj->asTenured().getAllocKind());
  RootedPlainObject obj(
      cx, NewBuiltinClassInstance<PlainObject>(cx, allocKind, newKind));
  if (!obj) {
    return nullptr;
  }

  if (!obj->setLastProperty(cx, baseobj->lastProperty())) {
    return nullptr;
  }

  return obj;
}

inline NativeObject* NewNativeObjectWithGivenTaggedProto(
    JSContext* cx, const JSClass* clasp, Handle<TaggedProto> proto,
    gc::AllocKind allocKind, NewObjectKind newKind) {
  return MaybeNativeObject(
      NewObjectWithGivenTaggedProto(cx, clasp, proto, allocKind, newKind));
}

inline NativeObject* NewNativeObjectWithGivenTaggedProto(
    JSContext* cx, const JSClass* clasp, Handle<TaggedProto> proto,
    NewObjectKind newKind = GenericObject) {
  return MaybeNativeObject(
      NewObjectWithGivenTaggedProto(cx, clasp, proto, newKind));
}

inline NativeObject* NewNativeObjectWithGivenProto(JSContext* cx,
                                                   const JSClass* clasp,
                                                   HandleObject proto,
                                                   gc::AllocKind allocKind,
                                                   NewObjectKind newKind) {
  return MaybeNativeObject(
      NewObjectWithGivenProto(cx, clasp, proto, allocKind, newKind));
}

inline NativeObject* NewNativeObjectWithGivenProto(
    JSContext* cx, const JSClass* clasp, HandleObject proto,
    NewObjectKind newKind = GenericObject) {
  return MaybeNativeObject(NewObjectWithGivenProto(cx, clasp, proto, newKind));
}

inline NativeObject* NewNativeObjectWithClassProto(
    JSContext* cx, const JSClass* clasp, HandleObject proto,
    gc::AllocKind allocKind, NewObjectKind newKind = GenericObject) {
  return MaybeNativeObject(
      NewObjectWithClassProto(cx, clasp, proto, allocKind, newKind));
}

inline NativeObject* NewNativeObjectWithClassProto(
    JSContext* cx, const JSClass* clasp, HandleObject proto,
    NewObjectKind newKind = GenericObject) {
  return MaybeNativeObject(NewObjectWithClassProto(cx, clasp, proto, newKind));
}

/*
 * Call obj's resolve hook.
 *
 * cx and id are the parameters initially passed to the ongoing lookup;
 * propp and recursedp are its out parameters.
 *
 * There are four possible outcomes:
 *
 *   - On failure, report an error or exception and return false.
 *
 *   - If we are already resolving a property of obj, set *recursedp = true,
 *     and return true.
 *
 *   - If the resolve hook finds or defines the sought property, set propp
 *      appropriately, set *recursedp = false, and return true.
 *
 *   - Otherwise no property was resolved. Set propp to nullptr and
 *     *recursedp = false and return true.
 */
static MOZ_ALWAYS_INLINE bool CallResolveOp(JSContext* cx,
                                            HandleNativeObject obj, HandleId id,
                                            MutableHandle<PropertyResult> propp,
                                            bool* recursedp) {
  // Avoid recursion on (obj, id) already being resolved on cx.
  AutoResolving resolving(cx, obj, id);
  if (resolving.alreadyStarted()) {
    // Already resolving id in obj, suppress recursion.
    *recursedp = true;
    return true;
  }
  *recursedp = false;

  bool resolved = false;
  AutoRealm ar(cx, obj);
  if (!obj->getClass()->getResolve()(cx, obj, id, &resolved)) {
    return false;
  }

  if (!resolved) {
    return true;
  }

  // Assert the mayResolve hook, if there is one, returns true for this
  // property.
  MOZ_ASSERT_IF(obj->getClass()->getMayResolve(),
                obj->getClass()->getMayResolve()(cx->names(), id, obj));

  if (JSID_IS_INT(id) && obj->containsDenseElement(JSID_TO_INT(id))) {
    propp.setDenseOrTypedArrayElement();
    return true;
  }

  MOZ_ASSERT(!obj->is<TypedArrayObject>());

  RootedShape shape(cx, obj->lookup(cx, id));
  if (shape) {
    propp.setNativeProperty(shape);
  } else {
    propp.setNotFound();
  }

  return true;
}

template <AllowGC allowGC>
static MOZ_ALWAYS_INLINE bool LookupOwnPropertyInline(
    JSContext* cx, typename MaybeRooted<NativeObject*, allowGC>::HandleType obj,
    typename MaybeRooted<jsid, allowGC>::HandleType id,
    typename MaybeRooted<PropertyResult, allowGC>::MutableHandleType propp,
    bool* donep) {
  // Check for a native dense element.
  if (JSID_IS_INT(id) && obj->containsDenseElement(JSID_TO_INT(id))) {
    propp.setDenseOrTypedArrayElement();
    *donep = true;
    return true;
  }

  // Check for a typed array element. Integer lookups always finish here
  // so that integer properties on the prototype are ignored even for out
  // of bounds accesses.
  if (obj->template is<TypedArrayObject>()) {
    uint64_t index;
    if (IsTypedArrayIndex(id, &index)) {
      if (index < obj->template as<TypedArrayObject>().length()) {
        propp.setDenseOrTypedArrayElement();
      } else {
        propp.setNotFound();
      }
      *donep = true;
      return true;
    }
  }

  // Check for a native property. Call Shape::search directly (instead of
  // NativeObject::lookup) because it's inlined.
  if (Shape* shape = obj->lastProperty()->search(cx, id)) {
    propp.setNativeProperty(shape);
    *donep = true;
    return true;
  }

  // id was not found in obj. Try obj's resolve hook, if any.
  if (obj->getClass()->getResolve()) {
    MOZ_ASSERT(!cx->isHelperThreadContext());
    if (!allowGC) {
      return false;
    }

    bool recursed;
    if (!CallResolveOp(
            cx, MaybeRooted<NativeObject*, allowGC>::toHandle(obj),
            MaybeRooted<jsid, allowGC>::toHandle(id),
            MaybeRooted<PropertyResult, allowGC>::toMutableHandle(propp),
            &recursed)) {
      return false;
    }

    if (recursed) {
      propp.setNotFound();
      *donep = true;
      return true;
    }

    if (propp) {
      *donep = true;
      return true;
    }
  }

  propp.setNotFound();
  *donep = false;
  return true;
}

/*
 * Simplified version of LookupOwnPropertyInline that doesn't call resolve
 * hooks.
 */
static inline void NativeLookupOwnPropertyNoResolve(
    JSContext* cx, HandleNativeObject obj, HandleId id,
    MutableHandle<PropertyResult> result) {
  // Check for a native dense element.
  if (JSID_IS_INT(id) && obj->containsDenseElement(JSID_TO_INT(id))) {
    result.setDenseOrTypedArrayElement();
    return;
  }

  // Check for a typed array element.
  if (obj->is<TypedArrayObject>()) {
    uint64_t index;
    if (IsTypedArrayIndex(id, &index)) {
      if (index < obj->as<TypedArrayObject>().length()) {
        result.setDenseOrTypedArrayElement();
      } else {
        result.setNotFound();
      }
      return;
    }
  }

  // Check for a native property.
  if (Shape* shape = obj->lookup(cx, id)) {
    result.setNativeProperty(shape);
  } else {
    result.setNotFound();
  }
}

template <AllowGC allowGC>
static MOZ_ALWAYS_INLINE bool LookupPropertyInline(
    JSContext* cx, typename MaybeRooted<NativeObject*, allowGC>::HandleType obj,
    typename MaybeRooted<jsid, allowGC>::HandleType id,
    typename MaybeRooted<JSObject*, allowGC>::MutableHandleType objp,
    typename MaybeRooted<PropertyResult, allowGC>::MutableHandleType propp) {
  /* Search scopes starting with obj and following the prototype link. */
  typename MaybeRooted<NativeObject*, allowGC>::RootType current(cx, obj);

  while (true) {
    bool done;
    if (!LookupOwnPropertyInline<allowGC>(cx, current, id, propp, &done)) {
      return false;
    }
    if (done) {
      if (propp) {
        objp.set(current);
      } else {
        objp.set(nullptr);
      }
      return true;
    }

    typename MaybeRooted<JSObject*, allowGC>::RootType proto(
        cx, current->staticPrototype());

    if (!proto) {
      break;
    }
    if (!proto->isNative()) {
      MOZ_ASSERT(!cx->isHelperThreadContext());
      if (!allowGC) {
        return false;
      }
      return LookupProperty(
          cx, MaybeRooted<JSObject*, allowGC>::toHandle(proto),
          MaybeRooted<jsid, allowGC>::toHandle(id),
          MaybeRooted<JSObject*, allowGC>::toMutableHandle(objp),
          MaybeRooted<PropertyResult, allowGC>::toMutableHandle(propp));
    }

    current = &proto->template as<NativeObject>();
  }

  objp.set(nullptr);
  propp.setNotFound();
  return true;
}

inline bool ThrowIfNotConstructing(JSContext* cx, const CallArgs& args,
                                   const char* builtinName) {
  if (args.isConstructing()) {
    return true;
  }
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_BUILTIN_CTOR_NO_NEW, builtinName);
  return false;
}

inline bool IsPackedArray(JSObject* obj) {
  if (!obj->is<ArrayObject>() || obj->hasLazyGroup()) {
    return false;
  }
  AutoSweepObjectGroup sweep(obj->group());
  if (obj->group()->hasAllFlags(sweep, OBJECT_FLAG_NON_PACKED)) {
    return false;
  }
  return obj->as<ArrayObject>().getDenseInitializedLength() ==
         obj->as<ArrayObject>().length();
}

}  // namespace js

#endif /* vm_NativeObject_inl_h */
