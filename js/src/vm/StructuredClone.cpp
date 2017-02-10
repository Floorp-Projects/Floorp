/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file implements the structured clone algorithm of
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#safe-passing-of-structured-data
 *
 * The implementation differs slightly in that it uses an explicit stack, and
 * the "memory" maps source objects to sequential integer indexes rather than
 * directly pointing to destination objects. As a result, the order in which
 * things are added to the memory must exactly match the order in which they
 * are placed into 'allObjs', an analogous array of back-referenceable
 * destination objects constructed while reading.
 *
 * For the most part, this is easy: simply add objects to the memory when first
 * encountering them. But reading in a typed array requires an ArrayBuffer for
 * construction, so objects cannot just be added to 'allObjs' in the order they
 * are created. If they were, ArrayBuffers would come before typed arrays when
 * in fact the typed array was added to 'memory' first.
 *
 * So during writing, we add objects to the memory when first encountering
 * them. When reading a typed array, a placeholder is pushed onto allObjs until
 * the ArrayBuffer has been read, then it is updated with the actual typed
 * array object.
 */

#include "js/StructuredClone.h"

#include "mozilla/EndianUtils.h"
#include "mozilla/FloatingPoint.h"

#include <algorithm>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jswrapper.h"

#include "builtin/DataViewObject.h"
#include "builtin/MapObject.h"
#include "js/Date.h"
#include "js/GCHashTable.h"
#include "vm/SavedFrame.h"
#include "vm/SharedArrayObject.h"
#include "vm/TypedArrayObject.h"
#include "vm/WrapperObject.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

using namespace js;

using mozilla::BitwiseCast;
using mozilla::IsNaN;
using mozilla::LittleEndian;
using mozilla::NativeEndian;
using mozilla::NumbersAreIdentical;
using JS::CanonicalizeNaN;

// When you make updates here, make sure you consider whether you need to bump the
// value of JS_STRUCTURED_CLONE_VERSION in js/public/StructuredClone.h.  You will
// likely need to increment the version if anything at all changes in the serialization
// format.
//
// Note that SCTAG_END_OF_KEYS is written into the serialized form and should have
// a stable ID, it need not be at the end of the list and should not be used for
// sizing data structures.

enum StructuredDataType : uint32_t {
    /* Structured data types provided by the engine */
    SCTAG_FLOAT_MAX = 0xFFF00000,
    SCTAG_HEADER = 0xFFF10000,
    SCTAG_NULL = 0xFFFF0000,
    SCTAG_UNDEFINED,
    SCTAG_BOOLEAN,
    SCTAG_INT32,
    SCTAG_STRING,
    SCTAG_DATE_OBJECT,
    SCTAG_REGEXP_OBJECT,
    SCTAG_ARRAY_OBJECT,
    SCTAG_OBJECT_OBJECT,
    SCTAG_ARRAY_BUFFER_OBJECT,
    SCTAG_BOOLEAN_OBJECT,
    SCTAG_STRING_OBJECT,
    SCTAG_NUMBER_OBJECT,
    SCTAG_BACK_REFERENCE_OBJECT,
    SCTAG_DO_NOT_USE_1, // Required for backwards compatibility
    SCTAG_DO_NOT_USE_2, // Required for backwards compatibility
    SCTAG_TYPED_ARRAY_OBJECT,
    SCTAG_MAP_OBJECT,
    SCTAG_SET_OBJECT,
    SCTAG_END_OF_KEYS,
    SCTAG_DO_NOT_USE_3, // Required for backwards compatibility
    SCTAG_DATA_VIEW_OBJECT,
    SCTAG_SAVED_FRAME_OBJECT,

    // No new tags before principals.
    SCTAG_JSPRINCIPALS,
    SCTAG_NULL_JSPRINCIPALS,
    SCTAG_RECONSTRUCTED_SAVED_FRAME_PRINCIPALS_IS_SYSTEM,
    SCTAG_RECONSTRUCTED_SAVED_FRAME_PRINCIPALS_IS_NOT_SYSTEM,

    SCTAG_SHARED_ARRAY_BUFFER_OBJECT,

    SCTAG_TYPED_ARRAY_V1_MIN = 0xFFFF0100,
    SCTAG_TYPED_ARRAY_V1_INT8 = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Int8,
    SCTAG_TYPED_ARRAY_V1_UINT8 = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Uint8,
    SCTAG_TYPED_ARRAY_V1_INT16 = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Int16,
    SCTAG_TYPED_ARRAY_V1_UINT16 = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Uint16,
    SCTAG_TYPED_ARRAY_V1_INT32 = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Int32,
    SCTAG_TYPED_ARRAY_V1_UINT32 = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Uint32,
    SCTAG_TYPED_ARRAY_V1_FLOAT32 = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Float32,
    SCTAG_TYPED_ARRAY_V1_FLOAT64 = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Float64,
    SCTAG_TYPED_ARRAY_V1_UINT8_CLAMPED = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::Uint8Clamped,
    SCTAG_TYPED_ARRAY_V1_MAX = SCTAG_TYPED_ARRAY_V1_MIN + Scalar::MaxTypedArrayViewType - 1,

    /*
     * Define a separate range of numbers for Transferable-only tags, since
     * they are not used for persistent clone buffers and therefore do not
     * require bumping JS_STRUCTURED_CLONE_VERSION.
     */
    SCTAG_TRANSFER_MAP_HEADER = 0xFFFF0200,
    SCTAG_TRANSFER_MAP_PENDING_ENTRY,
    SCTAG_TRANSFER_MAP_ARRAY_BUFFER,
    SCTAG_TRANSFER_MAP_STORED_ARRAY_BUFFER,
    SCTAG_TRANSFER_MAP_END_OF_BUILTIN_TYPES,

    SCTAG_END_OF_BUILTIN_TYPES
};

/*
 * Format of transfer map:
 *   <SCTAG_TRANSFER_MAP_HEADER, TransferableMapHeader(UNREAD|TRANSFERRED)>
 *   numTransferables (64 bits)
 *   array of:
 *     <SCTAG_TRANSFER_MAP_*, TransferableOwnership>
 *     pointer (64 bits)
 *     extraData (64 bits), eg byte length for ArrayBuffers
 */

// Data associated with an SCTAG_TRANSFER_MAP_HEADER that tells whether the
// contents have been read out yet or not.
enum TransferableMapHeader {
    SCTAG_TM_UNREAD = 0,
    SCTAG_TM_TRANSFERRED
};

static inline uint64_t
PairToUInt64(uint32_t tag, uint32_t data)
{
    return uint64_t(data) | (uint64_t(tag) << 32);
}

namespace js {

template<typename T, typename AllocPolicy>
struct BufferIterator {
    typedef mozilla::BufferList<AllocPolicy> BufferList;

    explicit BufferIterator(BufferList& buffer)
        : mBuffer(buffer)
        , mIter(buffer.Iter())
    {
        JS_STATIC_ASSERT(8 % sizeof(T) == 0);
    }

    BufferIterator(const BufferIterator& other)
        : mBuffer(other.mBuffer)
        , mIter(other.mIter)
    {
    }

    BufferIterator& operator=(const BufferIterator& other)
    {
        MOZ_ASSERT(&mBuffer == &other.mBuffer);
        mIter = other.mIter;
        return *this;
    }

    BufferIterator operator++(int) {
        BufferIterator ret = *this;
        if (!mIter.AdvanceAcrossSegments(mBuffer, sizeof(T))) {
            MOZ_ASSERT(false, "Failed to read StructuredCloneData. Data incomplete");
        }
        return ret;
    }

    BufferIterator& operator+=(size_t size) {
        if (!mIter.AdvanceAcrossSegments(mBuffer, size)) {
            MOZ_ASSERT(false, "Failed to read StructuredCloneData. Data incomplete");
        }
        return *this;
    }

    size_t operator-(const BufferIterator& other) {
        MOZ_ASSERT(&mBuffer == &other.mBuffer);
        return mBuffer.RangeLength(other.mIter, mIter);
    }

    void next() {
        if (!mIter.AdvanceAcrossSegments(mBuffer, sizeof(T))) {
            MOZ_ASSERT(false, "Failed to read StructuredCloneData. Data incomplete");
        }
    }

    bool done() const {
        return mIter.Done();
    }

    bool readBytes(char* outData, size_t size) {
        return mBuffer.ReadBytes(mIter, outData, size);
    }

    void write(const T& data) {
        MOZ_ASSERT(mIter.HasRoomFor(sizeof(T)));
        *reinterpret_cast<T*>(mIter.Data()) = data;
    }

    T peek() const {
        MOZ_ASSERT(mIter.HasRoomFor(sizeof(T)));
        return *reinterpret_cast<T*>(mIter.Data());
    }

    BufferList& mBuffer;
    typename BufferList::IterImpl mIter;
};

struct SCOutput {
  public:
    using Iter = BufferIterator<uint64_t, TempAllocPolicy>;

    explicit SCOutput(JSContext* cx);

    JSContext* context() const { return cx; }

    bool write(uint64_t u);
    bool writePair(uint32_t tag, uint32_t data);
    bool writeDouble(double d);
    bool writeBytes(const void* p, size_t nbytes);
    bool writeChars(const Latin1Char* p, size_t nchars);
    bool writeChars(const char16_t* p, size_t nchars);
    bool writePtr(const void*);

    template <class T>
    bool writeArray(const T* p, size_t nbytes);

    bool extractBuffer(JSStructuredCloneData* data);
    void discardTransferables(const JSStructuredCloneCallbacks* cb, void* cbClosure);

    uint64_t tell() const { return buf.Size(); }
    uint64_t count() const { return buf.Size() / sizeof(uint64_t); }
    Iter iter() {
        return BufferIterator<uint64_t, TempAllocPolicy>(buf);
    }

    size_t offset(Iter dest) {
        return dest - iter();
    }

  private:
    JSContext* cx;
    mozilla::BufferList<TempAllocPolicy> buf;
};

class SCInput {
    typedef js::BufferIterator<uint64_t, SystemAllocPolicy> BufferIterator;

  public:
    SCInput(JSContext* cx, JSStructuredCloneData& data);

    JSContext* context() const { return cx; }

    static void getPtr(uint64_t data, void** ptr);
    static void getPair(uint64_t data, uint32_t* tagp, uint32_t* datap);

    bool read(uint64_t* p);
    bool readNativeEndian(uint64_t* p);
    bool readPair(uint32_t* tagp, uint32_t* datap);
    bool readDouble(double* p);
    bool readBytes(void* p, size_t nbytes);
    bool readChars(Latin1Char* p, size_t nchars);
    bool readChars(char16_t* p, size_t nchars);
    bool readPtr(void**);

    bool get(uint64_t* p);
    bool getPair(uint32_t* tagp, uint32_t* datap);

    const BufferIterator& tell() const { return point; }
    void seekTo(const BufferIterator& pos) { point = pos; }
    void seekBy(size_t pos) { point += pos; }

    template <class T>
    bool readArray(T* p, size_t nelems);

    bool reportTruncated() {
         JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SC_BAD_SERIALIZED_DATA,
                                   "truncated");
         return false;
     }

  private:
    void staticAssertions() {
        JS_STATIC_ASSERT(sizeof(char16_t) == 2);
        JS_STATIC_ASSERT(sizeof(uint32_t) == 4);
    }

