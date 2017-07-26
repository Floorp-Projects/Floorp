/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_StructuredClone_h
#define js_StructuredClone_h

#include "mozilla/Attributes.h"
#include "mozilla/BufferList.h"
#include "mozilla/MemoryReporting.h"

#include <stdint.h>

#include "jstypes.h"

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/Vector.h"

struct JSRuntime;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

// API for the HTML5 internal structured cloning algorithm.

namespace JS {

enum class StructuredCloneScope : uint32_t {
    SameProcessSameThread,
    SameProcessDifferentThread,
    DifferentProcess
};

enum TransferableOwnership {
    /** Transferable data has not been filled in yet */
    SCTAG_TMO_UNFILLED = 0,

    /** Structured clone buffer does not yet own the data */
    SCTAG_TMO_UNOWNED = 1,

    /** All values at least this large are owned by the clone buffer */
    SCTAG_TMO_FIRST_OWNED = 2,

    /** Data is a pointer that can be freed */
    SCTAG_TMO_ALLOC_DATA = 2,

    /** Data is a memory mapped pointer */
    SCTAG_TMO_MAPPED_DATA = 3,

    /**
     * Data is embedding-specific. The engine can free it by calling the
     * freeTransfer op. The embedding can also use SCTAG_TMO_USER_MIN and
     * greater, up to 32 bits, to distinguish specific ownership variants.
     */
    SCTAG_TMO_CUSTOM = 4,

    SCTAG_TMO_USER_MIN
};

class CloneDataPolicy
{
    bool sharedArrayBuffer_;

  public:
    // The default is to allow all policy-controlled aspects.

    CloneDataPolicy() :
      sharedArrayBuffer_(true)
    {}

    // In the JS engine, SharedArrayBuffers can only be cloned intra-process
    // because the shared memory areas are allocated in process-private memory.
    // Clients should therefore deny SharedArrayBuffers when cloning data that
    // are to be transmitted inter-process.
    //
    // Clients should also deny SharedArrayBuffers when cloning data that are to
    // be transmitted intra-process if policy needs dictate such denial.

    CloneDataPolicy& denySharedArrayBuffer() {
        sharedArrayBuffer_ = false;
        return *this;
    }

    bool isSharedArrayBufferAllowed() const {
        return sharedArrayBuffer_;
    }
};

} /* namespace JS */

/**
 * Read structured data from the reader r. This hook is used to read a value
 * previously serialized by a call to the WriteStructuredCloneOp hook.
 *
 * tag and data are the pair of uint32_t values from the header. The callback
 * may use the JS_Read* APIs to read any other relevant parts of the object
 * from the reader r. closure is any value passed to the JS_ReadStructuredClone
 * function. Return the new object on success, nullptr on error/exception.
 */
typedef JSObject* (*ReadStructuredCloneOp)(JSContext* cx, JSStructuredCloneReader* r,
                                           uint32_t tag, uint32_t data, void* closure);

/**
 * Structured data serialization hook. The engine can write primitive values,
 * Objects, Arrays, Dates, RegExps, TypedArrays, ArrayBuffers, Sets, Maps,
 * and SharedTypedArrays. Any other type of object requires application support.
 * This callback must first use the JS_WriteUint32Pair API to write an object
 * header, passing a value greater than JS_SCTAG_USER to the tag parameter.
 * Then it can use the JS_Write* APIs to write any other relevant parts of
 * the value v to the writer w. closure is any value passed to the
 * JS_WriteStructuredClone function.
 *
 * Return true on success, false on error/exception.
 */
typedef bool (*WriteStructuredCloneOp)(JSContext* cx, JSStructuredCloneWriter* w,
                                       JS::HandleObject obj, void* closure);

/**
 * This is called when JS_WriteStructuredClone is given an invalid transferable.
 * To follow HTML5, the application must throw a DATA_CLONE_ERR DOMException
 * with error set to one of the JS_SCERR_* values.
 */
typedef void (*StructuredCloneErrorOp)(JSContext* cx, uint32_t errorid);

