/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ArrayBufferViewObject_h
#define vm_ArrayBufferViewObject_h

#include "builtin/TypedObjectConstants.h"
#include "vm/ArrayBufferObject.h"
#include "vm/NativeObject.h"
#include "vm/SharedArrayObject.h"
#include "vm/SharedMem.h"

namespace js {

/*
 * ArrayBufferViewObject
 *
 * Common base class for all array buffer views (DataViewObject and
 * TypedArrayObject).
 */

class ArrayBufferViewObject : public NativeObject
{
  public:
    // Underlying (Shared)ArrayBufferObject.
    static constexpr size_t BUFFER_SLOT = 0;
    static_assert(BUFFER_SLOT == JS_TYPEDARRAYLAYOUT_BUFFER_SLOT,
                  "self-hosted code with burned-in constants must get the "
                  "right buffer slot");

    // Slot containing length of the view in number of typed elements.
    static constexpr size_t LENGTH_SLOT = 1;
    static_assert(LENGTH_SLOT == JS_TYPEDARRAYLAYOUT_LENGTH_SLOT,
                  "self-hosted code with burned-in constants must get the "
                  "right length slot");

    // Offset of view within underlying (Shared)ArrayBufferObject.
    static constexpr size_t BYTEOFFSET_SLOT = 2;
    static_assert(BYTEOFFSET_SLOT == JS_TYPEDARRAYLAYOUT_BYTEOFFSET_SLOT,
                  "self-hosted code with burned-in constants must get the "
                  "right byteOffset slot");

    static constexpr size_t RESERVED_SLOTS = 3;

#ifdef DEBUG
    static const uint8_t ZeroLengthArrayData = 0x4A;
#endif

    // The raw pointer to the buffer memory, the "private" value.
    //
    // This offset is exposed for performance reasons - so that it
    // need not be looked up on accesses.
    static constexpr size_t DATA_SLOT = 3;

  private:
    void* dataPointerEither_() const {
        // Note, do not check whether shared or not
        // Keep synced with js::Get<Type>ArrayLengthAndData in jsfriendapi.h!
        return static_cast<void*>(getPrivate(DATA_SLOT));
    }

  public:
    static ArrayBufferObjectMaybeShared* bufferObject(JSContext* cx, Handle<ArrayBufferViewObject*> obj);

    void notifyBufferDetached(JSContext* cx, void* newData);

    // By construction we only need unshared variants here.  See
    // comments in ArrayBufferObject.cpp.
    uint8_t* dataPointerUnshared(const JS::AutoRequireNoGC&);
    void setDataPointerUnshared(uint8_t* data);

    void initDataPointer(SharedMem<uint8_t*> viewData) {
        // Install a pointer to the buffer location that corresponds
        // to offset zero within the typed array.
        //
        // The following unwrap is safe because the DATA_SLOT is
        // accessed only from jitted code and from the
        // dataPointerEither_() accessor above; in neither case does the
        // raw pointer escape untagged into C++ code.
        initPrivate(viewData.unwrap(/*safe - see above*/));
    }

    SharedMem<void*> dataPointerShared() const {
        return SharedMem<void*>::shared(dataPointerEither_());
    }
    SharedMem<void*> dataPointerEither() const {
        if (isSharedMemory()) {
            return SharedMem<void*>::shared(dataPointerEither_());
        }
        return SharedMem<void*>::unshared(dataPointerEither_());
    }
    void* dataPointerUnshared() const {
        MOZ_ASSERT(!isSharedMemory());
        return dataPointerEither_();
    }

    static Value bufferValue(const ArrayBufferViewObject* view) {
        return view->getFixedSlot(BUFFER_SLOT);
    }
    bool hasBuffer() const {
        return bufferValue(this).isObject();
    }
    JSObject* bufferObject() const {
        return bufferValue(this).toObjectOrNull();
    }

    ArrayBufferObject* bufferUnshared() const {
        MOZ_ASSERT(!isSharedMemory());
        JSObject* obj = bufferObject();
        if (!obj) {
            return nullptr;
        }
        return &obj->as<ArrayBufferObject>();
    }
    SharedArrayBufferObject* bufferShared() const {
        MOZ_ASSERT(isSharedMemory());
        JSObject* obj = bufferObject();
        if (!obj) {
            return nullptr;
        }
        return &obj->as<SharedArrayBufferObject>();
    }
    ArrayBufferObjectMaybeShared* bufferEither() const {
        JSObject* obj = bufferObject();
        if (!obj) {
            return nullptr;
        }
        if (isSharedMemory()) {
            return &obj->as<SharedArrayBufferObject>();
        }
        return &obj->as<ArrayBufferObject>();
    }

    bool hasDetachedBuffer() const {
        // Shared buffers can't be detached.
        if (isSharedMemory()) {
            return false;
        }

        // A typed array with a null buffer has never had its buffer exposed to
        // become detached.
        ArrayBufferObject* buffer = bufferUnshared();
        if (!buffer) {
            return false;
        }

        return buffer->isDetached();
    }

    static void trace(JSTracer* trc, JSObject* obj);
};

} // namespace js

template <>
bool
JSObject::is<js::ArrayBufferViewObject>() const;

#endif // vm_ArrayBufferViewObject_h