    JSContext* cx;
    BufferIterator point;
};

} /* namespace js */

struct JSStructuredCloneReader {
  public:
    explicit JSStructuredCloneReader(SCInput& in, JS::StructuredCloneScope scope,
                                     const JSStructuredCloneCallbacks* cb,
                                     void* cbClosure)
        : in(in), allowedScope(scope), objs(in.context()), allObjs(in.context()),
          callbacks(cb), closure(cbClosure) { }

    SCInput& input() { return in; }
    bool read(MutableHandleValue vp);

  private:
    JSContext* context() { return in.context(); }

    bool readHeader();
    bool readTransferMap();

    template <typename CharT>
    JSString* readStringImpl(uint32_t nchars);
    JSString* readString(uint32_t data);

    bool checkDouble(double d);
    bool readTypedArray(uint32_t arrayType, uint32_t nelems, MutableHandleValue vp,
                        bool v1Read = false);
    bool readDataView(uint32_t byteLength, MutableHandleValue vp);
    bool readArrayBuffer(uint32_t nbytes, MutableHandleValue vp);
    bool readSharedArrayBuffer(uint32_t nbytes, MutableHandleValue vp);
    bool readV1ArrayBuffer(uint32_t arrayType, uint32_t nelems, MutableHandleValue vp);
    JSObject* readSavedFrame(uint32_t principalsTag);
    bool startRead(MutableHandleValue vp);

    SCInput& in;

    // The widest scope that the caller will accept, where
    // SameProcessSameThread is the widest (it can store anything it wants) and
    // DifferentProcess is the narrowest (it cannot contain pointers and must
    // be valid cross-process.)
    JS::StructuredCloneScope allowedScope;

    // The scope the buffer was generated for (what sort of buffer it is.) The
    // scope is not just a permissions thing; it also affects the storage
    // format (eg a Transferred ArrayBuffer can be stored as a pointer for
    // SameProcessSameThread but must have its contents in the clone buffer for
    // DifferentProcess.)
    JS::StructuredCloneScope storedScope;

    // Stack of objects with properties remaining to be read.
    AutoValueVector objs;

    // Stack of all objects read during this deserialization
    AutoValueVector allObjs;

    // The user defined callbacks that will be used for cloning.
    const JSStructuredCloneCallbacks* callbacks;

    // Any value passed to JS_ReadStructuredClone.
    void* closure;

    friend bool JS_ReadTypedArray(JSStructuredCloneReader* r, MutableHandleValue vp);
};

struct JSStructuredCloneWriter {
  public:
    explicit JSStructuredCloneWriter(JSContext* cx,
                                     JS::StructuredCloneScope scope,
                                     JS::CloneDataPolicy cloneDataPolicy,
                                     const JSStructuredCloneCallbacks* cb,
                                     void* cbClosure,
                                     const Value& tVal)
        : out(cx), scope(scope), objs(out.context()),
          counts(out.context()), entries(out.context()),
          memory(out.context()), callbacks(cb),
          closure(cbClosure), transferable(out.context(), tVal),
          transferableObjects(out.context(), GCHashSet<JSObject*>(cx)),
          cloneDataPolicy(cloneDataPolicy)
    {}

    ~JSStructuredCloneWriter();

    bool init() {
        if (!memory.init()) {
            ReportOutOfMemory(context());
            return false;
        }
        return parseTransferable() && writeHeader() && writeTransferMap();
    }

    bool write(HandleValue v);

    SCOutput& output() { return out; }

    bool extractBuffer(JSStructuredCloneData* data) {
        bool success = out.extractBuffer(data);
        if (success) {
            data->setOptionalCallbacks(callbacks, closure,
                                       OwnTransferablePolicy::OwnsTransferablesIfAny);
        }
        return success;
    }

    JS::StructuredCloneScope cloneScope() const { return scope; }

  private:
    JSStructuredCloneWriter() = delete;
    JSStructuredCloneWriter(const JSStructuredCloneWriter&) = delete;

    JSContext* context() { return out.context(); }

    bool writeHeader();
    bool writeTransferMap();

    bool writeString(uint32_t tag, JSString* str);
    bool writeArrayBuffer(HandleObject obj);
    bool writeTypedArray(HandleObject obj);
    bool writeDataView(HandleObject obj);
    bool writeSharedArrayBuffer(HandleObject obj);
    bool startObject(HandleObject obj, bool* backref);
    bool startWrite(HandleValue v);
    bool traverseObject(HandleObject obj);
    bool traverseMap(HandleObject obj);
    bool traverseSet(HandleObject obj);
    bool traverseSavedFrame(HandleObject obj);

    bool reportDataCloneError(uint32_t errorId);

    bool parseTransferable();
    bool transferOwnership();

    inline void checkStack();

    SCOutput out;

    // The (address space, thread) scope within which this clone is valid.
    JS::StructuredCloneScope scope;

    // Vector of objects with properties remaining to be written.
    //
    // NB: These can span multiple compartments, so the compartment must be
    // entered before any manipulation is performed.
    AutoValueVector objs;

    // counts[i] is the number of entries of objs[i] remaining to be written.
    // counts.length() == objs.length() and sum(counts) == entries.length().
    Vector<size_t> counts;

    // For JSObject: Property IDs as value
    // For Map: Key followed by value
    // For Set: Key
    // For SavedFrame: parent SavedFrame
    AutoValueVector entries;

    // The "memory" list described in the HTML5 internal structured cloning
    // algorithm.  memory is a superset of objs; items are never removed from
    // Memory until a serialization operation is finished
    using CloneMemory = GCHashMap<JSObject*,
                                  uint32_t,
                                  MovableCellHasher<JSObject*>,
                                  SystemAllocPolicy>;
    Rooted<CloneMemory> memory;

    // The user defined callbacks that will be used for cloning.
    const JSStructuredCloneCallbacks* callbacks;

    // Any value passed to JS_WriteStructuredClone.
    void* closure;

    // Set of transferable objects
    RootedValue transferable;
    Rooted<GCHashSet<JSObject*>> transferableObjects;

    const JS::CloneDataPolicy cloneDataPolicy;

    friend bool JS_WriteString(JSStructuredCloneWriter* w, HandleString str);
    friend bool JS_WriteTypedArray(JSStructuredCloneWriter* w, HandleValue v);
    friend bool JS_ObjectNotWritten(JSStructuredCloneWriter* w, HandleObject obj);
};

JS_FRIEND_API(uint64_t)
js::GetSCOffset(JSStructuredCloneWriter* writer)
{
    MOZ_ASSERT(writer);
    return writer->output().count() * sizeof(uint64_t);
}

JS_STATIC_ASSERT(SCTAG_END_OF_BUILTIN_TYPES <= JS_SCTAG_USER_MIN);
JS_STATIC_ASSERT(JS_SCTAG_USER_MIN <= JS_SCTAG_USER_MAX);
JS_STATIC_ASSERT(Scalar::Int8 == 0);

static void
ReportDataCloneError(JSContext* cx,
                     const JSStructuredCloneCallbacks* callbacks,
                     uint32_t errorId)
{
    if (callbacks && callbacks->reportError) {
        callbacks->reportError(cx, errorId);
        return;
    }

    switch (errorId) {
      case JS_SCERR_DUP_TRANSFERABLE:
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SC_DUP_TRANSFERABLE);
        break;

      case JS_SCERR_TRANSFERABLE:
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SC_NOT_TRANSFERABLE);
        break;

      case JS_SCERR_UNSUPPORTED_TYPE:
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SC_UNSUPPORTED_TYPE);
        break;

      case JS_SCERR_SAB_TRANSFERABLE:
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SC_SAB_TRANSFERABLE);
        break;

      default:
        MOZ_CRASH("Unkown errorId");
        break;
    }
}

bool
WriteStructuredClone(JSContext* cx, HandleValue v, JSStructuredCloneData* bufp,
                     JS::StructuredCloneScope scope,
                     JS::CloneDataPolicy cloneDataPolicy,
                     const JSStructuredCloneCallbacks* cb, void* cbClosure,
                     const Value& transferable)
{
    JSStructuredCloneWriter w(cx, scope, cloneDataPolicy, cb, cbClosure, transferable);
    return w.init() && w.write(v) && w.extractBuffer(bufp);
}

bool
ReadStructuredClone(JSContext* cx, JSStructuredCloneData& data,
                    JS::StructuredCloneScope scope, MutableHandleValue vp,
                    const JSStructuredCloneCallbacks* cb, void* cbClosure)
{
    SCInput in(cx, data);
    JSStructuredCloneReader r(in, scope, cb, cbClosure);
    return r.read(vp);
}

// If the given buffer contains Transferables, free them. Note that custom
// Transferables will use the JSStructuredCloneCallbacks::freeTransfer() to
// delete their transferables.
template<typename AllocPolicy>
static void
DiscardTransferables(mozilla::BufferList<AllocPolicy>& buffer,
                     const JSStructuredCloneCallbacks* cb, void* cbClosure)
{
    auto point = BufferIterator<uint64_t, AllocPolicy>(buffer);
    if (point.done())
        return; // Empty buffer

    uint32_t tag, data;
    SCInput::getPair(point.peek(), &tag, &data);
    point.next();

    if (tag == SCTAG_HEADER) {
        if (point.done())
            return;

        SCInput::getPair(point.peek(), &tag, &data);
        point.next();
    }

    if (tag != SCTAG_TRANSFER_MAP_HEADER)
        return;

    if (TransferableMapHeader(data) == SCTAG_TM_TRANSFERRED)
        return;

    // freeTransfer should not GC
    JS::AutoSuppressGCAnalysis nogc;

    if (point.done())
        return;

    uint64_t numTransferables = NativeEndian::swapFromLittleEndian(point.peek());
    point.next();
    while (numTransferables--) {
        if (point.done())
            return;

        uint32_t ownership;
        SCInput::getPair(point.peek(), &tag, &ownership);
        point.next();
        MOZ_ASSERT(tag >= SCTAG_TRANSFER_MAP_PENDING_ENTRY);
        if (point.done())
            return;

        void* content;
        SCInput::getPtr(point.peek(), &content);
        point.next();
        if (point.done())
            return;

        uint64_t extraData = NativeEndian::swapFromLittleEndian(point.peek());
        point.next();

        if (ownership < JS::SCTAG_TMO_FIRST_OWNED)
            continue;

        if (ownership == JS::SCTAG_TMO_ALLOC_DATA) {
            js_free(content);
        } else if (ownership == JS::SCTAG_TMO_MAPPED_DATA) {
            JS_ReleaseMappedArrayBufferContents(content, extraData);
        } else if (cb && cb->freeTransfer) {
            cb->freeTransfer(tag, JS::TransferableOwnership(ownership), content, extraData, cbClosure);
        } else {
            MOZ_ASSERT(false, "unknown ownership");
        }
    }
}

static bool
StructuredCloneHasTransferObjects(const JSStructuredCloneData& data)
{
    auto iter = data.Iter();

    if (data.Size() < sizeof(uint64_t))
        return false;

    uint64_t u;
    data.ReadBytes(iter, reinterpret_cast<char*>(&u), sizeof(u));
    uint32_t tag = uint32_t(u >> 32);
    return (tag == SCTAG_TRANSFER_MAP_HEADER);
}