/**
 * This is called when JS_ReadStructuredClone receives a transferable object
 * not known to the engine. If this hook does not exist or returns false, the
 * JS engine calls the reportError op if set, otherwise it throws a
 * DATA_CLONE_ERR DOM Exception. This method is called before any other
 * callback and must return a non-null object in returnObject on success.
 */
typedef bool (*ReadTransferStructuredCloneOp)(JSContext* cx, JSStructuredCloneReader* r,
                                              uint32_t tag, void* content, uint64_t extraData,
                                              void* closure,
                                              JS::MutableHandleObject returnObject);

/**
 * Called when JS_WriteStructuredClone receives a transferable object not
 * handled by the engine. If this hook does not exist or returns false, the JS
 * engine will call the reportError hook or fall back to throwing a
 * DATA_CLONE_ERR DOM Exception. This method is called before any other
 * callback.
 *
 *  tag: indicates what type of transferable this is. Must be greater than
 *       0xFFFF0201 (value of the internal SCTAG_TRANSFER_MAP_PENDING_ENTRY)
 *
 *  ownership: see TransferableOwnership, above. Used to communicate any needed
 *       ownership info to the FreeTransferStructuredCloneOp.
 *
 *  content, extraData: what the ReadTransferStructuredCloneOp will receive
 */
typedef bool (*TransferStructuredCloneOp)(JSContext* cx,
                                          JS::Handle<JSObject*> obj,
                                          void* closure,
                                          // Output:
                                          uint32_t* tag,
                                          JS::TransferableOwnership* ownership,
                                          void** content,
                                          uint64_t* extraData);

/**
 * Called when freeing an unknown transferable object. Note that it
 * should never trigger a garbage collection (and will assert in a
 * debug build if it does.)
 */
typedef void (*FreeTransferStructuredCloneOp)(uint32_t tag, JS::TransferableOwnership ownership,
                                              void* content, uint64_t extraData, void* closure);

// The maximum supported structured-clone serialization format version.
// Increment this when anything at all changes in the serialization format.
// (Note that this does not need to be bumped for Transferable-only changes,
// since they are never saved to persistent storage.)
#define JS_STRUCTURED_CLONE_VERSION 8

struct JSStructuredCloneCallbacks {
    ReadStructuredCloneOp read;
    WriteStructuredCloneOp write;
    StructuredCloneErrorOp reportError;
    ReadTransferStructuredCloneOp readTransfer;
    TransferStructuredCloneOp writeTransfer;
    FreeTransferStructuredCloneOp freeTransfer;
};

enum OwnTransferablePolicy {
    OwnsTransferablesIfAny,
    IgnoreTransferablesIfAny,
    NoTransferables
};

namespace js
{
    class SharedArrayRawBuffer;

    class SharedArrayRawBufferRefs
    {
      public:
        SharedArrayRawBufferRefs() = default;
        SharedArrayRawBufferRefs(SharedArrayRawBufferRefs&& other) = default;
        SharedArrayRawBufferRefs& operator=(SharedArrayRawBufferRefs&& other);
        ~SharedArrayRawBufferRefs();

        MOZ_MUST_USE bool acquire(JSContext* cx, SharedArrayRawBuffer* rawbuf);
        MOZ_MUST_USE bool acquireAll(JSContext* cx, const SharedArrayRawBufferRefs& that);
        void takeOwnership(SharedArrayRawBufferRefs&&);
        void releaseAll();

      private:
        js::Vector<js::SharedArrayRawBuffer*, 0, js::SystemAllocPolicy> refs_;
    };
}

