/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Stream_h
#define builtin_Stream_h

#include "builtin/Promise.h"
#include "vm/NativeObject.h"


namespace js {

class ReadableStreamReader;
class ReadableStreamController;

class ReadableStream : public NativeObject
{
  public:
    /**
     * Memory layout of Stream instances.
     *
     * See https://streams.spec.whatwg.org/#rs-internal-slots for details on
     * the stored state. [[state]] and [[disturbed]] are stored in
     * StreamSlot_State as ReadableStream::State enum values.
     *
     * Of the stored values, Reader and StoredError might be cross-compartment
     * wrappers. This can happen if the Reader was created by applying a
     * different compartment's ReadableStream.prototype.getReader method.
     *
     * A stream's associated controller is always created from under the
     * stream's constructor and thus cannot be in a different compartment.
     */
    enum Slots {
        Slot_Controller,
        Slot_Reader,
        Slot_State,
        Slot_StoredError,
        SlotCount
    };

  private:
    uint32_t stateBits() const { return getFixedSlot(Slot_State).toInt32(); }
    void initStateBits(uint32_t stateBits) { setFixedSlot(Slot_State, Int32Value(stateBits)); }
    void setStateBits(uint32_t stateBits) {
        MOZ_ASSERT_IF(disturbed(), stateBits & Disturbed);
        MOZ_ASSERT_IF(closed() || errored(), !(stateBits & Readable));
        setFixedSlot(Slot_State, Int32Value(stateBits));
    }

  public:
    bool readable() const { return stateBits() & Readable; }
    bool closed() const { return stateBits() & Closed; }
    void setClosed() { setStateBits((stateBits() & Disturbed) | Closed); }
    bool errored() const { return stateBits() & Errored; }
    void setErrored() { setStateBits((stateBits() & Disturbed) | Errored); }
    bool disturbed() const { return stateBits() & Disturbed; }
    void setDisturbed() { setStateBits(stateBits() | Disturbed); }

    bool hasController() const { return !getFixedSlot(Slot_Controller).isUndefined(); }
    inline ReadableStreamController* controller() const;
    inline void setController(ReadableStreamController* controller);
    void clearController() { setFixedSlot(Slot_Controller, JS::UndefinedValue()); }

    bool hasReader() const { return !getFixedSlot(Slot_Reader).isUndefined(); }
    void setReader(JSObject* reader) { setFixedSlot(Slot_Reader, JS::ObjectValue(*reader)); }
    void clearReader() { setFixedSlot(Slot_Reader, JS::UndefinedValue()); }

    Value storedError() const { return getFixedSlot(Slot_StoredError); }
    void setStoredError(HandleValue value) { setFixedSlot(Slot_StoredError, value); }

  public:
    static ReadableStream* createDefaultStream(JSContext* cx, HandleValue underlyingSource,
                                               HandleValue size, HandleValue highWaterMark,
                                               HandleObject proto = nullptr);
    static ReadableStream* createExternalSourceStream(JSContext* cx, void* underlyingSource,
                                                      uint8_t flags, HandleObject proto = nullptr);

    bool locked() const;

    void desiredSize(bool* hasSize, double* size) const;

    JS::ReadableStreamMode mode() const;

    static MOZ_MUST_USE bool close(JSContext* cx, Handle<ReadableStream*> stream);
    static MOZ_MUST_USE JSObject* cancel(JSContext* cx, Handle<ReadableStream*> stream,
                                         HandleValue reason);
    static MOZ_MUST_USE bool error(JSContext* cx, Handle<ReadableStream*> stream,
                                   HandleValue error);

    static MOZ_MUST_USE ReadableStreamReader* getReader(JSContext* cx,
                                                        Handle<ReadableStream*> stream,
                                                        JS::ReadableStreamReaderMode mode);

    static MOZ_MUST_USE bool tee(JSContext* cx,
                                 Handle<ReadableStream*> stream, bool cloneForBranch2,
                                 MutableHandle<ReadableStream*> branch1Stream,
                                 MutableHandle<ReadableStream*> branch2Stream);

