/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SharedArrayObject_h
#define vm_SharedArrayObject_h

#include "mozilla/Atomics.h"

#include "jsapi.h"
#include "jsobj.h"
#include "jstypes.h"

#include "gc/Barrier.h"
#include "vm/ArrayBufferObject.h"

typedef struct JSProperty JSProperty;

namespace js {

/*
 * SharedArrayRawBuffer
 *
 * A bookkeeping object always stored immediately before the raw buffer.
 * The buffer itself is mmap()'d and refcounted.
 * SharedArrayBufferObjects and AsmJS code may hold references.
 *
 *           |<------ sizeof ------>|<- length ->|
 *
 *   | waste | SharedArrayRawBuffer | data array | waste |
 */
class SharedArrayRawBuffer
{
  private:
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> refcount;
    uint32_t length;

  protected:
    SharedArrayRawBuffer(uint8_t *buffer, uint32_t length)
      : refcount(1), length(length)
    {
        JS_ASSERT(buffer == dataPointer());
    }

  public:
    static SharedArrayRawBuffer *New(uint32_t length);

    inline uint8_t *dataPointer() const {
        return ((uint8_t *)this) + sizeof(SharedArrayRawBuffer);
    }

    inline uint32_t byteLength() const {
        return length;
    }

    void addReference();
    void dropReference();
};

/*
 * SharedArrayBufferObject
 *
 * When transferred to a WebWorker, the buffer is not neutered on the parent side,
 * and both child and parent reference the same buffer.
 */
class SharedArrayBufferObject : public ArrayBufferObject
{
    static bool byteLengthGetterImpl(JSContext *cx, CallArgs args);

  public:
    static const Class class_;
    static const Class protoClass;

    // Slot used for storing a pointer to the SharedArrayRawBuffer.
    // First two slots hold the ObjectElements.
    static const int32_t RAWBUF_SLOT = 2;

    static bool class_constructor(JSContext *cx, unsigned argc, Value *vp);

    // Create a SharedArrayBufferObject with a new SharedArrayRawBuffer.
    static JSObject *New(JSContext *cx, uint32_t length);

    // Create a SharedArrayBufferObject using an existing SharedArrayRawBuffer.
    static JSObject *New(JSContext *cx, SharedArrayRawBuffer *buffer);

    static bool byteLengthGetter(JSContext *cx, unsigned argc, Value *vp);

    static void Finalize(FreeOp *fop, JSObject *obj);

    void acceptRawBuffer(SharedArrayRawBuffer *buffer);
    void dropRawBuffer();

    SharedArrayRawBuffer *rawBufferObject() const;
    uint8_t *dataPointer() const;
    uint32_t byteLength() const;
};

bool
IsSharedArrayBuffer(HandleValue v);

} // namespace js

#endif // vm_SharedArrayObject_h