namespace js {

SCInput::SCInput(JSContext* cx, JSStructuredCloneData& data)
    : cx(cx), point(data)
{

    static_assert(JSStructuredCloneData::kSegmentAlignment % 8 == 0,
                  "structured clone buffer reads should be aligned");
    MOZ_ASSERT(data.Size() % 8 == 0);
}

bool
SCInput::read(uint64_t* p)
{
    if (point.done()) {
        *p = 0;  /* initialize to shut GCC up */
        return reportTruncated();
    }
    *p = NativeEndian::swapFromLittleEndian(point.peek());
    point.next();
    return true;
}

bool
SCInput::readNativeEndian(uint64_t* p)
{
    if (point.done()) {
        *p = 0;  /* initialize to shut GCC up */
        return reportTruncated();
    }
    *p = point.peek();
    point.next();
    return true;
}

bool
SCInput::readPair(uint32_t* tagp, uint32_t* datap)
{
    uint64_t u;
    bool ok = read(&u);
    if (ok) {
        *tagp = uint32_t(u >> 32);
        *datap = uint32_t(u);
    }
    return ok;
}

bool
SCInput::get(uint64_t* p)
{
    if (point.done())
        return reportTruncated();
    *p = NativeEndian::swapFromLittleEndian(point.peek());
    return true;
}

bool
SCInput::getPair(uint32_t* tagp, uint32_t* datap)
{
    uint64_t u = 0;
    if (!get(&u))
        return false;

    *tagp = uint32_t(u >> 32);
    *datap = uint32_t(u);
    return true;
}

void
SCInput::getPair(uint64_t data, uint32_t* tagp, uint32_t* datap)
{
    uint64_t u = NativeEndian::swapFromLittleEndian(data);
    *tagp = uint32_t(u >> 32);
    *datap = uint32_t(u);
}

bool
SCInput::readDouble(double* p)
{
    union {
        uint64_t u;
        double d;
    } pun;
    if (!read(&pun.u))
        return false;
    *p = CanonicalizeNaN(pun.d);
    return true;
}

template <typename T>
static void
swapFromLittleEndianInPlace(T* ptr, size_t nelems)
{
    if (nelems > 0)
        NativeEndian::swapFromLittleEndianInPlace(ptr, nelems);
}

template <>
void
swapFromLittleEndianInPlace(uint8_t* ptr, size_t nelems)
{}

template <class T>
bool
SCInput::readArray(T* p, size_t nelems)
{
    if (!nelems)
        return true;

    JS_STATIC_ASSERT(sizeof(uint64_t) % sizeof(T) == 0);

    /*
     * Fail if nelems is so huge as to make JS_HOWMANY overflow or if nwords is
     * larger than the remaining data.
     */
    size_t nwords = JS_HOWMANY(nelems, sizeof(uint64_t) / sizeof(T));
    if (nelems + sizeof(uint64_t) / sizeof(T) - 1 < nelems)
        return reportTruncated();

    size_t size = sizeof(T) * nelems;
    if (!point.readBytes(reinterpret_cast<char*>(p), size))
        return false;

    swapFromLittleEndianInPlace(p, nelems);

    point += sizeof(uint64_t) * nwords - size;

    return true;
}

bool
SCInput::readBytes(void* p, size_t nbytes)
{
    return readArray((uint8_t*) p, nbytes);
}

bool
SCInput::readChars(Latin1Char* p, size_t nchars)
{
    static_assert(sizeof(Latin1Char) == sizeof(uint8_t), "Latin1Char must fit in 1 byte");
    return readBytes(p, nchars);
}

bool
SCInput::readChars(char16_t* p, size_t nchars)
{
    MOZ_ASSERT(sizeof(char16_t) == sizeof(uint16_t));
    return readArray((uint16_t*) p, nchars);
}

void
SCInput::getPtr(uint64_t data, void** ptr)
{
    // No endianness conversion is used for pointers, since they are not sent
    // across address spaces anyway.
    *ptr = reinterpret_cast<void*>(data);
}

bool
SCInput::readPtr(void** p)
{
    uint64_t u;
    if (!readNativeEndian(&u))
        return false;
    *p = reinterpret_cast<void*>(NativeEndian::swapFromLittleEndian(u));
    return true;
}

SCOutput::SCOutput(JSContext* cx)
    : cx(cx)
    , buf(0, 0, 4096, cx)
{
}

bool
SCOutput::write(uint64_t u)
{
    uint64_t v = NativeEndian::swapToLittleEndian(u);
    return buf.WriteBytes(reinterpret_cast<char*>(&v), sizeof(u));
}

bool
SCOutput::writePair(uint32_t tag, uint32_t data)
{
    /*
     * As it happens, the tag word appears after the data word in the output.
     * This is because exponents occupy the last 2 bytes of doubles on the
     * little-endian platforms we care most about.
     *
     * For example, TrueValue() is written using writePair(SCTAG_BOOLEAN, 1).
     * PairToUInt64 produces the number 0xFFFF000200000001.
     * That is written out as the bytes 01 00 00 00 02 00 FF FF.
     */
    return write(PairToUInt64(tag, data));
}

static inline double
ReinterpretPairAsDouble(uint32_t tag, uint32_t data)
{
    return BitwiseCast<double>(PairToUInt64(tag, data));
}

bool
SCOutput::writeDouble(double d)
{
    return write(BitwiseCast<uint64_t>(CanonicalizeNaN(d)));
}

template <typename T>
static T
swapToLittleEndian(T value)
{
    return NativeEndian::swapToLittleEndian(value);
}

template <>
uint8_t
swapToLittleEndian(uint8_t value)
{
    return value;
}

template <class T>
bool
SCOutput::writeArray(const T* p, size_t nelems)
{
    JS_STATIC_ASSERT(8 % sizeof(T) == 0);
    JS_STATIC_ASSERT(sizeof(uint64_t) % sizeof(T) == 0);

    if (nelems == 0)
        return true;

    if (nelems + sizeof(uint64_t) / sizeof(T) - 1 < nelems) {
        ReportAllocationOverflow(context());
        return false;
    }

    for (size_t i = 0; i < nelems; i++) {
        T value = swapToLittleEndian(p[i]);
        if (!buf.WriteBytes(reinterpret_cast<char*>(&value), sizeof(value)))
            return false;
    }

    // zero-pad to 8 bytes boundary
    size_t nwords = JS_HOWMANY(nelems, sizeof(uint64_t) / sizeof(T));
    size_t padbytes = sizeof(uint64_t) * nwords - sizeof(T) * nelems;
    char zero = 0;
    for (size_t i = 0; i < padbytes; i++) {
        if (!buf.WriteBytes(&zero, sizeof(zero)))
            return false;
    }

    return true;
}

bool
SCOutput::writeBytes(const void* p, size_t nbytes)
{
    return writeArray((const uint8_t*) p, nbytes);
}

bool
SCOutput::writeChars(const char16_t* p, size_t nchars)
{
    static_assert(sizeof(char16_t) == sizeof(uint16_t),
                  "required so that treating char16_t[] memory as uint16_t[] "
                  "memory is permissible");
    return writeArray((const uint16_t*) p, nchars);
}

bool
SCOutput::writeChars(const Latin1Char* p, size_t nchars)
{
    static_assert(sizeof(Latin1Char) == sizeof(uint8_t), "Latin1Char must fit in 1 byte");
    return writeBytes(p, nchars);
}

bool
SCOutput::writePtr(const void* p)
{
    return write(reinterpret_cast<uint64_t>(p));
}

bool
SCOutput::extractBuffer(JSStructuredCloneData* data)
{
    bool success;
    mozilla::BufferList<SystemAllocPolicy> out =
        buf.MoveFallible<SystemAllocPolicy>(&success);
    if (!success) {
        ReportOutOfMemory(cx);
        return false;
    }
    *data = JSStructuredCloneData(Move(out));
    return true;
}

void
SCOutput::discardTransferables(const JSStructuredCloneCallbacks* cb, void* cbClosure)
{
    DiscardTransferables(buf, cb, cbClosure);
}

} /* namespace js */

JSStructuredCloneData::~JSStructuredCloneData()
{
    if (!Size())
        return;
    if (ownTransferables_ == OwnTransferablePolicy::OwnsTransferablesIfAny)
        DiscardTransferables(*this, callbacks_, closure_);
}

JS_STATIC_ASSERT(JSString::MAX_LENGTH < UINT32_MAX);

JSStructuredCloneWriter::~JSStructuredCloneWriter()
{
    // Free any transferable data left lying around in the buffer
    if (out.count()) {
        out.discardTransferables(callbacks, closure);
    }
}

bool
JSStructuredCloneWriter::parseTransferable()
{
    // NOTE: The transferables set is tested for non-emptiness at various
    //       junctures in structured cloning, so this set must be initialized
    //       by this method in all non-error cases.
    MOZ_ASSERT(!transferableObjects.initialized(),
               "parseTransferable called with stale data");

    if (transferable.isNull() || transferable.isUndefined())
        return transferableObjects.init(0);

    if (!transferable.isObject())
        return reportDataCloneError(JS_SCERR_TRANSFERABLE);

    JSContext* cx = context();
    RootedObject array(cx, &transferable.toObject());
    bool isArray;
    if (!JS_IsArrayObject(cx, array, &isArray))
        return false;
    if (!isArray)
        return reportDataCloneError(JS_SCERR_TRANSFERABLE);

    uint32_t length;
    if (!JS_GetArrayLength(cx, array, &length))
        return false;

    // Initialize the set for the provided array's length.
    if (!transferableObjects.init(length))
        return false;

    if (length == 0)
        return true;

    RootedValue v(context());
    RootedObject tObj(context());

    for (uint32_t i = 0; i < length; ++i) {
        if (!CheckForInterrupt(cx))
            return false;

        if (!JS_GetElement(cx, array, i, &v))
            return false;

        if (!v.isObject())
            return reportDataCloneError(JS_SCERR_TRANSFERABLE);
        tObj = &v.toObject();

        if (tObj->is<SharedArrayBufferObject>())
            return reportDataCloneError(JS_SCERR_SAB_TRANSFERABLE);

        // No duplicates allowed
        auto p = transferableObjects.lookupForAdd(tObj);
        if (p)
            return reportDataCloneError(JS_SCERR_DUP_TRANSFERABLE);

        if (!transferableObjects.add(p, tObj))
            return false;
    }

    return true;
}

bool
JSStructuredCloneWriter::reportDataCloneError(uint32_t errorId)
{
    ReportDataCloneError(context(), callbacks, errorId);
    return false;
}

bool
JSStructuredCloneWriter::writeString(uint32_t tag, JSString* str)
{
    JSLinearString* linear = str->ensureLinear(context());
    if (!linear)
        return false;

    static_assert(JSString::MAX_LENGTH <= INT32_MAX, "String length must fit in 31 bits");

    uint32_t length = linear->length();
    uint32_t lengthAndEncoding = length | (uint32_t(linear->hasLatin1Chars()) << 31);
    if (!out.writePair(tag, lengthAndEncoding))
        return false;

    JS::AutoCheckCannotGC nogc;
    return linear->hasLatin1Chars()
           ? out.writeChars(linear->latin1Chars(nogc), length)
           : out.writeChars(linear->twoByteChars(nogc), length);
}