class MOZ_NON_MEMMOVABLE JS_PUBLIC_API(JSStructuredCloneData) :
    public mozilla::BufferList<js::SystemAllocPolicy>
{
    typedef js::SystemAllocPolicy AllocPolicy;
    typedef mozilla::BufferList<js::SystemAllocPolicy> BufferList;

    static const size_t kInitialSize = 0;
    static const size_t kInitialCapacity = 4096;
    static const size_t kStandardCapacity = 4096;

    const JSStructuredCloneCallbacks* callbacks_ = nullptr;
    void* closure_ = nullptr;
    OwnTransferablePolicy ownTransferables_ = OwnTransferablePolicy::NoTransferables;
    js::SharedArrayRawBufferRefs refsHeld_;

    void setOptionalCallbacks(const JSStructuredCloneCallbacks* callbacks,
                              void* closure,
                              OwnTransferablePolicy policy) {
        callbacks_ = callbacks;
        closure_ = closure;
        ownTransferables_ = policy;
    }

    friend struct JSStructuredCloneWriter;
    friend class JS_PUBLIC_API(JSAutoStructuredCloneBuffer);

public:
    explicit JSStructuredCloneData(AllocPolicy aAP = AllocPolicy())
        : BufferList(kInitialSize, kInitialCapacity, kStandardCapacity, aAP)
        , callbacks_(nullptr)
        , closure_(nullptr)
        , ownTransferables_(OwnTransferablePolicy::NoTransferables)
    {}
    MOZ_IMPLICIT JSStructuredCloneData(BufferList&& buffers)
        : BufferList(Move(buffers))
        , callbacks_(nullptr)
        , closure_(nullptr)
        , ownTransferables_(OwnTransferablePolicy::NoTransferables)
    {}
    JSStructuredCloneData(JSStructuredCloneData&& other) = default;
    JSStructuredCloneData& operator=(JSStructuredCloneData&& other) = default;
    ~JSStructuredCloneData();

    using BufferList::BufferList;
};

/** Note: if the *data contains transferable objects, it can be read only once. */
JS_PUBLIC_API(bool)
JS_ReadStructuredClone(JSContext* cx, JSStructuredCloneData& data, uint32_t version,
                       JS::StructuredCloneScope scope,
                       JS::MutableHandleValue vp,
                       const JSStructuredCloneCallbacks* optionalCallbacks, void* closure);

JS_PUBLIC_API(bool)
JS_WriteStructuredClone(JSContext* cx, JS::HandleValue v, JSStructuredCloneData* data,
                        JS::StructuredCloneScope scope,
                        JS::CloneDataPolicy cloneDataPolicy,
                        const JSStructuredCloneCallbacks* optionalCallbacks,
                        void* closure, JS::HandleValue transferable);

JS_PUBLIC_API(bool)
JS_StructuredCloneHasTransferables(JSStructuredCloneData& data, bool* hasTransferable);

JS_PUBLIC_API(bool)
JS_StructuredClone(JSContext* cx, JS::HandleValue v, JS::MutableHandleValue vp,
                   const JSStructuredCloneCallbacks* optionalCallbacks, void* closure);

/** RAII sugar for JS_WriteStructuredClone. */
class JS_PUBLIC_API(JSAutoStructuredCloneBuffer) {
    const JS::StructuredCloneScope scope_;
    JSStructuredCloneData data_;
    uint32_t version_;

  public:
    JSAutoStructuredCloneBuffer(JS::StructuredCloneScope scope,
                                const JSStructuredCloneCallbacks* callbacks, void* closure)
        : scope_(scope), version_(JS_STRUCTURED_CLONE_VERSION)
    {
        data_.setOptionalCallbacks(callbacks, closure, OwnTransferablePolicy::NoTransferables);
    }

    JSAutoStructuredCloneBuffer(JSAutoStructuredCloneBuffer&& other);
    JSAutoStructuredCloneBuffer& operator=(JSAutoStructuredCloneBuffer&& other);

    ~JSAutoStructuredCloneBuffer() { clear(); }

    JSStructuredCloneData& data() { return data_; }
    bool empty() const { return !data_.Size(); }

    void clear(const JSStructuredCloneCallbacks* optionalCallbacks=nullptr, void* closure=nullptr);

    /** Copy some memory. It will be automatically freed by the destructor. */
    bool copy(JSContext* cx, const JSStructuredCloneData& data,
              uint32_t version=JS_STRUCTURED_CLONE_VERSION,
              const JSStructuredCloneCallbacks* callbacks=nullptr, void* closure=nullptr);

