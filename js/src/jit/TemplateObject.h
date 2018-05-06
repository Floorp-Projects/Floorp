/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TemplateObject_h
#define jit_TemplateObject_h

#include "vm/NativeObject.h"
#include "vm/Shape.h"
#include "vm/UnboxedObject.h"

namespace js {
namespace jit {

class NativeTemplateObject;

// Wrapper for template objects. This should only expose methods that can be
// safely called off-thread without racing with the main thread.
class TemplateObject
{
  protected:
    JSObject* obj_;
    bool denseElementsAreCopyOnWrite_;
    bool convertDoubleElements_;

  public:
    explicit TemplateObject(JSObject* obj)
      : obj_(obj),
        denseElementsAreCopyOnWrite_(false),
        convertDoubleElements_(false)
    {}
    void setDenseElementsAreCopyOnWrite() { denseElementsAreCopyOnWrite_ = true; }
    void setConvertDoubleElements() { convertDoubleElements_ = true; }

    inline gc::AllocKind getAllocKind() const;

    // The following methods rely on the object's group->clasp. This is safe
    // to read off-thread for template objects.
    inline bool isNative() const;
    inline const NativeTemplateObject& asNativeTemplateObject() const;
    inline bool isArrayObject() const;
    inline bool isArgumentsObject() const;
    inline bool isTypedArrayObject() const;
    inline bool isRegExpObject() const;
    inline bool isInlineTypedObject() const;
    inline bool isUnboxedPlainObject() const;
    inline bool isCallObject() const;
    inline bool isPlainObject() const;

    // The group and shape should not change. This is true for template objects
    // because they're never exposed to arbitrary script.
    inline gc::Cell* group() const;
    inline gc::Cell* maybeShape() const;

    // Some TypedObject and UnboxedPlainObject methods that can be called
    // off-thread.
    inline uint32_t getInlineTypedObjectSize() const;
    inline uint8_t* getInlineTypedObjectMem(const JS::AutoRequireNoGC& nogc) const;
    inline const UnboxedLayout& unboxedObjectLayout() const;
#ifdef DEBUG
    inline bool unboxedObjectHasExpando() const;
#endif
};

class NativeTemplateObject : public TemplateObject
{
  protected:
    NativeObject& asNative() const {
        return obj_->as<NativeObject>();
    }

  public:
    // Reading slot counts and object slots is safe, as long as we don't touch
    // the BaseShape (it can change when we create a ShapeTable for the shape).
    inline bool hasDynamicSlots() const;
    inline uint32_t numDynamicSlots() const;
    inline uint32_t numUsedFixedSlots() const;
    inline uint32_t numFixedSlots() const;
    inline uint32_t slotSpan() const;
    inline Value getSlot(uint32_t i) const;

    // Reading ObjectElements fields is safe, except for the flags (we can set
    // the convert-double-elements flag on the main thread for COW elements).
    // isSharedMemory is an exception: it's debug-only and not called on arrays.
    bool denseElementsAreCopyOnWrite() const { return denseElementsAreCopyOnWrite_; }
    bool convertDoubleElements() const { return convertDoubleElements_; }
#ifdef DEBUG
    inline bool isSharedMemory() const;
#endif
    inline uint32_t getDenseCapacity() const;
    inline uint32_t getDenseInitializedLength() const;
    inline uint32_t getArrayLength() const;
    inline bool hasDynamicElements() const;
    inline const Value* getDenseElements() const;

    // Reading private slots is safe.
    inline bool hasPrivate() const;
    inline gc::Cell* regExpShared() const;
    inline void* getPrivate() const;
};

} // namespace jit
} // namespace js

#endif /* jit_TemplateObject_h */