inline void
JSStructuredCloneWriter::checkStack()
{
#ifdef DEBUG
    /* To avoid making serialization O(n^2), limit stack-checking at 10. */
    const size_t MAX = 10;

    size_t limit = Min(counts.length(), MAX);
    MOZ_ASSERT(objs.length() == counts.length());
    size_t total = 0;
    for (size_t i = 0; i < limit; i++) {
        MOZ_ASSERT(total + counts[i] >= total);
        total += counts[i];
    }
    if (counts.length() <= MAX)
        MOZ_ASSERT(total == entries.length());
    else
        MOZ_ASSERT(total <= entries.length());

    size_t j = objs.length();
    for (size_t i = 0; i < limit; i++) {
        --j;
        MOZ_ASSERT(memory.has(&objs[j].toObject()));
    }
#endif
}

/*
 * Write out a typed array. Note that post-v1 structured clone buffers do not
 * perform endianness conversion on stored data, so multibyte typed arrays
 * cannot be deserialized into a different endianness machine. Endianness
 * conversion would prevent sharing ArrayBuffers: if you have Int8Array and
 * Int16Array views of the same ArrayBuffer, should the data bytes be
 * byte-swapped when writing or not? The Int8Array requires them to not be
 * swapped; the Int16Array requires that they are.
 */
bool
JSStructuredCloneWriter::writeTypedArray(HandleObject obj)
{
    Rooted<TypedArrayObject*> tarr(context(), &CheckedUnwrap(obj)->as<TypedArrayObject>());
    JSAutoCompartment ac(context(), tarr);

    if (!TypedArrayObject::ensureHasBuffer(context(), tarr))
        return false;

    if (!out.writePair(SCTAG_TYPED_ARRAY_OBJECT, tarr->length()))
        return false;
    uint64_t type = tarr->type();
    if (!out.write(type))
        return false;

    // Write out the ArrayBuffer tag and contents
    RootedValue val(context(), TypedArrayObject::bufferValue(tarr));
    if (!startWrite(val))
        return false;

    return out.write(tarr->byteOffset());
}

bool
JSStructuredCloneWriter::writeDataView(HandleObject obj)
{
    Rooted<DataViewObject*> view(context(), &CheckedUnwrap(obj)->as<DataViewObject>());
    JSAutoCompartment ac(context(), view);

    if (!out.writePair(SCTAG_DATA_VIEW_OBJECT, view->byteLength()))
        return false;

    // Write out the ArrayBuffer tag and contents
    RootedValue val(context(), DataViewObject::bufferValue(view));
    if (!startWrite(val))
        return false;

    return out.write(view->byteOffset());
}

bool
JSStructuredCloneWriter::writeArrayBuffer(HandleObject obj)
{
    ArrayBufferObject& buffer = CheckedUnwrap(obj)->as<ArrayBufferObject>();
    JSAutoCompartment ac(context(), &buffer);

    return out.writePair(SCTAG_ARRAY_BUFFER_OBJECT, buffer.byteLength()) &&
           out.writeBytes(buffer.dataPointer(), buffer.byteLength());
}

bool
JSStructuredCloneWriter::writeSharedArrayBuffer(HandleObject obj)
{
    if (!cloneDataPolicy.isSharedArrayBufferAllowed()) {
        JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr, JSMSG_SC_NOT_CLONABLE,
                                  "SharedArrayBuffer");
        return false;
    }

    Rooted<SharedArrayBufferObject*> sharedArrayBuffer(context(), &CheckedUnwrap(obj)->as<SharedArrayBufferObject>());
    SharedArrayRawBuffer* rawbuf = sharedArrayBuffer->rawBufferObject();

    // Avoids a race condition where the parent thread frees the buffer
    // before the child has accepted the transferable.
    rawbuf->addReference();

    intptr_t p = reinterpret_cast<intptr_t>(rawbuf);
    return out.writePair(SCTAG_SHARED_ARRAY_BUFFER_OBJECT, static_cast<uint32_t>(sizeof(p))) &&
           out.writeBytes(&p, sizeof(p));
}

bool
JSStructuredCloneWriter::startObject(HandleObject obj, bool* backref)
{
    /* Handle cycles in the object graph. */
    CloneMemory::AddPtr p = memory.lookupForAdd(obj);
    if ((*backref = p.found()))
        return out.writePair(SCTAG_BACK_REFERENCE_OBJECT, p->value());
    if (!memory.add(p, obj, memory.count())) {
        ReportOutOfMemory(context());
        return false;
    }

    if (memory.count() == UINT32_MAX) {
        JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr, JSMSG_NEED_DIET,
                                  "object graph to serialize");
        return false;
    }

    return true;
}

bool
JSStructuredCloneWriter::traverseObject(HandleObject obj)
{
    /*
     * Get enumerable property ids and put them in reverse order so that they
     * will come off the stack in forward order.
     */
    AutoIdVector properties(context());
    if (!GetPropertyKeys(context(), obj, JSITER_OWNONLY, &properties))
        return false;

    for (size_t i = properties.length(); i > 0; --i) {
        MOZ_ASSERT(JSID_IS_STRING(properties[i - 1]) || JSID_IS_INT(properties[i - 1]));
        RootedValue val(context(), IdToValue(properties[i - 1]));
        if (!entries.append(val))
            return false;
    }

    /* Push obj and count to the stack. */
    if (!objs.append(ObjectValue(*obj)) || !counts.append(properties.length()))
        return false;

    checkStack();

    /* Write the header for obj. */
    ESClass cls;
    if (!GetBuiltinClass(context(), obj, &cls))
        return false;
    return out.writePair(cls == ESClass::Array ? SCTAG_ARRAY_OBJECT : SCTAG_OBJECT_OBJECT, 0);
}

bool
JSStructuredCloneWriter::traverseMap(HandleObject obj)
{
    Rooted<GCVector<Value>> newEntries(context(), GCVector<Value>(context()));
    {
        // If there is no wrapper, the compartment munging is a no-op.
        RootedObject unwrapped(context(), CheckedUnwrap(obj));
        MOZ_ASSERT(unwrapped);
        JSAutoCompartment ac(context(), unwrapped);
        if (!MapObject::getKeysAndValuesInterleaved(context(), unwrapped, &newEntries))
            return false;
    }
    if (!context()->compartment()->wrap(context(), &newEntries))
        return false;

    for (size_t i = newEntries.length(); i > 0; --i) {
        if (!entries.append(newEntries[i - 1]))
            return false;
    }

    /* Push obj and count to the stack. */
    if (!objs.append(ObjectValue(*obj)) || !counts.append(newEntries.length()))
        return false;

    checkStack();

    /* Write the header for obj. */
    return out.writePair(SCTAG_MAP_OBJECT, 0);
}

bool
JSStructuredCloneWriter::traverseSet(HandleObject obj)
{
    Rooted<GCVector<Value>> keys(context(), GCVector<Value>(context()));
    {
        // If there is no wrapper, the compartment munging is a no-op.
        RootedObject unwrapped(context(), CheckedUnwrap(obj));
        MOZ_ASSERT(unwrapped);
        JSAutoCompartment ac(context(), unwrapped);
        if (!SetObject::keys(context(), unwrapped, &keys))
            return false;
    }
    if (!context()->compartment()->wrap(context(), &keys))
        return false;

    for (size_t i = keys.length(); i > 0; --i) {
        if (!entries.append(keys[i - 1]))
            return false;
    }

    /* Push obj and count to the stack. */
    if (!objs.append(ObjectValue(*obj)) || !counts.append(keys.length()))
        return false;

    checkStack();

    /* Write the header for obj. */
    return out.writePair(SCTAG_SET_OBJECT, 0);
}

// Objects are written as a "preorder" traversal of the object graph: object
// "headers" (the class tag and any data needed for initial construction) are
// visited first, then the children are recursed through (where children are
// properties, Set or Map entries, etc.). So for example
//
//     m = new Map();
//     m.set(key1 = {}, value1 = {})
//
// would be stored as
//
//     <Map tag>
//     <key1 class tag>
//     <value1 class tag>
//     <end-of-children marker for key1>
//     <end-of-children marker for value1>
//     <end-of-children marker for Map>
//
// Notice how the end-of-children marker for key1 is sandwiched between the
// value1 beginning and end.
bool
JSStructuredCloneWriter::traverseSavedFrame(HandleObject obj)
{
    RootedObject unwrapped(context(), js::CheckedUnwrap(obj));
    MOZ_ASSERT(unwrapped && unwrapped->is<SavedFrame>());

    RootedSavedFrame savedFrame(context(), &unwrapped->as<SavedFrame>());

    RootedObject parent(context(), savedFrame->getParent());
    if (!context()->compartment()->wrap(context(), &parent))
        return false;

    if (!objs.append(ObjectValue(*obj)) ||
        !entries.append(parent ? ObjectValue(*parent) : NullValue()) ||
        !counts.append(1))
    {
        return false;
    }

    checkStack();

    // Write the SavedFrame tag and the SavedFrame's principals.

    if (savedFrame->getPrincipals() == &ReconstructedSavedFramePrincipals::IsSystem) {
        if (!out.writePair(SCTAG_SAVED_FRAME_OBJECT,
                           SCTAG_RECONSTRUCTED_SAVED_FRAME_PRINCIPALS_IS_SYSTEM))
        {
            return false;
        };
    } else if (savedFrame->getPrincipals() == &ReconstructedSavedFramePrincipals::IsNotSystem) {
        if (!out.writePair(SCTAG_SAVED_FRAME_OBJECT,
                           SCTAG_RECONSTRUCTED_SAVED_FRAME_PRINCIPALS_IS_NOT_SYSTEM))
        {
            return false;
        }
    } else {
        if (auto principals = savedFrame->getPrincipals()) {
            if (!out.writePair(SCTAG_SAVED_FRAME_OBJECT, SCTAG_JSPRINCIPALS) ||
                !principals->write(context(), this))
            {
                return false;
            }
        } else {
            if (!out.writePair(SCTAG_SAVED_FRAME_OBJECT, SCTAG_NULL_JSPRINCIPALS))
                return false;
        }
    }

    // Write the SavedFrame's reserved slots, except for the parent, which is
    // queued on objs for further traversal.

    RootedValue val(context());

    context()->markAtom(savedFrame->getSource());
    val = StringValue(savedFrame->getSource());
    if (!startWrite(val))
        return false;

    val = NumberValue(savedFrame->getLine());
    if (!startWrite(val))
        return false;

    val = NumberValue(savedFrame->getColumn());
    if (!startWrite(val))
        return false;

    auto name = savedFrame->getFunctionDisplayName();
    context()->markAtom(name);
    val = name ? StringValue(name) : NullValue();
    if (!startWrite(val))
        return false;

    auto cause = savedFrame->getAsyncCause();
    context()->markAtom(cause);
    val = cause ? StringValue(cause) : NullValue();
    if (!startWrite(val))
        return false;

    return true;
}