    static MOZ_MUST_USE bool enqueue(JSContext* cx, Handle<ReadableStream*> stream,
                                     HandleValue chunk);
    static MOZ_MUST_USE bool getExternalSource(JSContext* cx, Handle<ReadableStream*> stream,
                                               void** source);
    void releaseExternalSource();
    uint8_t embeddingFlags() const;
    static MOZ_MUST_USE bool updateDataAvailableFromSource(JSContext* cx,
                                                           Handle<ReadableStream*> stream,
                                                           uint32_t availableData);

    enum State {
         Readable  = 1 << 0,
         Closed    = 1 << 1,
         Errored   = 1 << 2,
         Disturbed = 1 << 3
    };

  private:
    static MOZ_MUST_USE ReadableStream* createStream(JSContext* cx, HandleObject proto = nullptr);

  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableStreamReader : public NativeObject
{
  public:
    /**
     * Memory layout of Stream Reader instances.
     *
     * See https://streams.spec.whatwg.org/#default-reader-internal-slots and
     * https://streams.spec.whatwg.org/#byob-reader-internal-slots for details.
     *
     * Note that [[readRequests]] and [[readIntoRequests]] are treated the same
     * in our implementation.
     *
     * Of the stored values, Stream and ClosedPromise might be
     * cross-compartment wrapper wrappers.
     *
     * For Stream, this can happen if the Reader was created by applying a
     * different compartment's ReadableStream.prototype.getReader method.
     *
     * For ClosedPromise, it can be caused by applying a different
     * compartment's ReadableStream*Reader.prototype.releaseLock method.
     *
     * Requests is guaranteed to be in the same compartment as the Reader, but
     * can contain wrapped request objects from other globals.
     */
    enum Slots {
        Slot_Stream,
        Slot_Requests,
        Slot_ClosedPromise,
        SlotCount,
    };

    bool hasStream() const { return !getFixedSlot(Slot_Stream).isUndefined(); }
    void setStream(JSObject* stream) { setFixedSlot(Slot_Stream, ObjectValue(*stream)); }
    void clearStream() { setFixedSlot(Slot_Stream, UndefinedValue()); }
    bool isClosed() { return !hasStream(); }

    NativeObject* requests() const {
        return &getFixedSlot(Slot_Requests).toObject().as<NativeObject>();
    }
    void clearRequests() { setFixedSlot(Slot_Requests, UndefinedValue()); }

    JSObject* closedPromise() const { return &getFixedSlot(Slot_ClosedPromise).toObject(); }
    void setClosedPromise(JSObject* wrappedPromise) {
        setFixedSlot(Slot_ClosedPromise, ObjectValue(*wrappedPromise));
    }

    static const Class class_;
};

class ReadableStreamDefaultReader : public ReadableStreamReader
{
  public:
    static MOZ_MUST_USE JSObject* read(JSContext* cx, Handle<ReadableStreamDefaultReader*> reader);

    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

bool ReadableStreamReaderIsClosed(const JSObject* reader);

MOZ_MUST_USE bool ReadableStreamReaderCancel(JSContext* cx, HandleObject reader,
                                             HandleValue reason);

MOZ_MUST_USE bool ReadableStreamReaderReleaseLock(JSContext* cx, HandleObject reader);

class ReadableStreamController : public NativeObject
{
  public:
    static const Class class_;
};

class ReadableStreamDefaultController : public ReadableStreamController
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ReadableByteStreamController : public ReadableStreamController
{
  public:
    bool hasExternalSource();

    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class ByteLengthQueuingStrategy : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

class CountQueuingStrategy : public NativeObject
{
  public:
    static bool constructor(JSContext* cx, unsigned argc, Value* vp);
    static const ClassSpec classSpec_;
    static const Class class_;
    static const ClassSpec protoClassSpec_;
    static const Class protoClass_;
};

} // namespace js

template <>
inline bool
JSObject::is<js::ReadableStreamController>() const
{
    return is<js::ReadableStreamDefaultController>() || is<js::ReadableByteStreamController>();
}

template <>
inline bool
JSObject::is<js::ReadableStreamReader>() const
{
    return is<js::ReadableStreamDefaultReader>();
}

namespace js {

inline ReadableStreamController*
ReadableStream::controller() const
{
    return &getFixedSlot(Slot_Controller).toObject().as<ReadableStreamController>();
}

inline void
ReadableStream::setController(ReadableStreamController* controller)
{
    setFixedSlot(Slot_Controller, JS::ObjectValue(*controller));
}

} // namespace js

#endif /* builtin_Stream_h */
