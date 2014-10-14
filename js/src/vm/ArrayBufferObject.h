/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ArrayBufferObject_h
#define vm_ArrayBufferObject_h

#include "jsobj.h"

#include "builtin/TypedObjectConstants.h"
#include "vm/Runtime.h"

typedef struct JSProperty JSProperty;

namespace js {

class ArrayBufferViewObject;

// The inheritance hierarchy for the various classes relating to typed arrays
// is as follows.
//
// - JSObject
//   - ArrayBufferObjectMaybeShared
//     - ArrayBufferObject
//     - SharedArrayBufferObject
//   - ArrayBufferViewObject
//     - DataViewObject
//     - TypedArrayObject (declared in vm/TypedArrayObject.h)
//       - TypedArrayObjectTemplate
//         - Int8ArrayObject
//         - Uint8ArrayObject
//         - ...
//     - TypedObject (declared in builtin/TypedObject.h)
//   - SharedTypedArrayObject (declared in vm/SharedTypedArrayObject.h)
//     - SharedTypedArrayObjectTemplate
//       - SharedInt8ArrayObject
//       - SharedUint8ArrayObject
//       - ...
//
// Note that |TypedArrayObjectTemplate| is just an implementation
// detail that makes implementing its various subclasses easier.
// Note that |TypedArrayObjectTemplate| and |SharedTypedArrayObjectTemplate| are
// just implementation details that make implementing their various subclasses easier.
//
// ArrayBufferObject and SharedArrayBufferObject are unrelated data types:
// the racy memory of the latter cannot substitute for the non-racy memory of
// the former; the non-racy memory of the former cannot be used with the atomics;
// the former can be neutered and the latter not; and they have different
// method suites.  Hence they have been separated completely.
//
// Most APIs will only accept ArrayBufferObject.  ArrayBufferObjectMaybeShared exists
// as a join point to allow APIs that can take or use either, notably AsmJS.
//
// As ArrayBufferObject and SharedArrayBufferObject are separated, so are the
// TypedArray hierarchies below the two.  However, the TypedArrays have the
// same layout (see TypedArrayObject.h), so there is little code duplication.

class ArrayBufferObjectMaybeShared;

uint32_t AnyArrayBufferByteLength(const ArrayBufferObjectMaybeShared *buf);
uint8_t *AnyArrayBufferDataPointer(const ArrayBufferObjectMaybeShared *buf);
ArrayBufferObjectMaybeShared &AsAnyArrayBuffer(HandleValue val);

class ArrayBufferObjectMaybeShared : public NativeObject
{
  public:
    uint32_t byteLength() {
        return AnyArrayBufferByteLength(this);
    }

    uint8_t *dataPointer() {
        return AnyArrayBufferDataPointer(this);
    }
};

/*
 * ArrayBufferObject
 *
 * This class holds the underlying raw buffer that the various
 * ArrayBufferViewObject subclasses (DataViewObject and the TypedArrays)
 * access. It can be created explicitly and passed to an ArrayBufferViewObject
 * subclass, or can be created implicitly by constructing a TypedArrayObject
 * with a size.
 *
 * ArrayBufferObject (or really the underlying memory) /is not racy/: the
 * memory is private to a single worker.
 */
class ArrayBufferObject : public ArrayBufferObjectMaybeShared
{
    static bool byteLengthGetterImpl(JSContext *cx, CallArgs args);
    static bool fun_slice_impl(JSContext *cx, CallArgs args);

  public:
    static const uint8_t DATA_SLOT = 0;
    static const uint8_t BYTE_LENGTH_SLOT = 1;
    static const uint8_t FIRST_VIEW_SLOT = 2;
    static const uint8_t FLAGS_SLOT = 3;

    static const uint8_t RESERVED_SLOTS = 4;

    static const size_t ARRAY_BUFFER_ALIGNMENT = 8;

  public:

    enum BufferKind {
        PLAIN_BUFFER        =   0, // malloced or inline data
        ASMJS_BUFFER        = 0x1,
        MAPPED_BUFFER       = 0x2,
        KIND_MASK           = ASMJS_BUFFER | MAPPED_BUFFER
    };

  protected:

    enum ArrayBufferFlags {
        // The flags also store the BufferKind
        BUFFER_KIND_MASK    = BufferKind::KIND_MASK,

        NEUTERED_BUFFER     = 0x4,

        // The dataPointer() is owned by this buffer and should be released
        // when no longer in use. Releasing the pointer may be done by either
        // freeing or unmapping it, and how to do this is determined by the
        // buffer's other flags.
        OWNS_DATA           = 0x8,
    };