bool
JSStructuredCloneWriter::startWrite(HandleValue v)
{
    assertSameCompartment(context(), v);

    if (v.isString()) {
        return writeString(SCTAG_STRING, v.toString());
    } else if (v.isInt32()) {
        return out.writePair(SCTAG_INT32, v.toInt32());
    } else if (v.isDouble()) {
        return out.writeDouble(v.toDouble());
    } else if (v.isBoolean()) {
        return out.writePair(SCTAG_BOOLEAN, v.toBoolean());
    } else if (v.isNull()) {
        return out.writePair(SCTAG_NULL, 0);
    } else if (v.isUndefined()) {
        return out.writePair(SCTAG_UNDEFINED, 0);
    } else if (v.isObject()) {
        RootedObject obj(context(), &v.toObject());

        bool backref;
        if (!startObject(obj, &backref))
            return false;
        if (backref)
            return true;

        ESClass cls;
        if (!GetBuiltinClass(context(), obj, &cls))
            return false;

        if (cls == ESClass::RegExp) {
            RegExpGuard re(context());
            if (!RegExpToShared(context(), obj, &re))
                return false;
            return out.writePair(SCTAG_REGEXP_OBJECT, re->getFlags()) &&
                   writeString(SCTAG_STRING, re->getSource());
        } else if (cls == ESClass::Date) {
            RootedValue unboxed(context());
            if (!Unbox(context(), obj, &unboxed))
                return false;
            return out.writePair(SCTAG_DATE_OBJECT, 0) && out.writeDouble(unboxed.toNumber());
        } else if (JS_IsTypedArrayObject(obj)) {
            return writeTypedArray(obj);
        } else if (JS_IsDataViewObject(obj)) {
            return writeDataView(obj);
        } else if (JS_IsArrayBufferObject(obj) && JS_ArrayBufferHasData(obj)) {
            return writeArrayBuffer(obj);
        } else if (JS_IsSharedArrayBufferObject(obj)) {
            return writeSharedArrayBuffer(obj);
        } else if (cls == ESClass::Object) {
            return traverseObject(obj);
        } else if (cls == ESClass::Array) {
            return traverseObject(obj);
        } else if (cls == ESClass::Boolean) {
            RootedValue unboxed(context());
            if (!Unbox(context(), obj, &unboxed))
                return false;
            return out.writePair(SCTAG_BOOLEAN_OBJECT, unboxed.toBoolean());
        } else if (cls == ESClass::Number) {
            RootedValue unboxed(context());
            if (!Unbox(context(), obj, &unboxed))
                return false;
            return out.writePair(SCTAG_NUMBER_OBJECT, 0) && out.writeDouble(unboxed.toNumber());
        } else if (cls == ESClass::String) {
            RootedValue unboxed(context());
            if (!Unbox(context(), obj, &unboxed))
                return false;
            return writeString(SCTAG_STRING_OBJECT, unboxed.toString());
        } else if (cls == ESClass::Map) {
            return traverseMap(obj);
        } else if (cls == ESClass::Set) {
            return traverseSet(obj);
        } else if (SavedFrame::isSavedFrameOrWrapperAndNotProto(*obj)) {
            return traverseSavedFrame(obj);
        }

        if (callbacks && callbacks->write)
            return callbacks->write(context(), this, obj, closure);
        /* else fall through */
    }

    return reportDataCloneError(JS_SCERR_UNSUPPORTED_TYPE);
}

bool
JSStructuredCloneWriter::writeHeader()
{
    return out.writePair(SCTAG_HEADER, (uint32_t)scope);
}

bool
JSStructuredCloneWriter::writeTransferMap()
{
    if (transferableObjects.empty())
        return true;

    if (!out.writePair(SCTAG_TRANSFER_MAP_HEADER, (uint32_t)SCTAG_TM_UNREAD))
        return false;

    if (!out.write(transferableObjects.count()))
        return false;

    RootedObject obj(context());
    for (auto tr = transferableObjects.all(); !tr.empty(); tr.popFront()) {
        obj = tr.front();
        if (!memory.put(obj, memory.count())) {
            ReportOutOfMemory(context());
            return false;
        }

        // Emit a placeholder pointer.  We defer stealing the data until later
        // (and, if necessary, detaching this object if it's an ArrayBuffer).
        if (!out.writePair(SCTAG_TRANSFER_MAP_PENDING_ENTRY, JS::SCTAG_TMO_UNFILLED))
            return false;
        if (!out.writePtr(nullptr)) // Pointer to ArrayBuffer contents.
            return false;
        if (!out.write(0)) // extraData
            return false;
    }

    return true;
}

bool
JSStructuredCloneWriter::transferOwnership()
{
    if (transferableObjects.empty())
        return true;

    // Walk along the transferables and the transfer map at the same time,
    // grabbing out pointers from the transferables and stuffing them into the
    // transfer map.
    auto point = out.iter();
    MOZ_ASSERT(uint32_t(NativeEndian::swapFromLittleEndian(point.peek()) >> 32) == SCTAG_HEADER);
    point++;
    MOZ_ASSERT(uint32_t(NativeEndian::swapFromLittleEndian(point.peek()) >> 32) == SCTAG_TRANSFER_MAP_HEADER);
    point++;
    MOZ_ASSERT(NativeEndian::swapFromLittleEndian(point.peek()) == transferableObjects.count());
    point++;

    JSContext* cx = context();
    RootedObject obj(cx);
    for (auto tr = transferableObjects.all(); !tr.empty(); tr.popFront()) {
        obj = tr.front();

        uint32_t tag;
        JS::TransferableOwnership ownership;
        void* content;
        uint64_t extraData;

#if DEBUG
        SCInput::getPair(point.peek(), &tag, (uint32_t*) &ownership);
        MOZ_ASSERT(tag == SCTAG_TRANSFER_MAP_PENDING_ENTRY);
        MOZ_ASSERT(ownership == JS::SCTAG_TMO_UNFILLED);
#endif

        ESClass cls;
        if (!GetBuiltinClass(cx, obj, &cls))
            return false;

        if (cls == ESClass::ArrayBuffer) {
            tag = SCTAG_TRANSFER_MAP_ARRAY_BUFFER;

            // The current setup of the array buffer inheritance hierarchy doesn't
            // lend itself well to generic manipulation via proxies.
            Rooted<ArrayBufferObject*> arrayBuffer(cx, &CheckedUnwrap(obj)->as<ArrayBufferObject>());
            JSAutoCompartment ac(cx, arrayBuffer);
            size_t nbytes = arrayBuffer->byteLength();

            if (arrayBuffer->isWasm() || arrayBuffer->isPreparedForAsmJS()) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_NO_TRANSFER);
                return false;
            }

            if (scope == JS::StructuredCloneScope::DifferentProcess) {
                // Write Transferred ArrayBuffers in DifferentProcess scope at
                // the end of the clone buffer, and store the offset within the
                // buffer to where the ArrayBuffer was written. Note that this
                // will invalidate the current position iterator.

                size_t pointOffset = out.offset(point);
                tag = SCTAG_TRANSFER_MAP_STORED_ARRAY_BUFFER;
                ownership = JS::SCTAG_TMO_UNOWNED;
                content = nullptr;
                extraData = out.tell() - pointOffset; // Offset from tag to current end of buffer
                if (!writeArrayBuffer(arrayBuffer))
                    return false;

                // Must refresh the point iterator after its collection has
                // been modified.
                point = out.iter();
                point += pointOffset;

                if (!JS_DetachArrayBuffer(cx, arrayBuffer))
                    return false;
            } else {
                bool hasStealableContents = arrayBuffer->hasStealableContents();

                ArrayBufferObject::BufferContents bufContents =
                    ArrayBufferObject::stealContents(cx, arrayBuffer, hasStealableContents);
                if (!bufContents)
                    return false; // already transferred data

                content = bufContents.data();
                if (bufContents.kind() == ArrayBufferObject::MAPPED)
                    ownership = JS::SCTAG_TMO_MAPPED_DATA;
                else
                    ownership = JS::SCTAG_TMO_ALLOC_DATA;
                extraData = nbytes;
            }
        } else {
            if (!callbacks || !callbacks->writeTransfer)
                return reportDataCloneError(JS_SCERR_TRANSFERABLE);
            if (!callbacks->writeTransfer(cx, obj, closure, &tag, &ownership, &content, &extraData))
                return false;
            MOZ_ASSERT(tag > SCTAG_TRANSFER_MAP_PENDING_ENTRY);
        }

        point.write(NativeEndian::swapToLittleEndian(PairToUInt64(tag, ownership)));
        point.next();
        point.write(NativeEndian::swapToLittleEndian(reinterpret_cast<uint64_t>(content)));
        point.next();
        point.write(NativeEndian::swapToLittleEndian(extraData));
        point.next();
    }

#if DEBUG
    // Make sure there aren't any more transfer map entries after the expected
    // number we read out.
    if (!point.done()) {
        uint32_t tag, data;
        SCInput::getPair(point.peek(), &tag, &data);
        MOZ_ASSERT(tag < SCTAG_TRANSFER_MAP_HEADER || tag >= SCTAG_TRANSFER_MAP_END_OF_BUILTIN_TYPES);
    }
#endif
    return true;
}

bool
JSStructuredCloneWriter::write(HandleValue v)
{
    if (!startWrite(v))
        return false;

    while (!counts.empty()) {
        RootedObject obj(context(), &objs.back().toObject());
        AutoCompartment ac(context(), obj);
        if (counts.back()) {
            counts.back()--;
            RootedValue key(context(), entries.back());
            entries.popBack();
            checkStack();

            ESClass cls;
            if (!GetBuiltinClass(context(), obj, &cls))
                return false;

            if (cls == ESClass::Map) {
                counts.back()--;
                RootedValue val(context(), entries.back());
                entries.popBack();
                checkStack();

                if (!startWrite(key) || !startWrite(val))
                    return false;
            } else if (cls == ESClass::Set || SavedFrame::isSavedFrameOrWrapperAndNotProto(*obj)) {
                if (!startWrite(key))
                    return false;
            } else {
                RootedId id(context());
                if (!ValueToId<CanGC>(context(), key, &id))
                  return false;
                MOZ_ASSERT(JSID_IS_STRING(id) || JSID_IS_INT(id));

                /*
                 * If obj still has an own property named id, write it out.
                 * The cost of re-checking could be avoided by using
                 * NativeIterators.
                 */
                bool found;
                if (!HasOwnProperty(context(), obj, id, &found))
                    return false;

                if (found) {
                    RootedValue val(context());
                    if (!startWrite(key) ||
                        !GetProperty(context(), obj, obj, id, &val) ||
                        !startWrite(val))
                    {
                        return false;
                    }
                }
            }
        } else {
            if (!out.writePair(SCTAG_END_OF_KEYS, 0))
                return false;
            objs.popBack();
            counts.popBack();
        }
    }

    memory.clear();
    return transferOwnership();
}

bool
JSStructuredCloneReader::checkDouble(double d)
{
    if (!JS::IsCanonicalized(d)) {
        JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr, JSMSG_SC_BAD_SERIALIZED_DATA,
                                  "unrecognized NaN");
        return false;
    }
    return true;
}

namespace {

template <typename CharT>
class Chars {
    JSContext* cx;
    CharT* p;
  public:
    explicit Chars(JSContext* cx) : cx(cx), p(nullptr) {}
    ~Chars() { js_free(p); }

    bool allocate(size_t len) {
        MOZ_ASSERT(!p);
        // We're going to null-terminate!
        p = cx->pod_malloc<CharT>(len + 1);
        if (p) {
            p[len] = CharT(0);
            return true;
        }
        return false;
    }
    CharT* get() { return p; }
    void forget() { p = nullptr; }
};

} /* anonymous namespace */