    /**
     * Adopt some memory. It will be automatically freed by the destructor.
     * data must have been allocated by the JS engine (e.g., extracted via
     * JSAutoStructuredCloneBuffer::steal).
     */
    void adopt(JSStructuredCloneData&& data, uint32_t version=JS_STRUCTURED_CLONE_VERSION,
               const JSStructuredCloneCallbacks* callbacks=nullptr, void* closure=nullptr);

    /**
     * Release the buffer and transfer ownership to the caller.
     */
    void steal(JSStructuredCloneData* data, uint32_t* versionp=nullptr,
               const JSStructuredCloneCallbacks** callbacks=nullptr, void** closure=nullptr);

    /**
     * Abandon ownership of any transferable objects stored in the buffer,
     * without freeing the buffer itself. Useful when copying the data out into
     * an external container, though note that you will need to use adopt() to
     * properly release that data eventually.
     */
    void abandon() { data_.ownTransferables_ = OwnTransferablePolicy::IgnoreTransferablesIfAny; }

    bool read(JSContext* cx, JS::MutableHandleValue vp,
              const JSStructuredCloneCallbacks* optionalCallbacks=nullptr, void* closure=nullptr);

    bool write(JSContext* cx, JS::HandleValue v,
               const JSStructuredCloneCallbacks* optionalCallbacks=nullptr, void* closure=nullptr);

    bool write(JSContext* cx, JS::HandleValue v, JS::HandleValue transferable,
               JS::CloneDataPolicy cloneDataPolicy,
               const JSStructuredCloneCallbacks* optionalCallbacks=nullptr, void* closure=nullptr);

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
    {
        return data_.SizeOfExcludingThis(mallocSizeOf);
    }

    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
    {
        return mallocSizeOf(this) + sizeOfExcludingThis(mallocSizeOf);
    }

  private:
    // Copy and assignment are not supported.
    JSAutoStructuredCloneBuffer(const JSAutoStructuredCloneBuffer& other) = delete;
    JSAutoStructuredCloneBuffer& operator=(const JSAutoStructuredCloneBuffer& other) = delete;
};

// The range of tag values the application may use for its own custom object types.
#define JS_SCTAG_USER_MIN  ((uint32_t) 0xFFFF8000)
#define JS_SCTAG_USER_MAX  ((uint32_t) 0xFFFFFFFF)

#define JS_SCERR_RECURSION 0
#define JS_SCERR_TRANSFERABLE 1
#define JS_SCERR_DUP_TRANSFERABLE 2
#define JS_SCERR_UNSUPPORTED_TYPE 3
#define JS_SCERR_SAB_TRANSFERABLE 4

JS_PUBLIC_API(bool)
JS_ReadUint32Pair(JSStructuredCloneReader* r, uint32_t* p1, uint32_t* p2);

JS_PUBLIC_API(bool)
JS_ReadBytes(JSStructuredCloneReader* r, void* p, size_t len);

JS_PUBLIC_API(bool)
JS_ReadTypedArray(JSStructuredCloneReader* r, JS::MutableHandleValue vp);

JS_PUBLIC_API(bool)
JS_WriteUint32Pair(JSStructuredCloneWriter* w, uint32_t tag, uint32_t data);

JS_PUBLIC_API(bool)
JS_WriteBytes(JSStructuredCloneWriter* w, const void* p, size_t len);

JS_PUBLIC_API(bool)
JS_WriteString(JSStructuredCloneWriter* w, JS::HandleString str);

JS_PUBLIC_API(bool)
JS_WriteTypedArray(JSStructuredCloneWriter* w, JS::HandleValue v);

JS_PUBLIC_API(bool)
JS_ObjectNotWritten(JSStructuredCloneWriter* w, JS::HandleObject obj);

JS_PUBLIC_API(JS::StructuredCloneScope)
JS_GetStructuredCloneScope(JSStructuredCloneWriter* w);

#endif  /* js_StructuredClone_h */