  public:

    class BufferContents {
        uint8_t *data_;
        BufferKind kind_;

        friend class ArrayBufferObject;

        typedef void (BufferContents::* ConvertibleToBool)();
        void nonNull() {}

        BufferContents(uint8_t *data, BufferKind kind) : data_(data), kind_(kind) {
            MOZ_ASSERT((kind_ & ~KIND_MASK) == 0);
        }

      public:

        template<BufferKind Kind>
        static BufferContents create(void *data)
        {
            return BufferContents(static_cast<uint8_t*>(data), Kind);
        }

        static BufferContents createUnowned(void *data)
        {
            return BufferContents(static_cast<uint8_t*>(data), PLAIN_BUFFER);
        }

        uint8_t *data() const { return data_; }
        BufferKind kind() const { return kind_; }

        operator ConvertibleToBool() const { return data_ ? &BufferContents::nonNull : nullptr; }
    };

    static const Class class_;

    static const Class protoClass;
    static const JSFunctionSpec jsfuncs[];
    static const JSFunctionSpec jsstaticfuncs[];

    static bool byteLengthGetter(JSContext *cx, unsigned argc, Value *vp);

    static bool fun_slice(JSContext *cx, unsigned argc, Value *vp);

    static bool fun_isView(JSContext *cx, unsigned argc, Value *vp);

    static bool class_constructor(JSContext *cx, unsigned argc, Value *vp);

    static ArrayBufferObject *create(JSContext *cx, uint32_t nbytes,
                                     BufferContents contents,
                                     NewObjectKind newKind = GenericObject);
    static ArrayBufferObject *create(JSContext *cx, uint32_t nbytes,
                                     NewObjectKind newKind = GenericObject);

    static JSObject *createSlice(JSContext *cx, Handle<ArrayBufferObject*> arrayBuffer,
                                 uint32_t begin, uint32_t end);

    static bool createDataViewForThisImpl(JSContext *cx, CallArgs args);
    static bool createDataViewForThis(JSContext *cx, unsigned argc, Value *vp);

    template<typename T>
    static bool createTypedArrayFromBufferImpl(JSContext *cx, CallArgs args);

    template<typename T>
    static bool createTypedArrayFromBuffer(JSContext *cx, unsigned argc, Value *vp);

    static void objectMoved(JSObject *obj, const JSObject *old);

    static BufferContents stealContents(JSContext *cx,
                                        Handle<ArrayBufferObject*> buffer,
                                        bool hasStealableContents);

    bool hasStealableContents() const {
        // Inline elements strictly adhere to the corresponding buffer.
        if (!ownsData())
            return false;

        // asm.js buffer contents are transferred by copying, just like inline
        // elements.
        if (isAsmJSArrayBuffer())
            return false;

        // Neutered contents aren't transferrable because we want a neutered
        // array's contents to be backed by zeroed memory equal in length to
        // the original buffer contents.  Transferring these contents would
        // allocate new ones based on the current byteLength, which is 0 for a
        // neutered array -- not the original byteLength.
        return !isNeutered();
    }

    static void addSizeOfExcludingThis(JSObject *obj, mozilla::MallocSizeOf mallocSizeOf,
                                       JS::ClassInfo *info);

    // ArrayBufferObjects (strongly) store the first view added to them, while
    // later views are (weakly) stored in the compartment's InnerViewTable
    // below. Buffers typically have at least one view, so this slot optimizes
    // for the common case. Avoid entries in the InnerViewTable saves memory
    // and non-incrementalized sweep time.
    ArrayBufferViewObject *firstView();

    bool addView(JSContext *cx, JSObject *view);

    void setNewOwnedData(FreeOp* fop, BufferContents newContents);
    void changeContents(JSContext *cx, BufferContents newContents);

    /*
     * Ensure data is not stored inline in the object. Used when handing back a
     * GC-safe pointer.
     */
    static bool ensureNonInline(JSContext *cx, Handle<ArrayBufferObject*> buffer);

    /* Neuter this buffer and all its views. */
    static MOZ_WARN_UNUSED_RESULT bool
    neuter(JSContext *cx, Handle<ArrayBufferObject*> buffer, BufferContents newContents);

  private:
    void neuterView(JSContext *cx, ArrayBufferViewObject *view,
                    BufferContents newContents);
    void changeViewContents(JSContext *cx, ArrayBufferViewObject *view,
                            uint8_t *oldDataPointer, BufferContents newContents);
    void setFirstView(ArrayBufferViewObject *view);