template <typename CharT>
JSString*
JSStructuredCloneReader::readStringImpl(uint32_t nchars)
{
    if (nchars > JSString::MAX_LENGTH) {
        JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr, JSMSG_SC_BAD_SERIALIZED_DATA,
                                  "string length");
        return nullptr;
    }
    Chars<CharT> chars(context());
    if (!chars.allocate(nchars) || !in.readChars(chars.get(), nchars))
        return nullptr;
    JSString* str = NewString<CanGC>(context(), chars.get(), nchars);
    if (str)
        chars.forget();
    return str;
}

JSString*
JSStructuredCloneReader::readString(uint32_t data)
{
    uint32_t nchars = data & JS_BITMASK(31);
    bool latin1 = data & (1 << 31);
    return latin1 ? readStringImpl<Latin1Char>(nchars) : readStringImpl<char16_t>(nchars);
}

static uint32_t
TagToV1ArrayType(uint32_t tag)
{
    MOZ_ASSERT(tag >= SCTAG_TYPED_ARRAY_V1_MIN && tag <= SCTAG_TYPED_ARRAY_V1_MAX);
    return tag - SCTAG_TYPED_ARRAY_V1_MIN;
}

bool
JSStructuredCloneReader::readTypedArray(uint32_t arrayType, uint32_t nelems, MutableHandleValue vp,
                                        bool v1Read)
{
    if (arrayType > Scalar::Uint8Clamped) {
        JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr, JSMSG_SC_BAD_SERIALIZED_DATA,
                                  "unhandled typed array element type");
        return false;
    }

    // Push a placeholder onto the allObjs list to stand in for the typed array
    uint32_t placeholderIndex = allObjs.length();
    Value dummy = UndefinedValue();
    if (!allObjs.append(dummy))
        return false;

    // Read the ArrayBuffer object and its contents (but no properties)
    RootedValue v(context());
    uint32_t byteOffset;
    if (v1Read) {
        if (!readV1ArrayBuffer(arrayType, nelems, &v))
            return false;
        byteOffset = 0;
    } else {
        if (!startRead(&v))
            return false;
        uint64_t n;
        if (!in.read(&n))
            return false;
        byteOffset = n;
    }
    RootedObject buffer(context(), &v.toObject());
    RootedObject obj(context(), nullptr);

    switch (arrayType) {
      case Scalar::Int8:
        obj = JS_NewInt8ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case Scalar::Uint8:
        obj = JS_NewUint8ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case Scalar::Int16:
        obj = JS_NewInt16ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case Scalar::Uint16:
        obj = JS_NewUint16ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case Scalar::Int32:
        obj = JS_NewInt32ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case Scalar::Uint32:
        obj = JS_NewUint32ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case Scalar::Float32:
        obj = JS_NewFloat32ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case Scalar::Float64:
        obj = JS_NewFloat64ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case Scalar::Uint8Clamped:
        obj = JS_NewUint8ClampedArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      default:
        MOZ_CRASH("Can't happen: arrayType range checked above");
    }

    if (!obj)
        return false;
    vp.setObject(*obj);

    allObjs[placeholderIndex].set(vp);

    return true;
}

bool
JSStructuredCloneReader::readDataView(uint32_t byteLength, MutableHandleValue vp)
{
    // Push a placeholder onto the allObjs list to stand in for the DataView.
    uint32_t placeholderIndex = allObjs.length();
    Value dummy = UndefinedValue();
    if (!allObjs.append(dummy))
        return false;

    // Read the ArrayBuffer object and its contents (but no properties).
    RootedValue v(context());
    if (!startRead(&v))
        return false;

    // Read byteOffset.
    uint64_t n;
    if (!in.read(&n))
        return false;
    uint32_t byteOffset = n;

    RootedObject buffer(context(), &v.toObject());
    RootedObject obj(context(), JS_NewDataView(context(), buffer, byteOffset, byteLength));
    if (!obj)
        return false;
    vp.setObject(*obj);

    allObjs[placeholderIndex].set(vp);

    return true;
}

bool
JSStructuredCloneReader::readArrayBuffer(uint32_t nbytes, MutableHandleValue vp)
{
    JSObject* obj = ArrayBufferObject::create(context(), nbytes);
    if (!obj)
        return false;
    vp.setObject(*obj);
    ArrayBufferObject& buffer = obj->as<ArrayBufferObject>();
    MOZ_ASSERT(buffer.byteLength() == nbytes);
    return in.readArray(buffer.dataPointer(), nbytes);
}

bool
JSStructuredCloneReader::readSharedArrayBuffer(uint32_t nbytes, MutableHandleValue vp)
{
    intptr_t p;
    in.readBytes(&p, sizeof(p));

    SharedArrayRawBuffer* rawbuf = reinterpret_cast<SharedArrayRawBuffer*>(p);

    // There's no guarantee that the receiving agent has enabled shared memory
    // even if the transmitting agent has done so.  Ideally we'd check at the
    // transmission point, but that's tricky, and it will be a very rare problem
    // in any case.  Just fail at the receiving end if we can't handle it.

    if (!context()->compartment()->creationOptions().getSharedMemoryAndAtomicsEnabled()) {
        // The sending side performed a reference increment before sending.
        // Account for that here before leaving.
        if (rawbuf)
            rawbuf->dropReference();

        JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr, JSMSG_SC_SAB_DISABLED);
        return false;
    }

    // The constructor absorbs the reference count increment performed by the sender.
    JSObject* obj = SharedArrayBufferObject::New(context(), rawbuf);

    vp.setObject(*obj);
    return true;
}

/*
 * Read in the data for a structured clone version 1 ArrayBuffer, performing
 * endianness-conversion while reading.
 */
bool
JSStructuredCloneReader::readV1ArrayBuffer(uint32_t arrayType, uint32_t nelems,
                                           MutableHandleValue vp)
{
    MOZ_ASSERT(arrayType <= Scalar::Uint8Clamped);

    uint32_t nbytes = nelems << TypedArrayShift(static_cast<Scalar::Type>(arrayType));
    JSObject* obj = ArrayBufferObject::create(context(), nbytes);
    if (!obj)
        return false;
    vp.setObject(*obj);
    ArrayBufferObject& buffer = obj->as<ArrayBufferObject>();
    MOZ_ASSERT(buffer.byteLength() == nbytes);

    switch (arrayType) {
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Uint8Clamped:
        return in.readArray((uint8_t*) buffer.dataPointer(), nelems);
      case Scalar::Int16:
      case Scalar::Uint16:
        return in.readArray((uint16_t*) buffer.dataPointer(), nelems);
      case Scalar::Int32:
      case Scalar::Uint32:
      case Scalar::Float32:
        return in.readArray((uint32_t*) buffer.dataPointer(), nelems);
      case Scalar::Float64:
        return in.readArray((uint64_t*) buffer.dataPointer(), nelems);
      default:
        MOZ_CRASH("Can't happen: arrayType range checked by caller");
    }
}

static bool
PrimitiveToObject(JSContext* cx, MutableHandleValue vp)
{
    JSObject* obj = js::PrimitiveToObject(cx, vp);
    if (!obj)
        return false;

    vp.setObject(*obj);
    return true;
}

bool
JSStructuredCloneReader::startRead(MutableHandleValue vp)
{
    uint32_t tag, data;

    if (!in.readPair(&tag, &data))
        return false;

    switch (tag) {
      case SCTAG_NULL:
        vp.setNull();
        break;

      case SCTAG_UNDEFINED:
        vp.setUndefined();
        break;

      case SCTAG_INT32:
        vp.setInt32(data);
        break;

      case SCTAG_BOOLEAN:
      case SCTAG_BOOLEAN_OBJECT:
        vp.setBoolean(!!data);
        if (tag == SCTAG_BOOLEAN_OBJECT && !PrimitiveToObject(context(), vp))
            return false;
        break;

      case SCTAG_STRING:
      case SCTAG_STRING_OBJECT: {
        JSString* str = readString(data);
        if (!str)
            return false;
        vp.setString(str);
        if (tag == SCTAG_STRING_OBJECT && !PrimitiveToObject(context(), vp))
            return false;
        break;
      }

      case SCTAG_NUMBER_OBJECT: {
        double d;
        if (!in.readDouble(&d) || !checkDouble(d))
            return false;
        vp.setDouble(d);
        if (!PrimitiveToObject(context(), vp))
            return false;
        break;
      }

      case SCTAG_DATE_OBJECT: {
        double d;
        if (!in.readDouble(&d) || !checkDouble(d))
            return false;
        JS::ClippedTime t = JS::TimeClip(d);
        if (!NumbersAreIdentical(d, t.toDouble())) {
            JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr,
                                      JSMSG_SC_BAD_SERIALIZED_DATA,
                                      "date");
            return false;
        }
        JSObject* obj = NewDateObjectMsec(context(), t);
        if (!obj)
            return false;
        vp.setObject(*obj);
        break;
      }

      case SCTAG_REGEXP_OBJECT: {
        RegExpFlag flags = RegExpFlag(data);
        uint32_t tag2, stringData;
        if (!in.readPair(&tag2, &stringData))
            return false;
        if (tag2 != SCTAG_STRING) {
            JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr,
                                      JSMSG_SC_BAD_SERIALIZED_DATA,
                                      "regexp");
            return false;
        }
        JSString* str = readString(stringData);
        if (!str)
            return false;

        RootedAtom atom(context(), AtomizeString(context(), str));
        if (!atom)
            return false;

        RegExpObject* reobj = RegExpObject::create(context(), atom, flags, nullptr,
                                                   context()->tempLifoAlloc());
        if (!reobj)
            return false;
        vp.setObject(*reobj);
        break;
      }

      case SCTAG_ARRAY_OBJECT:
      case SCTAG_OBJECT_OBJECT: {
        JSObject* obj = (tag == SCTAG_ARRAY_OBJECT)
                        ? (JSObject*) NewDenseEmptyArray(context())
                        : (JSObject*) NewBuiltinClassInstance<PlainObject>(context());
        if (!obj || !objs.append(ObjectValue(*obj)))
            return false;
        vp.setObject(*obj);
        break;
      }

      case SCTAG_BACK_REFERENCE_OBJECT: {
        if (data >= allObjs.length() || !allObjs[data].isObject()) {
            JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr,
                                      JSMSG_SC_BAD_SERIALIZED_DATA,
                                      "invalid back reference in input");
            return false;
        }
        vp.set(allObjs[data]);
        return true;
      }

      case SCTAG_TRANSFER_MAP_HEADER:
      case SCTAG_TRANSFER_MAP_PENDING_ENTRY:
        // We should be past all the transfer map tags.
        JS_ReportErrorNumberASCII(context(), GetErrorMessage, NULL, JSMSG_SC_BAD_SERIALIZED_DATA,
                                  "invalid input");
        return false;

      case SCTAG_ARRAY_BUFFER_OBJECT:
        if (!readArrayBuffer(data, vp))
            return false;
        break;

      case SCTAG_SHARED_ARRAY_BUFFER_OBJECT:
        if (!readSharedArrayBuffer(data, vp))
            return false;
        break;

      case SCTAG_TYPED_ARRAY_OBJECT: {
        // readTypedArray adds the array to allObjs.
        uint64_t arrayType;
        if (!in.read(&arrayType))
            return false;
        return readTypedArray(arrayType, data, vp);
      }

      case SCTAG_DATA_VIEW_OBJECT: {
        // readDataView adds the array to allObjs.
        return readDataView(data, vp);
      }

      case SCTAG_MAP_OBJECT: {
        JSObject* obj = MapObject::create(context());
        if (!obj || !objs.append(ObjectValue(*obj)))
            return false;
        vp.setObject(*obj);
        break;
      }

      case SCTAG_SET_OBJECT: {
        JSObject* obj = SetObject::create(context());
        if (!obj || !objs.append(ObjectValue(*obj)))
            return false;
        vp.setObject(*obj);
        break;
      }

      case SCTAG_SAVED_FRAME_OBJECT: {
        auto obj = readSavedFrame(data);
        if (!obj || !objs.append(ObjectValue(*obj)))
            return false;
        vp.setObject(*obj);
        break;
      }

      default: {
        if (tag <= SCTAG_FLOAT_MAX) {
            double d = ReinterpretPairAsDouble(tag, data);
            if (!checkDouble(d))
                return false;
            vp.setNumber(d);
            break;
        }

        if (SCTAG_TYPED_ARRAY_V1_MIN <= tag && tag <= SCTAG_TYPED_ARRAY_V1_MAX) {
            // A v1-format typed array
            // readTypedArray adds the array to allObjs
            return readTypedArray(TagToV1ArrayType(tag), data, vp, true);
        }

        if (!callbacks || !callbacks->read) {
            JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr,
                                      JSMSG_SC_BAD_SERIALIZED_DATA,
                                      "unsupported type");
            return false;
        }
        JSObject* obj = callbacks->read(context(), this, tag, data, closure);
        if (!obj)
            return false;
        vp.setObject(*obj);
      }
    }

    if (vp.isObject() && !allObjs.append(vp))
        return false;

    return true;
}

