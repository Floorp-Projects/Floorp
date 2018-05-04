/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TemplateObject_inl_h
#define jit_TemplateObject_inl_h

#include "jit/TemplateObject.h"

#include "vm/RegExpObject.h"

#include "vm/ShapedObject-inl.h"

namespace js {
namespace jit {

inline gc::AllocKind
TemplateObject::getAllocKind() const
{
    return obj_->asTenured().getAllocKind();
}

inline bool
TemplateObject::isNative() const
{
    return obj_->isNative();
}

inline bool
TemplateObject::isArrayObject() const
{
    return obj_->is<ArrayObject>();
}

inline bool
TemplateObject::isArgumentsObject() const
{
    return obj_->is<ArgumentsObject>();
}

inline bool
TemplateObject::isTypedArrayObject() const
{
    return obj_->is<TypedArrayObject>();
}

inline bool
TemplateObject::isRegExpObject() const
{
    return obj_->is<RegExpObject>();
}

inline bool
TemplateObject::isInlineTypedObject() const
{
    return obj_->is<InlineTypedObject>();
}

inline bool
TemplateObject::isUnboxedPlainObject() const
{
    return obj_->is<UnboxedPlainObject>();
}

inline bool
TemplateObject::isCallObject() const
{
    return obj_->is<CallObject>();
}

inline bool
TemplateObject::isPlainObject() const
{
    return obj_->is<PlainObject>();
}

inline gc::Cell*
TemplateObject::group() const
{
    MOZ_ASSERT(!obj_->hasLazyGroup());
    return obj_->group();
}

inline gc::Cell*
TemplateObject::maybeShape() const
{
    if (obj_->is<ShapedObject>()) {
        Shape* shape = obj_->maybeShape();
        MOZ_ASSERT(!shape->inDictionary());
        return shape;
    }
    return nullptr;
}

inline uint32_t
TemplateObject::getInlineTypedObjectSize() const
{
    return obj_->as<InlineTypedObject>().size();
}

inline uint8_t*
TemplateObject::getInlineTypedObjectMem(const JS::AutoRequireNoGC& nogc) const
{
    return obj_->as<InlineTypedObject>().inlineTypedMem(nogc);
}

inline const UnboxedLayout&
TemplateObject::unboxedObjectLayout() const
{
    return obj_->as<UnboxedPlainObject>().layoutDontCheckGeneration();
}

#ifdef DEBUG
inline bool
TemplateObject::unboxedObjectHasExpando() const
{
    return obj_->as<UnboxedPlainObject>().maybeExpando();
}
#endif

inline const NativeTemplateObject&
TemplateObject::asNativeTemplateObject() const
{
    MOZ_ASSERT(isNative());
    return *static_cast<const NativeTemplateObject*>(this);
}

inline bool
NativeTemplateObject::hasDynamicSlots() const
{
    return asNative().hasDynamicSlots();
}

inline uint32_t
NativeTemplateObject::numDynamicSlots() const
{
    // We can't call numDynamicSlots because that uses shape->base->clasp and
    // shape->base can change when we create a ShapeTable.
    return NativeObject::dynamicSlotsCount(numFixedSlots(), slotSpan(), obj_->getClass());
}

inline uint32_t
NativeTemplateObject::numUsedFixedSlots() const
{
    return asNative().numUsedFixedSlots();
}

inline uint32_t
NativeTemplateObject::numFixedSlots() const
{
    return asNative().numFixedSlots();
}

inline uint32_t
NativeTemplateObject::slotSpan() const
{
    // Don't call NativeObject::slotSpan, it uses shape->base->clasp and the
    // shape's BaseShape can change when we create a ShapeTable for it.
    return asNative().shape()->slotSpan(obj_->getClass());
}

inline Value
NativeTemplateObject::getSlot(uint32_t i) const
{
    return asNative().getSlot(i);
}

inline const Value*
NativeTemplateObject::getDenseElements() const
{
    return asNative().getDenseElementsAllowCopyOnWrite();
}

#ifdef DEBUG
inline bool
NativeTemplateObject::isSharedMemory() const
{
    return asNative().isSharedMemory();
}
#endif

inline uint32_t
NativeTemplateObject::getDenseCapacity() const
{
    return asNative().getDenseCapacity();
}

inline uint32_t
NativeTemplateObject::getDenseInitializedLength() const
{
    return asNative().getDenseInitializedLength();
}

inline uint32_t
NativeTemplateObject::getArrayLength() const
{
    return obj_->as<ArrayObject>().length();
}

inline bool
NativeTemplateObject::hasDynamicElements() const
{
    return asNative().hasDynamicElements();
}

inline bool
NativeTemplateObject::hasPrivate() const
{
    return asNative().hasPrivate();
}

inline gc::Cell*
NativeTemplateObject::regExpShared() const
{
    RegExpObject* regexp = &obj_->as<RegExpObject>();
    MOZ_ASSERT(regexp->hasShared());
    MOZ_ASSERT(regexp->getPrivate() == regexp->sharedRef().get());
    return regexp->sharedRef().get();
}

inline void*
NativeTemplateObject::getPrivate() const
{
    return asNative().getPrivate();
}

} // namespace jit
} // namespace js

#endif /* jit_TemplateObject_inl_h */