    uint8_t *inlineDataPointer() const;

  public:
    uint8_t *dataPointer() const;
    size_t byteLength() const;
    BufferContents contents() const {
        return BufferContents(dataPointer(), bufferKind());
    }
    bool hasInlineData() const {
        return dataPointer() == inlineDataPointer();
    }

    void releaseData(FreeOp *fop);

    /*
     * Check if the arrayBuffer contains any data. This will return false for
     * ArrayBuffer.prototype and neutered ArrayBuffers.
     */
    bool hasData() const {
        return getClass() == &class_;
    }

    BufferKind bufferKind() const { return BufferKind(flags() & BUFFER_KIND_MASK); }
    bool isAsmJSArrayBuffer() const { return flags() & ASMJS_BUFFER; }
    bool isMappedArrayBuffer() const { return flags() & MAPPED_BUFFER; }
    bool isNeutered() const { return flags() & NEUTERED_BUFFER; }

    static bool prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer,
                                bool usesSignalHandlers);
    static bool prepareForAsmJSNoSignals(JSContext *cx, Handle<ArrayBufferObject*> buffer);

    static void finalize(FreeOp *fop, JSObject *obj);

    static BufferContents createMappedContents(int fd, size_t offset, size_t length);

    static size_t offsetOfFlagsSlot() {
        return getFixedSlotOffset(FLAGS_SLOT);
    }
    static size_t offsetOfDataSlot() {
        return getFixedSlotOffset(DATA_SLOT);
    }

    static uint32_t neuteredFlag() { return NEUTERED_BUFFER; }

  protected:
    enum OwnsState {
        DoesntOwnData = 0,
        OwnsData = 1,
    };

    void setDataPointer(BufferContents contents, OwnsState ownsState);
    void setByteLength(size_t length);

    uint32_t flags() const;
    void setFlags(uint32_t flags);

    bool ownsData() const { return flags() & OWNS_DATA; }
    void setOwnsData(OwnsState owns) {
        setFlags(owns ? (flags() | OWNS_DATA) : (flags() & ~OWNS_DATA));
    }

    void setIsAsmJSArrayBuffer() { setFlags(flags() | ASMJS_BUFFER); }
    void setIsMappedArrayBuffer() { setFlags(flags() | MAPPED_BUFFER); }
    void setIsNeutered() { setFlags(flags() | NEUTERED_BUFFER); }

    void initialize(size_t byteLength, BufferContents contents, OwnsState ownsState) {
        setByteLength(byteLength);
        setFlags(0);
        setFirstView(nullptr);
        setDataPointer(contents, ownsState);
    }

    void releaseAsmJSArray(FreeOp *fop);
    void releaseAsmJSArrayNoSignals(FreeOp *fop);
    void releaseMappedArray();
};

/*
 * ArrayBufferViewObject
 *
 * Common definitions shared by all array buffer views.
 */

class ArrayBufferViewObject : public JSObject
{
  public:
    static ArrayBufferObject *bufferObject(JSContext *cx, Handle<ArrayBufferViewObject *> obj);

    void neuter(void *newData);

    uint8_t *dataPointer();

    static void trace(JSTracer *trc, JSObject *obj);
};

bool
ToClampedIndex(JSContext *cx, HandleValue v, uint32_t length, uint32_t *out);

inline void
PostBarrierTypedArrayObject(JSObject *obj)
{
#ifdef JSGC_GENERATIONAL
    MOZ_ASSERT(obj);
    JSRuntime *rt = obj->runtimeFromMainThread();
    if (!rt->isHeapBusy() && !IsInsideNursery(JS::AsCell(obj)))
        rt->gc.storeBuffer.putWholeCellFromMainThread(obj);
#endif
}

inline void
InitArrayBufferViewDataPointer(JSObject *obj, ArrayBufferObject *buffer, size_t byteOffset)
{
    /*
     * N.B. The base of the array's data is stored in the object's
     * private data rather than a slot to avoid the restriction that
     * private Values that are pointers must have the low bits clear.
     */
    MOZ_ASSERT(buffer->dataPointer() != nullptr);
    obj->as<NativeObject>().initPrivate(buffer->dataPointer() + byteOffset);

    PostBarrierTypedArrayObject(obj);
}

/*
 * Tests for ArrayBufferObject, like obj->is<ArrayBufferObject>().
 */
bool IsArrayBuffer(HandleValue v);
bool IsArrayBuffer(HandleObject obj);
bool IsArrayBuffer(JSObject *obj);
ArrayBufferObject &AsArrayBuffer(HandleObject obj);
ArrayBufferObject &AsArrayBuffer(JSObject *obj);