bool
JSStructuredCloneReader::readHeader()
{
    uint32_t tag, data;
    if (!in.getPair(&tag, &data))
        return in.reportTruncated();

    if (tag != SCTAG_HEADER) {
        // Old structured clone buffer. We must have read it from disk or
        // somewhere, so we can assume it's scope-compatible.
        return true;
    }

    MOZ_ALWAYS_TRUE(in.readPair(&tag, &data));
    storedScope = JS::StructuredCloneScope(data);
    if (storedScope < allowedScope) {
        JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr, JSMSG_SC_BAD_SERIALIZED_DATA,
                                  "incompatible structured clone scope");
        return false;
    }

    return true;
}

bool
JSStructuredCloneReader::readTransferMap()
{
    JSContext* cx = context();
    auto headerPos = in.tell();

    uint32_t tag, data;
    if (!in.getPair(&tag, &data))
        return in.reportTruncated();

    if (tag != SCTAG_TRANSFER_MAP_HEADER || TransferableMapHeader(data) == SCTAG_TM_TRANSFERRED)
        return true;

    uint64_t numTransferables;
    MOZ_ALWAYS_TRUE(in.readPair(&tag, &data));
    if (!in.read(&numTransferables))
        return false;

    for (uint64_t i = 0; i < numTransferables; i++) {
        auto pos = in.tell();

        if (!in.readPair(&tag, &data))
            return false;

        MOZ_ASSERT(tag != SCTAG_TRANSFER_MAP_PENDING_ENTRY);
        RootedObject obj(cx);

        void* content;
        if (!in.readPtr(&content))
            return false;

        uint64_t extraData;
        if (!in.read(&extraData))
            return false;

        if (tag == SCTAG_TRANSFER_MAP_ARRAY_BUFFER) {
            if (storedScope == JS::StructuredCloneScope::DifferentProcess) {
                // Transferred ArrayBuffers in a DifferentProcess clone buffer
                // are treated as if they weren't Transferred at all.
                continue;
            }

            size_t nbytes = extraData;
            MOZ_ASSERT(data == JS::SCTAG_TMO_ALLOC_DATA ||
                       data == JS::SCTAG_TMO_MAPPED_DATA);
            if (data == JS::SCTAG_TMO_ALLOC_DATA)
                obj = JS_NewArrayBufferWithContents(cx, nbytes, content);
            else if (data == JS::SCTAG_TMO_MAPPED_DATA)
                obj = JS_NewMappedArrayBufferWithContents(cx, nbytes, content);
        } else if (tag == SCTAG_TRANSFER_MAP_STORED_ARRAY_BUFFER) {
            auto savedPos = in.tell();
            auto guard = mozilla::MakeScopeExit([&] {
                in.seekTo(savedPos);
            });
            in.seekTo(pos);
            in.seekBy(static_cast<size_t>(extraData));

            uint32_t tag, data;
            if (!in.readPair(&tag, &data))
                return false;
            MOZ_ASSERT(tag == SCTAG_ARRAY_BUFFER_OBJECT);
            RootedValue val(cx);
            if (!readArrayBuffer(data, &val))
                return false;
            obj = &val.toObject();
        } else {
            if (!callbacks || !callbacks->readTransfer) {
                ReportDataCloneError(cx, callbacks, JS_SCERR_TRANSFERABLE);
                return false;
            }
            if (!callbacks->readTransfer(cx, this, tag, content, extraData, closure, &obj))
                return false;
            MOZ_ASSERT(obj);
            MOZ_ASSERT(!cx->isExceptionPending());
        }

        // On failure, the buffer will still own the data (since its ownership
        // will not get set to SCTAG_TMO_UNOWNED), so the data will be freed by
        // DiscardTransferables.
        if (!obj)
            return false;

        // Mark the SCTAG_TRANSFER_MAP_* entry as no longer owned by the input
        // buffer.
        pos.write(PairToUInt64(tag, JS::SCTAG_TMO_UNOWNED));
        MOZ_ASSERT(!pos.done());

        if (!allObjs.append(ObjectValue(*obj)))
            return false;
    }

    // Mark the whole transfer map as consumed.
#ifdef DEBUG
    SCInput::getPair(headerPos.peek(), &tag, &data);
    MOZ_ASSERT(tag == SCTAG_TRANSFER_MAP_HEADER);
    MOZ_ASSERT(TransferableMapHeader(data) != SCTAG_TM_TRANSFERRED);
#endif
    headerPos.write(PairToUInt64(SCTAG_TRANSFER_MAP_HEADER, SCTAG_TM_TRANSFERRED));

    return true;
}

JSObject*
JSStructuredCloneReader::readSavedFrame(uint32_t principalsTag)
{
    RootedSavedFrame savedFrame(context(), SavedFrame::create(context()));
    if (!savedFrame)
        return nullptr;

    JSPrincipals* principals;
    if (principalsTag == SCTAG_JSPRINCIPALS) {
        if (!context()->runtime()->readPrincipals) {
            JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr,
                                      JSMSG_SC_UNSUPPORTED_TYPE);
            return nullptr;
        }

        if (!context()->runtime()->readPrincipals(context(), this, &principals))
            return nullptr;
    } else if (principalsTag == SCTAG_RECONSTRUCTED_SAVED_FRAME_PRINCIPALS_IS_SYSTEM) {
        principals = &ReconstructedSavedFramePrincipals::IsSystem;
        principals->refcount++;
    } else if (principalsTag == SCTAG_RECONSTRUCTED_SAVED_FRAME_PRINCIPALS_IS_NOT_SYSTEM) {
        principals = &ReconstructedSavedFramePrincipals::IsNotSystem;
        principals->refcount++;
    } else if (principalsTag == SCTAG_NULL_JSPRINCIPALS) {
        principals = nullptr;
    } else {
        JS_ReportErrorNumberASCII(context(), GetErrorMessage, nullptr, JSMSG_SC_BAD_SERIALIZED_DATA,
                                  "bad SavedFrame principals");
        return nullptr;
    }
    savedFrame->initPrincipalsAlreadyHeld(principals);

    RootedValue source(context());
    if (!startRead(&source) || !source.isString())
        return nullptr;
    auto atomSource = AtomizeString(context(), source.toString());
    if (!atomSource)
        return nullptr;
    savedFrame->initSource(atomSource);

    RootedValue lineVal(context());
    uint32_t line;
    if (!startRead(&lineVal) || !lineVal.isNumber() || !ToUint32(context(), lineVal, &line))
        return nullptr;
    savedFrame->initLine(line);

    RootedValue columnVal(context());
    uint32_t column;
    if (!startRead(&columnVal) || !columnVal.isNumber() || !ToUint32(context(), columnVal, &column))
        return nullptr;
    savedFrame->initColumn(column);

    RootedValue name(context());
    if (!startRead(&name) || !(name.isString() || name.isNull()))
        return nullptr;
    JSAtom* atomName = nullptr;
    if (name.isString()) {
        atomName = AtomizeString(context(), name.toString());
        if (!atomName)
            return nullptr;
    }

    savedFrame->initFunctionDisplayName(atomName);

    RootedValue cause(context());
    if (!startRead(&cause) || !(cause.isString() || cause.isNull()))
        return nullptr;
    JSAtom* atomCause = nullptr;
    if (cause.isString()) {
        atomCause = AtomizeString(context(), cause.toString());
        if (!atomCause)
            return nullptr;
    }
    savedFrame->initAsyncCause(atomCause);

    return savedFrame;
}

// Perform the whole recursive reading procedure.
bool
JSStructuredCloneReader::read(MutableHandleValue vp)
{
    if (!readHeader())
        return false;

    if (!readTransferMap())
        return false;

    // Start out by reading in the main object and pushing it onto the 'objs'
    // stack. The data related to this object and its descendants extends from
    // here to the SCTAG_END_OF_KEYS at the end of the stream.
    if (!startRead(vp))
        return false;

    // Stop when the stack shows that all objects have been read.
    while (objs.length() != 0) {
        // What happens depends on the top obj on the objs stack.
        RootedObject obj(context(), &objs.back().toObject());

        uint32_t tag, data;
        if (!in.getPair(&tag, &data))
            return false;

        if (tag == SCTAG_END_OF_KEYS) {
            // Pop the current obj off the stack, since we are done with it and
            // its children.
            MOZ_ALWAYS_TRUE(in.readPair(&tag, &data));
            objs.popBack();
            continue;
        }

        // The input stream contains a sequence of "child" values, whose
        // interpretation depends on the type of obj. These values can be
        // anything, and startRead() will push onto 'objs' for any non-leaf
        // value (i.e., anything that may contain children).
        //
        // startRead() will allocate the (empty) object, but note that when
        // startRead() returns, 'key' is not yet initialized with any of its
        // properties. Those will be filled in by returning to the head of this
        // loop, processing the first child obj, and continuing until all
        // children have been fully created.
        //
        // Note that this means the ordering in the stream is a little funky
        // for things like Map. See the comment above startWrite() for an
        // example.
        RootedValue key(context());
        if (!startRead(&key))
            return false;

        if (key.isNull() &&
            !(obj->is<MapObject>() || obj->is<SetObject>() || obj->is<SavedFrame>()))
        {
            // Backwards compatibility: Null formerly indicated the end of
            // object properties.
            objs.popBack();
            continue;
        }

        // Set object: the values between obj header (from startRead()) and
        // SCTAG_END_OF_KEYS are all interpreted as values to add to the set.
        if (obj->is<SetObject>()) {
            if (!SetObject::add(context(), obj, key))
                return false;
            continue;
        }

        // SavedFrame object: there is one following value, the parent
        // SavedFrame, which is either null or another SavedFrame object.
        if (obj->is<SavedFrame>()) {
            SavedFrame* parentFrame;
            if (key.isNull())
                parentFrame = nullptr;
            else if (key.isObject() && key.toObject().is<SavedFrame>())
                parentFrame = &key.toObject().as<SavedFrame>();
            else
                return false;

            obj->as<SavedFrame>().initParent(parentFrame);
            continue;
        }

        // Everything else uses a series of key,value,key,value,... Value
        // objects.
        RootedValue val(context());
        if (!startRead(&val))
            return false;

        if (obj->is<MapObject>()) {
            // For a Map, store those <key,value> pairs in the contained map
            // data structure.
            if (!MapObject::set(context(), obj, key, val))
                return false;
        } else {
            // For any other Object, interpret them as plain properties.
            RootedId id(context());
            if (!ValueToId<CanGC>(context(), key, &id))
                return false;

            if (!DefineProperty(context(), obj, id, val))
                return false;
         }
    }

    allObjs.clear();

    return true;
}

using namespace js;

JS_PUBLIC_API(bool)
JS_ReadStructuredClone(JSContext* cx, JSStructuredCloneData& buf,
                       uint32_t version, JS::StructuredCloneScope scope,
                       MutableHandleValue vp,
                       const JSStructuredCloneCallbacks* optionalCallbacks,
                       void* closure)
{
    AssertHeapIsIdle();
    CHECK_REQUEST(cx);

    if (version > JS_STRUCTURED_CLONE_VERSION) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SC_BAD_CLONE_VERSION);
        return false;
    }
    const JSStructuredCloneCallbacks* callbacks = optionalCallbacks;
    return ReadStructuredClone(cx, buf, scope, vp, callbacks, closure);
}

JS_PUBLIC_API(bool)
JS_WriteStructuredClone(JSContext* cx, HandleValue value, JSStructuredCloneData* bufp,
                        JS::StructuredCloneScope scope,
                        JS::CloneDataPolicy cloneDataPolicy,
                        const JSStructuredCloneCallbacks* optionalCallbacks,
                        void* closure, HandleValue transferable)
{
    AssertHeapIsIdle();
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);

    const JSStructuredCloneCallbacks* callbacks = optionalCallbacks;
    return WriteStructuredClone(cx, value, bufp, scope, cloneDataPolicy, callbacks, closure,
                                transferable);
}

JS_PUBLIC_API(bool)
JS_StructuredCloneHasTransferables(JSStructuredCloneData& data,
                                   bool* hasTransferable)
{
    *hasTransferable = StructuredCloneHasTransferObjects(data);
    return true;
}

JS_PUBLIC_API(bool)
JS_StructuredClone(JSContext* cx, HandleValue value, MutableHandleValue vp,
                   const JSStructuredCloneCallbacks* optionalCallbacks,
                   void* closure)
{
    AssertHeapIsIdle();
    CHECK_REQUEST(cx);

    // Strings are associated with zones, not compartments,
    // so we copy the string by wrapping it.
    if (value.isString()) {
      RootedString strValue(cx, value.toString());
      if (!cx->compartment()->wrap(cx, &strValue)) {
        return false;
      }
      vp.setString(strValue);
      return true;
    }

    const JSStructuredCloneCallbacks* callbacks = optionalCallbacks;

    JSAutoStructuredCloneBuffer buf(JS::StructuredCloneScope::SameProcessSameThread, callbacks, closure);
    {
        // If we use Maybe<AutoCompartment> here, G++ can't tell that the
        // destructor is only called when Maybe::construct was called, and
        // we get warnings about using uninitialized variables.
        if (value.isObject()) {
            AutoCompartment ac(cx, &value.toObject());
            if (!buf.write(cx, value, callbacks, closure))
                return false;
        } else {
            if (!buf.write(cx, value, callbacks, closure))
                return false;
        }
    }

    return buf.read(cx, vp, callbacks, closure);
}

JSAutoStructuredCloneBuffer::JSAutoStructuredCloneBuffer(JSAutoStructuredCloneBuffer&& other)
    : scope_(other.scope_)
{
    data_.ownTransferables_ = other.data_.ownTransferables_;
    other.steal(&data_, &version_, &data_.callbacks_, &data_.closure_);
}

JSAutoStructuredCloneBuffer&
JSAutoStructuredCloneBuffer::operator=(JSAutoStructuredCloneBuffer&& other)
{
    MOZ_ASSERT(&other != this);
    MOZ_ASSERT(scope_ == other.scope_);
    clear();
    data_.ownTransferables_ = other.data_.ownTransferables_;
    other.steal(&data_, &version_, &data_.callbacks_, &data_.closure_);
    return *this;
}

void
JSAutoStructuredCloneBuffer::clear(const JSStructuredCloneCallbacks* optionalCallbacks,
                                   void* optionalClosure)
{
    if (!data_.Size())
        return;

    const JSStructuredCloneCallbacks* callbacks =
        optionalCallbacks ?  optionalCallbacks : data_.callbacks_;
    void* closure = optionalClosure ?  optionalClosure : data_.closure_;

    if (data_.ownTransferables_ == OwnTransferablePolicy::OwnsTransferablesIfAny)
        DiscardTransferables(data_, callbacks, closure);
    data_.ownTransferables_ = OwnTransferablePolicy::NoTransferables;
    data_.Clear();
    version_ = 0;
}

bool
JSAutoStructuredCloneBuffer::copy(const JSStructuredCloneData& srcData, uint32_t version,
                                  const JSStructuredCloneCallbacks* callbacks,
                                  void* closure)
{
    // transferable objects cannot be copied
    if (StructuredCloneHasTransferObjects(srcData))
        return false;

    clear();

    auto iter = srcData.Iter();
    while (!iter.Done()) {
            data_.WriteBytes(iter.Data(), iter.RemainingInSegment());
            iter.Advance(srcData, iter.RemainingInSegment());
    }

    version_ = version;
    data_.setOptionalCallbacks(callbacks, closure, OwnTransferablePolicy::NoTransferables);
    return true;
}

void
JSAutoStructuredCloneBuffer::adopt(JSStructuredCloneData&& data, uint32_t version,
                                   const JSStructuredCloneCallbacks* callbacks,
                                   void* closure)
{
    clear();
    data_ = Move(data);
    version_ = version;
    data_.setOptionalCallbacks(callbacks, closure, OwnTransferablePolicy::OwnsTransferablesIfAny);
}

void
JSAutoStructuredCloneBuffer::steal(JSStructuredCloneData* data, uint32_t* versionp,
                                   const JSStructuredCloneCallbacks** callbacks,
                                   void** closure)
{
    if (versionp)
        *versionp = version_;
    if (callbacks)
        *callbacks = data_.callbacks_;
    if (closure)
        *closure = data_.closure_;
    *data = Move(data_);

    version_ = 0;
    data_.setOptionalCallbacks(nullptr, nullptr, OwnTransferablePolicy::NoTransferables);
}

bool
JSAutoStructuredCloneBuffer::read(JSContext* cx, MutableHandleValue vp,
                                  const JSStructuredCloneCallbacks* optionalCallbacks,
                                  void* closure)
{
    MOZ_ASSERT(cx);
    return !!JS_ReadStructuredClone(cx, data_, version_, scope_, vp,
                                    optionalCallbacks, closure);
}

bool
JSAutoStructuredCloneBuffer::write(JSContext* cx, HandleValue value,
                                   const JSStructuredCloneCallbacks* optionalCallbacks,
                                   void* closure)
{
    HandleValue transferable = UndefinedHandleValue;
    return write(cx, value, transferable, JS::CloneDataPolicy().denySharedArrayBuffer(), optionalCallbacks, closure);
}

bool
JSAutoStructuredCloneBuffer::write(JSContext* cx, HandleValue value,
                                   HandleValue transferable, JS::CloneDataPolicy cloneDataPolicy,
                                   const JSStructuredCloneCallbacks* optionalCallbacks,
                                   void* closure)
{
    clear();
    bool ok = JS_WriteStructuredClone(cx, value, &data_,
                                      scope_, cloneDataPolicy,
                                      optionalCallbacks, closure,
                                      transferable);

    if (ok) {
        data_.ownTransferables_ = OwnTransferablePolicy::OwnsTransferablesIfAny;
    } else {
        version_ = JS_STRUCTURED_CLONE_VERSION;
        data_.ownTransferables_ = OwnTransferablePolicy::NoTransferables;
    }
    return ok;
}

JS_PUBLIC_API(bool)
JS_ReadUint32Pair(JSStructuredCloneReader* r, uint32_t* p1, uint32_t* p2)
{
    return r->input().readPair((uint32_t*) p1, (uint32_t*) p2);
}

JS_PUBLIC_API(bool)
JS_ReadBytes(JSStructuredCloneReader* r, void* p, size_t len)
{
    return r->input().readBytes(p, len);
}

JS_PUBLIC_API(bool)
JS_ReadTypedArray(JSStructuredCloneReader* r, MutableHandleValue vp)
{
    uint32_t tag, nelems;
    if (!r->input().readPair(&tag, &nelems))
        return false;
    if (tag >= SCTAG_TYPED_ARRAY_V1_MIN && tag <= SCTAG_TYPED_ARRAY_V1_MAX) {
        return r->readTypedArray(TagToV1ArrayType(tag), nelems, vp, true);
    } else if (tag == SCTAG_TYPED_ARRAY_OBJECT) {
        uint64_t arrayType;
        if (!r->input().read(&arrayType))
            return false;
        return r->readTypedArray(arrayType, nelems, vp);
    } else {
        JS_ReportErrorNumberASCII(r->context(), GetErrorMessage, nullptr,
                                  JSMSG_SC_BAD_SERIALIZED_DATA,
                                  "expected type array");
        return false;
    }
}

JS_PUBLIC_API(bool)
JS_WriteUint32Pair(JSStructuredCloneWriter* w, uint32_t tag, uint32_t data)
{
    return w->output().writePair(tag, data);
}

JS_PUBLIC_API(bool)
JS_WriteBytes(JSStructuredCloneWriter* w, const void* p, size_t len)
{
    return w->output().writeBytes(p, len);
}

JS_PUBLIC_API(bool)
JS_WriteString(JSStructuredCloneWriter* w, HandleString str)
{
    return w->writeString(SCTAG_STRING, str);
}

JS_PUBLIC_API(bool)
JS_WriteTypedArray(JSStructuredCloneWriter* w, HandleValue v)
{
    MOZ_ASSERT(v.isObject());
    assertSameCompartment(w->context(), v);
    RootedObject obj(w->context(), &v.toObject());
    return w->writeTypedArray(obj);
}

JS_PUBLIC_API(bool)
JS_ObjectNotWritten(JSStructuredCloneWriter* w, HandleObject obj)
{
    w->memory.remove(w->memory.lookup(obj));

    return true;
}

JS_PUBLIC_API(JS::StructuredCloneScope)
JS_GetStructuredCloneScope(JSStructuredCloneWriter* w)
{
    return w->cloneScope();
}