extern uint32_t JS_FASTCALL
ClampDoubleToUint8(const double x);

struct uint8_clamped {
    uint8_t val;

    uint8_clamped() { }
    uint8_clamped(const uint8_clamped& other) : val(other.val) { }

    // invoke our assignment helpers for constructor conversion
    explicit uint8_clamped(uint8_t x)    { *this = x; }
    explicit uint8_clamped(uint16_t x)   { *this = x; }
    explicit uint8_clamped(uint32_t x)   { *this = x; }
    explicit uint8_clamped(int8_t x)     { *this = x; }
    explicit uint8_clamped(int16_t x)    { *this = x; }
    explicit uint8_clamped(int32_t x)    { *this = x; }
    explicit uint8_clamped(double x)     { *this = x; }

    uint8_clamped& operator=(const uint8_clamped& x) {
        val = x.val;
        return *this;
    }

    uint8_clamped& operator=(uint8_t x) {
        val = x;
        return *this;
    }

    uint8_clamped& operator=(uint16_t x) {
        val = (x > 255) ? 255 : uint8_t(x);
        return *this;
    }

    uint8_clamped& operator=(uint32_t x) {
        val = (x > 255) ? 255 : uint8_t(x);
        return *this;
    }

    uint8_clamped& operator=(int8_t x) {
        val = (x >= 0) ? uint8_t(x) : 0;
        return *this;
    }

    uint8_clamped& operator=(int16_t x) {
        val = (x >= 0)
              ? ((x < 255)
                 ? uint8_t(x)
                 : 255)
              : 0;
        return *this;
    }

    uint8_clamped& operator=(int32_t x) {
        val = (x >= 0)
              ? ((x < 255)
                 ? uint8_t(x)
                 : 255)
              : 0;
        return *this;
    }

    uint8_clamped& operator=(const double x) {
        val = uint8_t(ClampDoubleToUint8(x));
        return *this;
    }

    operator uint8_t() const {
        return val;
    }

    void staticAsserts() {
        static_assert(sizeof(uint8_clamped) == 1,
                      "uint8_clamped must be layout-compatible with uint8_t");
    }
};

/* Note that we can't use std::numeric_limits here due to uint8_clamped. */
template<typename T> inline bool TypeIsFloatingPoint() { return false; }
template<> inline bool TypeIsFloatingPoint<float>() { return true; }
template<> inline bool TypeIsFloatingPoint<double>() { return true; }

template<typename T> inline bool TypeIsUnsigned() { return false; }
template<> inline bool TypeIsUnsigned<uint8_t>() { return true; }
template<> inline bool TypeIsUnsigned<uint16_t>() { return true; }
template<> inline bool TypeIsUnsigned<uint32_t>() { return true; }

// Per-compartment table that manages the relationship between array buffers
// and the views that use their storage.
class InnerViewTable
{
  public:
    typedef Vector<ArrayBufferViewObject *, 1, SystemAllocPolicy> ViewVector;

    friend class ArrayBufferObject;

  private:
    typedef HashMap<JSObject *,
                    ViewVector,
                    DefaultHasher<JSObject *>,
                    SystemAllocPolicy> Map;

    // For all objects sharing their storage with some other view, this maps
    // the object to the list of such views. All entries in this map are weak.
    Map map;

    // List of keys from innerViews where either the source or at least one
    // target is in the nursery.
    Vector<JSObject *, 0, SystemAllocPolicy> nurseryKeys;

    // Whether nurseryKeys is a complete list.
    bool nurseryKeysValid;

    // Sweep an entry during GC, returning whether the entry should be removed.
    bool sweepEntry(JSObject **pkey, ViewVector &views);

    bool addView(JSContext *cx, ArrayBufferObject *obj, ArrayBufferViewObject *view);
    ViewVector *maybeViewsUnbarriered(ArrayBufferObject *obj);
    void removeViews(ArrayBufferObject *obj);

  public:
    InnerViewTable()
      : nurseryKeysValid(true)
    {}

    // Remove references to dead objects in the table and update table entries
    // to reflect moved objects.
    void sweep(JSRuntime *rt);
    void sweepAfterMinorGC(JSRuntime *rt);

    bool needsSweepAfterMinorGC() {
        return !nurseryKeys.empty() || !nurseryKeysValid;
    }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
};

} // namespace js

template <>
bool
JSObject::is<js::ArrayBufferViewObject>() const;

#endif // vm_ArrayBufferObject_h
