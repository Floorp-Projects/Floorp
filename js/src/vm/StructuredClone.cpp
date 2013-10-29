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

#include "mozilla/Endian.h"
#include "mozilla/FloatingPoint.h"

#include <algorithm>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsdate.h"
#include "jswrapper.h"

#include "vm/TypedArrayObject.h"
#include "vm/WrapperObject.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

using namespace js;

using mozilla::IsNaN;
using mozilla::LittleEndian;
using mozilla::NativeEndian;
using JS::CanonicalizeNaN;

enum StructuredDataType {
    /* Structured data types provided by the engine */
    SCTAG_FLOAT_MAX = 0xFFF00000,
    SCTAG_NULL = 0xFFFF0000,
    SCTAG_UNDEFINED,
    SCTAG_BOOLEAN,
    SCTAG_INDEX,
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
    SCTAG_DO_NOT_USE_1,
    SCTAG_DO_NOT_USE_2,
    SCTAG_TYPED_ARRAY_OBJECT,
    SCTAG_TYPED_ARRAY_V1_MIN = 0xFFFF0100,
    SCTAG_TYPED_ARRAY_V1_INT8 = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_INT8,
    SCTAG_TYPED_ARRAY_V1_UINT8 = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_UINT8,
    SCTAG_TYPED_ARRAY_V1_INT16 = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_INT16,
    SCTAG_TYPED_ARRAY_V1_UINT16 = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_UINT16,
    SCTAG_TYPED_ARRAY_V1_INT32 = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_INT32,
    SCTAG_TYPED_ARRAY_V1_UINT32 = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_UINT32,
    SCTAG_TYPED_ARRAY_V1_FLOAT32 = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_FLOAT32,
    SCTAG_TYPED_ARRAY_V1_FLOAT64 = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_FLOAT64,
    SCTAG_TYPED_ARRAY_V1_UINT8_CLAMPED = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_UINT8_CLAMPED,
    SCTAG_TYPED_ARRAY_V1_MAX = SCTAG_TYPED_ARRAY_V1_MIN + ScalarTypeRepresentation::TYPE_MAX - 1,

    /*
     * Define a separate range of numbers for Transferable-only tags, since
     * they are not used for persistent clone buffers and therefore do not
     * require bumping JS_STRUCTURED_CLONE_VERSION.
     */
    SCTAG_TRANSFER_MAP_HEADER = 0xFFFF0200,
    SCTAG_TRANSFER_MAP_ENTRY,

    SCTAG_END_OF_BUILTIN_TYPES
};

// Data associated with an SCTAG_TRANSFER_MAP_HEADER that tells whether the
// contents have been read out yet or not.
enum TransferableMapHeader {
    SCTAG_TM_UNREAD = 0,
    SCTAG_TM_TRANSFERRED
};

enum TransferableObjectType {
    // Transferable data has not been filled in yet
    SCTAG_TM_UNFILLED = 0,

    // Structured clone buffer does not yet own the data
    SCTAG_TM_UNOWNED = 1,

    // All values at least this large are owned by the clone buffer
    SCTAG_TM_FIRST_OWNED = 2,

    // Data is a pointer that can be freed
    SCTAG_TM_ALLOC_DATA = 2,
};

namespace js {

struct SCOutput {
  public:
    explicit SCOutput(JSContext *cx);
    ~SCOutput();

    JSContext *context() const { return cx; }

    bool write(uint64_t u);
    bool writePair(uint32_t tag, uint32_t data);
    bool writeDouble(double d);
    bool writeBytes(const void *p, size_t nbytes);
    bool writeChars(const jschar *p, size_t nchars);
    bool writePtr(const void *);

    template <class T>
    bool writeArray(const T *p, size_t nbytes);

    bool extractBuffer(uint64_t **datap, size_t *sizep);

    uint64_t count() const { return buf.length(); }
    uint64_t *rawBuffer() { return buf.begin(); }

  private:
    JSContext *cx;
    js::Vector<uint64_t> buf;
};

struct SCInput {
  public:
    SCInput(JSContext *cx, uint64_t *data, size_t nbytes);

    JSContext *context() const { return cx; }

    bool read(uint64_t *p);
    bool readPair(uint32_t *tagp, uint32_t *datap);
    bool readDouble(double *p);
    bool readBytes(void *p, size_t nbytes);
    bool readChars(jschar *p, size_t nchars);
    bool readPtr(void **);

    bool get(uint64_t *p);
    bool getPair(uint32_t *tagp, uint32_t *datap);

    bool replace(uint64_t u);
    bool replacePair(uint32_t tag, uint32_t data);

    uint64_t *tell() const { return point; }
    void seek(uint64_t *pos) {
        JS_ASSERT(pos <= end);
        point = pos;
    }

    template <class T>
    bool readArray(T *p, size_t nelems);

  private:
    bool eof();

    void staticAssertions() {
        JS_STATIC_ASSERT(sizeof(jschar) == 2);
        JS_STATIC_ASSERT(sizeof(uint32_t) == 4);
        JS_STATIC_ASSERT(sizeof(double) == 8);
    }

    JSContext *cx;
    uint64_t *point;
    uint64_t *end;
};

}

struct JSStructuredCloneReader {
  public:
    explicit JSStructuredCloneReader(js::SCInput &in, const JSStructuredCloneCallbacks *cb,
                                     void *cbClosure)
        : in(in), objs(in.context()), allObjs(in.context()),
          callbacks(cb), closure(cbClosure) { }

    js::SCInput &input() { return in; }
    bool read(js::Value *vp);

  private:
    JSContext *context() { return in.context(); }

    bool readTransferMap();

    bool checkDouble(double d);
    JSString *readString(uint32_t nchars);
    bool readTypedArray(uint32_t arrayType, uint32_t nelems, js::Value *vp, bool v1Read = false);
    bool readArrayBuffer(uint32_t nbytes, js::Value *vp);
    bool readV1ArrayBuffer(uint32_t arrayType, uint32_t nelems, js::Value *vp);
    bool readId(jsid *idp);
    bool startRead(js::Value *vp);

    js::SCInput &in;

    // Stack of objects with properties remaining to be read.
    js::AutoValueVector objs;

    // Stack of all objects read during this deserialization
    js::AutoValueVector allObjs;

    // The user defined callbacks that will be used for cloning.
    const JSStructuredCloneCallbacks *callbacks;

    // Any value passed to JS_ReadStructuredClone.
    void *closure;

    friend bool JS_ReadTypedArray(JSStructuredCloneReader *r, JS::Value *vp);
};

struct JSStructuredCloneWriter {
  public:
    explicit JSStructuredCloneWriter(js::SCOutput &out,
                                     const JSStructuredCloneCallbacks *cb,
                                     void *cbClosure,
                                     jsval tVal)
        : out(out), objs(out.context()),
          counts(out.context()), ids(out.context()),
          memory(out.context()), callbacks(cb), closure(cbClosure),
          transferable(out.context(), tVal), transferableObjects(out.context()) { }

    bool init() { return parseTransferable() && memory.init() && writeTransferMap(); }

    bool write(const js::Value &v);

    js::SCOutput &output() { return out; }

  private:
    JSContext *context() { return out.context(); }

    bool writeTransferMap();

    bool writeString(uint32_t tag, JSString *str);
    bool writeId(jsid id);
    bool writeArrayBuffer(JS::HandleObject obj);
    bool writeTypedArray(JS::HandleObject obj);
    bool startObject(JS::HandleObject obj, bool *backref);
    bool startWrite(const js::Value &v);
    bool traverseObject(JS::HandleObject obj);

    bool parseTransferable();
    void reportErrorTransferable();
    bool transferOwnership();

    inline void checkStack();

    js::SCOutput &out;

    // Vector of objects with properties remaining to be written.
    //
    // NB: These can span multiple compartments, so the compartment must be
    // entered before any manipulation is performed.
    js::AutoValueVector objs;

    // counts[i] is the number of properties of objs[i] remaining to be written.
    // counts.length() == objs.length() and sum(counts) == ids.length().
    js::Vector<size_t> counts;

    // Ids of properties remaining to be written.
    js::AutoIdVector ids;

    // The "memory" list described in the HTML5 internal structured cloning algorithm.
    // memory is a superset of objs; items are never removed from Memory
    // until a serialization operation is finished
    typedef js::AutoObjectUnsigned32HashMap CloneMemory;
    CloneMemory memory;

    // The user defined callbacks that will be used for cloning.
    const JSStructuredCloneCallbacks *callbacks;

    // Any value passed to JS_WriteStructuredClone.
    void *closure;

    // List of transferable objects
    JS::RootedValue transferable;
    JS::AutoObjectVector transferableObjects;

    friend bool JS_WriteTypedArray(JSStructuredCloneWriter *w, JS::Value v);
};

JS_FRIEND_API(uint64_t)
js_GetSCOffset(JSStructuredCloneWriter* writer)
{
    JS_ASSERT(writer);
    return writer->output().count() * sizeof(uint64_t);
}

JS_STATIC_ASSERT(SCTAG_END_OF_BUILTIN_TYPES <= JS_SCTAG_USER_MIN);
JS_STATIC_ASSERT(JS_SCTAG_USER_MIN <= JS_SCTAG_USER_MAX);
JS_STATIC_ASSERT(ScalarTypeRepresentation::TYPE_INT8 == 0);

namespace js {

bool
WriteStructuredClone(JSContext *cx, HandleValue v, uint64_t **bufp, size_t *nbytesp,
                     const JSStructuredCloneCallbacks *cb, void *cbClosure,
                     jsval transferable)
{
    SCOutput out(cx);
    JSStructuredCloneWriter w(out, cb, cbClosure, transferable);
    return w.init() && w.write(v) && out.extractBuffer(bufp, nbytesp);
}

bool
ReadStructuredClone(JSContext *cx, uint64_t *data, size_t nbytes, MutableHandleValue vp,
                    const JSStructuredCloneCallbacks *cb, void *cbClosure)
{
    SCInput in(cx, data, nbytes);
    JSStructuredCloneReader r(in, cb, cbClosure);
    return r.read(vp.address());
}

// This may acquire new ways of discarding transfer map entries as new
// Transferables are implemented.
static void
DiscardEntry(uint32_t mapEntryDescriptor, const uint64_t *ptr)
{
    JS_ASSERT(mapEntryDescriptor == SCTAG_TM_ALLOC_DATA);
    uint64_t u = LittleEndian::readUint64(ptr);
    js_free(reinterpret_cast<void*>(u));
}

static void
Discard(const uint64_t *begin, const uint64_t *end)
{
    const uint64_t *point = begin;

    uint64_t u = LittleEndian::readUint64(point++);
    uint32_t tag = uint32_t(u >> 32);
    if (tag != SCTAG_TRANSFER_MAP_HEADER)
        return;

    if (TransferableMapHeader(u) == SCTAG_TM_TRANSFERRED)
        return;

    uint64_t numTransferables = LittleEndian::readUint64(point++);
    while (numTransferables--) {
        uint64_t u = LittleEndian::readUint64(point++);
        JS_ASSERT(uint32_t(u >> 32) == SCTAG_TRANSFER_MAP_ENTRY);
        uint32_t mapEntryDescriptor = uint32_t(u);
        if (mapEntryDescriptor >= SCTAG_TM_FIRST_OWNED) {
            DiscardEntry(mapEntryDescriptor, point);
            point += 2; // Pointer and userdata
        }
    }
}

static void
ClearStructuredClone(const uint64_t *data, size_t nbytes)
{
    JS_ASSERT(nbytes % 8 == 0);
    Discard(data, data + nbytes / 8);
    js_free(const_cast<uint64_t*>(data));
}

bool
StructuredCloneHasTransferObjects(const uint64_t *data, size_t nbytes, bool *hasTransferable)
{
    *hasTransferable = false;

    if (data) {
        uint64_t u = LittleEndian::readUint64(data);
        uint32_t tag = uint32_t(u >> 32);
        if (tag == SCTAG_TRANSFER_MAP_HEADER)
            *hasTransferable = true;
    }

    return true;
}

} /* namespace js */

static inline uint64_t
PairToUInt64(uint32_t tag, uint32_t data)
{
    return uint64_t(data) | (uint64_t(tag) << 32);
}

bool
SCInput::eof()
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         JSMSG_SC_BAD_SERIALIZED_DATA, "truncated");
    return false;
}

SCInput::SCInput(JSContext *cx, uint64_t *data, size_t nbytes)
    : cx(cx), point(data), end(data + nbytes / 8)
{
    JS_ASSERT((uintptr_t(data) & 7) == 0);
    JS_ASSERT((nbytes & 7) == 0);
}

bool
SCInput::read(uint64_t *p)
{
    if (point == end) {
        *p = 0;  /* initialize to shut GCC up */
        return eof();
    }
    *p = LittleEndian::readUint64(point++);
    return true;
}

bool
SCInput::readPair(uint32_t *tagp, uint32_t *datap)
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
SCInput::get(uint64_t *p)
{
    if (point == end)
        return eof();
    *p = LittleEndian::readUint64(point);
    return true;
}

bool
SCInput::getPair(uint32_t *tagp, uint32_t *datap)
{
    uint64_t u;
    if (!get(&u))
        return false;

    *tagp = uint32_t(u >> 32);
    *datap = uint32_t(u);
    return true;
}

bool
SCInput::replace(uint64_t u)
{
    if (point == end)
       return eof();
    LittleEndian::writeUint64(point, u);
    return true;
}

bool
SCInput::replacePair(uint32_t tag, uint32_t data)
{
    return replace(PairToUInt64(tag, data));
}

bool
SCInput::readDouble(double *p)
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
copyAndSwapFromLittleEndian(T *dest, const void *src, size_t nelems)
{
    if (nelems > 0)
        NativeEndian::copyAndSwapFromLittleEndian(dest, src, nelems);
}

template <>
void
copyAndSwapFromLittleEndian(uint8_t *dest, const void *src, size_t nelems)
{
    memcpy(dest, src, nelems);
}

template <class T>
bool
SCInput::readArray(T *p, size_t nelems)
{
    JS_STATIC_ASSERT(sizeof(uint64_t) % sizeof(T) == 0);

    /*
     * Fail if nelems is so huge as to make JS_HOWMANY overflow or if nwords is
     * larger than the remaining data.
     */
    size_t nwords = JS_HOWMANY(nelems, sizeof(uint64_t) / sizeof(T));
    if (nelems + sizeof(uint64_t) / sizeof(T) - 1 < nelems || nwords > size_t(end - point))
        return eof();

    copyAndSwapFromLittleEndian(p, point, nelems);
    point += nwords;
    return true;
}

bool
SCInput::readBytes(void *p, size_t nbytes)
{
    return readArray((uint8_t *) p, nbytes);
}

bool
SCInput::readChars(jschar *p, size_t nchars)
{
    JS_ASSERT(sizeof(jschar) == sizeof(uint16_t));
    return readArray((uint16_t *) p, nchars);
}

bool
SCInput::readPtr(void **p)
{
    // On a 32 bit system the void* variable we have to write to is only
    // 32 bits, so we create a 64 temporary and discard the unused bits.
    uint64_t tmp;
    bool ret = read(&tmp);
    *p = reinterpret_cast<void*>(tmp);
    return ret;
}

SCOutput::SCOutput(JSContext *cx) : cx(cx), buf(cx) {}

SCOutput::~SCOutput()
{
    // Free any transferable data left lying around in the buffer
    Discard(rawBuffer(), rawBuffer() + count());
}

bool
SCOutput::write(uint64_t u)
{
    return buf.append(NativeEndian::swapToLittleEndian(u));
}

bool
SCOutput::writePair(uint32_t tag, uint32_t data)
{
    /*
     * As it happens, the tag word appears after the data word in the output.
     * This is because exponents occupy the last 2 bytes of doubles on the
     * little-endian platforms we care most about.
     *
     * For example, JSVAL_TRUE is written using writePair(SCTAG_BOOLEAN, 1).
     * PairToUInt64 produces the number 0xFFFF000200000001.
     * That is written out as the bytes 01 00 00 00 02 00 FF FF.
     */
    return write(PairToUInt64(tag, data));
}

static inline uint64_t
ReinterpretDoubleAsUInt64(double d)
{
    union {
        double d;
        uint64_t u;
    } pun;
    pun.d = d;
    return pun.u;
}

static inline double
ReinterpretUInt64AsDouble(uint64_t u)
{
    union {
        uint64_t u;
        double d;
    } pun;
    pun.u = u;
    return pun.d;
}

static inline double
ReinterpretPairAsDouble(uint32_t tag, uint32_t data)
{
    return ReinterpretUInt64AsDouble(PairToUInt64(tag, data));
}

bool
SCOutput::writeDouble(double d)
{
    return write(ReinterpretDoubleAsUInt64(CanonicalizeNaN(d)));
}

template <typename T>
static void
copyAndSwapToLittleEndian(void *dest, const T *src, size_t nelems)
{
    if (nelems > 0)
        NativeEndian::copyAndSwapToLittleEndian(dest, src, nelems);
}

template <>
void
copyAndSwapToLittleEndian(void *dest, const uint8_t *src, size_t nelems)
{
    memcpy(dest, src, nelems);
}

template <class T>
bool
SCOutput::writeArray(const T *p, size_t nelems)
{
    JS_ASSERT(8 % sizeof(T) == 0);
    JS_ASSERT(sizeof(uint64_t) % sizeof(T) == 0);

    if (nelems == 0)
        return true;

    if (nelems + sizeof(uint64_t) / sizeof(T) - 1 < nelems) {
        js_ReportAllocationOverflow(context());
        return false;
    }
    size_t nwords = JS_HOWMANY(nelems, sizeof(uint64_t) / sizeof(T));
    size_t start = buf.length();
    if (!buf.growByUninitialized(nwords))
        return false;

    buf.back() = 0;  /* zero-pad to an 8-byte boundary */

    T *q = (T *) &buf[start];
    copyAndSwapToLittleEndian(q, p, nelems);
    return true;
}

bool
SCOutput::writeBytes(const void *p, size_t nbytes)
{
    return writeArray((const uint8_t *) p, nbytes);
}

bool
SCOutput::writeChars(const jschar *p, size_t nchars)
{
    JS_ASSERT(sizeof(jschar) == sizeof(uint16_t));
    return writeArray((const uint16_t *) p, nchars);
}

bool
SCOutput::writePtr(const void *p)
{
    return write(reinterpret_cast<uint64_t>(p));
}

bool
SCOutput::extractBuffer(uint64_t **datap, size_t *sizep)
{
    *sizep = buf.length() * sizeof(uint64_t);
    return (*datap = buf.extractRawBuffer()) != nullptr;
}

JS_STATIC_ASSERT(JSString::MAX_LENGTH < UINT32_MAX);

bool
JSStructuredCloneWriter::parseTransferable()
{
    MOZ_ASSERT(transferableObjects.empty(), "parseTransferable called with stale data");

    if (JSVAL_IS_NULL(transferable) || JSVAL_IS_VOID(transferable))
        return true;

    if (!transferable.isObject()) {
        reportErrorTransferable();
        return false;
    }

    JSContext *cx = context();
    RootedObject array(cx, &transferable.toObject());
    if (!JS_IsArrayObject(cx, array)) {
        reportErrorTransferable();
        return false;
    }

    uint32_t length;
    if (!JS_GetArrayLength(cx, array, &length)) {
        return false;
    }

    RootedValue v(context());

    for (uint32_t i = 0; i < length; ++i) {
        if (!JS_GetElement(cx, array, i, &v)) {
            return false;
        }

        if (!v.isObject()) {
            reportErrorTransferable();
            return false;
        }

        JSObject* tObj = CheckedUnwrap(&v.toObject());
        if (!tObj) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr, JSMSG_UNWRAP_DENIED);
            return false;
        }
        if (!tObj->is<ArrayBufferObject>()) {
            reportErrorTransferable();
            return false;
        }

        // No duplicates allowed
        if (std::find(transferableObjects.begin(), transferableObjects.end(), tObj) != transferableObjects.end()) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr, JSMSG_SC_DUP_TRANSFERABLE);
            return false;
        }

        if (!transferableObjects.append(tObj))
            return false;
    }

    return true;
}

void
JSStructuredCloneWriter::reportErrorTransferable()
{
    if (callbacks && callbacks->reportError)
        return callbacks->reportError(context(), JS_SCERR_TRANSFERABLE);
    else
        JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr, JSMSG_SC_NOT_TRANSFERABLE);
}

bool
JSStructuredCloneWriter::writeString(uint32_t tag, JSString *str)
{
    size_t length = str->length();
    const jschar *chars = str->getChars(context());
    if (!chars)
        return false;
    return out.writePair(tag, uint32_t(length)) && out.writeChars(chars, length);
}

bool
JSStructuredCloneWriter::writeId(jsid id)
{
    if (JSID_IS_INT(id))
        return out.writePair(SCTAG_INDEX, uint32_t(JSID_TO_INT(id)));
    JS_ASSERT(JSID_IS_STRING(id));
    return writeString(SCTAG_STRING, JSID_TO_STRING(id));
}

inline void
JSStructuredCloneWriter::checkStack()
{
#ifdef DEBUG
    /* To avoid making serialization O(n^2), limit stack-checking at 10. */
    const size_t MAX = 10;

    size_t limit = Min(counts.length(), MAX);
    JS_ASSERT(objs.length() == counts.length());
    size_t total = 0;
    for (size_t i = 0; i < limit; i++) {
        JS_ASSERT(total + counts[i] >= total);
        total += counts[i];
    }
    if (counts.length() <= MAX)
        JS_ASSERT(total == ids.length());
    else
        JS_ASSERT(total <= ids.length());

    size_t j = objs.length();
    for (size_t i = 0; i < limit; i++)
        JS_ASSERT(memory.has(&objs[--j].toObject()));
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
    Rooted<TypedArrayObject*> tarr(context(), &obj->as<TypedArrayObject>());
    if (!out.writePair(SCTAG_TYPED_ARRAY_OBJECT, tarr->length()))
        return false;
    uint64_t type = tarr->type();
    if (!out.write(type))
        return false;

    // Write out the ArrayBuffer tag and contents
    if (!startWrite(TypedArrayObject::bufferValue(tarr)))
        return false;

    return out.write(tarr->byteOffset());
}

bool
JSStructuredCloneWriter::writeArrayBuffer(HandleObject obj)
{
    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();
    return out.writePair(SCTAG_ARRAY_BUFFER_OBJECT, buffer.byteLength()) &&
           out.writeBytes(buffer.dataPointer(), buffer.byteLength());
}

bool
JSStructuredCloneWriter::startObject(HandleObject obj, bool *backref)
{
    /* Handle cycles in the object graph. */
    CloneMemory::AddPtr p = memory.lookupForAdd(obj);
    if ((*backref = p))
        return out.writePair(SCTAG_BACK_REFERENCE_OBJECT, p->value);
    if (!memory.add(p, obj, memory.count()))
        return false;

    if (memory.count() == UINT32_MAX) {
        JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                             JSMSG_NEED_DIET, "object graph to serialize");
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
    size_t initialLength = ids.length();
    if (!GetPropertyNames(context(), obj, JSITER_OWNONLY, &ids))
        return false;
    jsid *begin = ids.begin() + initialLength, *end = ids.end();
    size_t count = size_t(end - begin);
    Reverse(begin, end);

    /* Push obj and count to the stack. */
    if (!objs.append(ObjectValue(*obj)) || !counts.append(count))
        return false;
    checkStack();

    /* Write the header for obj. */
    return out.writePair(obj->is<ArrayObject>() ? SCTAG_ARRAY_OBJECT : SCTAG_OBJECT_OBJECT, 0);
}

static bool
PrimitiveToObject(JSContext *cx, Value *vp)
{
    JSObject *obj = PrimitiveToObject(cx, *vp);
    if (!obj)
        return false;

    vp->setObject(*obj);
    return true;
}

bool
JSStructuredCloneWriter::startWrite(const Value &v)
{
    assertSameCompartment(context(), v);

    if (v.isString()) {
        return writeString(SCTAG_STRING, v.toString());
    } else if (v.isNumber()) {
        return out.writeDouble(v.toNumber());
    } else if (v.isBoolean()) {
        return out.writePair(SCTAG_BOOLEAN, v.toBoolean());
    } else if (v.isNull()) {
        return out.writePair(SCTAG_NULL, 0);
    } else if (v.isUndefined()) {
        return out.writePair(SCTAG_UNDEFINED, 0);
    } else if (v.isObject()) {
        RootedObject obj(context(), &v.toObject());

        // The object might be a security wrapper. See if we can clone what's
        // behind it. If we can, unwrap the object.
        obj = CheckedUnwrap(obj);
        if (!obj) {
            JS_ReportError(context(), "Permission denied to access object");
            return false;
        }

        AutoCompartment ac(context(), obj);

        bool backref;
        if (!startObject(obj, &backref))
            return false;
        if (backref)
            return true;

        if (obj->is<RegExpObject>()) {
            RegExpObject &reobj = obj->as<RegExpObject>();
            return out.writePair(SCTAG_REGEXP_OBJECT, reobj.getFlags()) &&
                   writeString(SCTAG_STRING, reobj.getSource());
        } else if (obj->is<DateObject>()) {
            double d = js_DateGetMsecSinceEpoch(obj);
            return out.writePair(SCTAG_DATE_OBJECT, 0) && out.writeDouble(d);
        } else if (obj->is<TypedArrayObject>()) {
            return writeTypedArray(obj);
        } else if (obj->is<ArrayBufferObject>() && obj->as<ArrayBufferObject>().hasData()) {
            return writeArrayBuffer(obj);
        } else if (obj->is<JSObject>() || obj->is<ArrayObject>()) {
            return traverseObject(obj);
        } else if (obj->is<BooleanObject>()) {
            return out.writePair(SCTAG_BOOLEAN_OBJECT, obj->as<BooleanObject>().unbox());
        } else if (obj->is<NumberObject>()) {
            return out.writePair(SCTAG_NUMBER_OBJECT, 0) &&
                   out.writeDouble(obj->as<NumberObject>().unbox());
        } else if (obj->is<StringObject>()) {
            return writeString(SCTAG_STRING_OBJECT, obj->as<StringObject>().unbox());
        }

        if (callbacks && callbacks->write)
            return callbacks->write(context(), this, obj, closure);
        /* else fall through */
    }

    JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr, JSMSG_SC_UNSUPPORTED_TYPE);
    return false;
}

bool
JSStructuredCloneWriter::writeTransferMap()
{
    if (transferableObjects.empty())
        return true;

    if (!out.writePair(SCTAG_TRANSFER_MAP_HEADER, (uint32_t)SCTAG_TM_UNREAD))
        return false;

    if (!out.write(transferableObjects.length()))
        return false;

    for (JS::AutoObjectVector::Range tr = transferableObjects.all();
         !tr.empty(); tr.popFront())
    {
        JSObject *obj = tr.front();

        if (!memory.put(obj, memory.count()))
            return false;

        // Emit a placeholder pointer. We will steal the data and neuter the
        // transferable later.
        if (!out.writePair(SCTAG_TRANSFER_MAP_ENTRY, SCTAG_TM_UNFILLED) ||
            !out.writePtr(nullptr) ||
            !out.write(0))
        {
            return false;
        }
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
    uint64_t *point = out.rawBuffer();
    JS_ASSERT(uint32_t(LittleEndian::readUint64(point) >> 32) == SCTAG_TRANSFER_MAP_HEADER);
    point++;
    JS_ASSERT(LittleEndian::readUint64(point) == transferableObjects.length());
    point++;

    for (JS::AutoObjectVector::Range tr = transferableObjects.all();
         !tr.empty();
         tr.popFront())
    {
        RootedObject obj(context(), tr.front());
        void *content;
        uint8_t *data;
        if (!JS_StealArrayBufferContents(context(), obj, &content, &data))
            return false; // Destructor will clean up the already-transferred data

        MOZ_ASSERT(uint32_t(LittleEndian::readUint64(point) >> 32) == SCTAG_TRANSFER_MAP_ENTRY);
        LittleEndian::writeUint64(point++, PairToUInt64(SCTAG_TRANSFER_MAP_ENTRY, SCTAG_TM_ALLOC_DATA));
        LittleEndian::writeUint64(point++, reinterpret_cast<uint64_t>(content));
        LittleEndian::writeUint64(point++, 0);
    }

    JS_ASSERT(point <= out.rawBuffer() + out.count());
    JS_ASSERT_IF(point < out.rawBuffer() + out.count(),
                 uint32_t(LittleEndian::readUint64(point) >> 32) != SCTAG_TRANSFER_MAP_ENTRY);

    return true;
}

bool
JSStructuredCloneWriter::write(const Value &v)
{
    if (!startWrite(v))
        return false;

    while (!counts.empty()) {
        RootedObject obj(context(), &objs.back().toObject());
        AutoCompartment ac(context(), obj);
        if (counts.back()) {
            counts.back()--;
            RootedId id(context(), ids.back());
            ids.popBack();
            checkStack();
            if (JSID_IS_STRING(id) || JSID_IS_INT(id)) {
                /*
                 * If obj still has an own property named id, write it out.
                 * The cost of re-checking could be avoided by using
                 * NativeIterators.
                 */
                RootedObject obj2(context());
                RootedShape prop(context());
                if (!HasOwnProperty<CanGC>(context(), obj->getOps()->lookupGeneric, obj, id,
                                           &obj2, &prop)) {
                    return false;
                }

                if (prop) {
                    RootedValue val(context());
                    if (!writeId(id) ||
                        !JSObject::getGeneric(context(), obj, obj, id, &val) ||
                        !startWrite(val))
                        return false;
                }
            }
        } else {
            out.writePair(SCTAG_NULL, 0);
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
    jsval_layout l;
    l.asDouble = d;
    if (!JSVAL_IS_DOUBLE_IMPL(l)) {
        JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                             JSMSG_SC_BAD_SERIALIZED_DATA, "unrecognized NaN");
        return false;
    }
    return true;
}

namespace {

class Chars {
    JSContext *cx;
    jschar *p;
  public:
    Chars(JSContext *cx) : cx(cx), p(nullptr) {}
    ~Chars() { js_free(p); }

    bool allocate(size_t len) {
        JS_ASSERT(!p);
        // We're going to null-terminate!
        p = cx->pod_malloc<jschar>(len + 1);
        if (p) {
            p[len] = jschar(0);
            return true;
        }
        return false;
    }
    jschar *get() { return p; }
    void forget() { p = nullptr; }
};

} /* anonymous namespace */

JSString *
JSStructuredCloneReader::readString(uint32_t nchars)
{
    if (nchars > JSString::MAX_LENGTH) {
        JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                             JSMSG_SC_BAD_SERIALIZED_DATA, "string length");
        return nullptr;
    }
    Chars chars(context());
    if (!chars.allocate(nchars) || !in.readChars(chars.get(), nchars))
        return nullptr;
    JSString *str = js_NewString<CanGC>(context(), chars.get(), nchars);
    if (str)
        chars.forget();
    return str;
}

static uint32_t
TagToV1ArrayType(uint32_t tag)
{
    JS_ASSERT(tag >= SCTAG_TYPED_ARRAY_V1_MIN && tag <= SCTAG_TYPED_ARRAY_V1_MAX);
    return tag - SCTAG_TYPED_ARRAY_V1_MIN;
}

bool
JSStructuredCloneReader::readTypedArray(uint32_t arrayType, uint32_t nelems, Value *vp,
                                        bool v1Read)
{
    if (arrayType > ScalarTypeRepresentation::TYPE_UINT8_CLAMPED) {
        JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                             JSMSG_SC_BAD_SERIALIZED_DATA, "unhandled typed array element type");
        return false;
    }

    // Push a placeholder onto the allObjs list to stand in for the typed array
    uint32_t placeholderIndex = allObjs.length();
    Value dummy = JSVAL_NULL;
    if (!allObjs.append(dummy))
        return false;

    // Read the ArrayBuffer object and its contents (but no properties)
    RootedValue v(context());
    uint32_t byteOffset;
    if (v1Read) {
        if (!readV1ArrayBuffer(arrayType, nelems, v.address()))
            return false;
        byteOffset = 0;
    } else {
        if (!startRead(v.address()))
            return false;
        uint64_t n;
        if (!in.read(&n))
            return false;
        byteOffset = n;
    }
    RootedObject buffer(context(), &v.toObject());
    RootedObject obj(context(), nullptr);

    switch (arrayType) {
      case ScalarTypeRepresentation::TYPE_INT8:
        obj = JS_NewInt8ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case ScalarTypeRepresentation::TYPE_UINT8:
        obj = JS_NewUint8ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case ScalarTypeRepresentation::TYPE_INT16:
        obj = JS_NewInt16ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case ScalarTypeRepresentation::TYPE_UINT16:
        obj = JS_NewUint16ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case ScalarTypeRepresentation::TYPE_INT32:
        obj = JS_NewInt32ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case ScalarTypeRepresentation::TYPE_UINT32:
        obj = JS_NewUint32ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case ScalarTypeRepresentation::TYPE_FLOAT32:
        obj = JS_NewFloat32ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case ScalarTypeRepresentation::TYPE_FLOAT64:
        obj = JS_NewFloat64ArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
        obj = JS_NewUint8ClampedArrayWithBuffer(context(), buffer, byteOffset, nelems);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unknown TypedArrayObject type");
    }

    if (!obj)
        return false;
    vp->setObject(*obj);

    allObjs[placeholderIndex] = *vp;

    return true;
}

bool
JSStructuredCloneReader::readArrayBuffer(uint32_t nbytes, Value *vp)
{
    JSObject *obj = ArrayBufferObject::create(context(), nbytes);
    if (!obj)
        return false;
    vp->setObject(*obj);
    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();
    JS_ASSERT(buffer.byteLength() == nbytes);
    return in.readArray(buffer.dataPointer(), nbytes);
}

static size_t
bytesPerTypedArrayElement(uint32_t arrayType)
{
    switch (arrayType) {
      case ScalarTypeRepresentation::TYPE_INT8:
      case ScalarTypeRepresentation::TYPE_UINT8:
      case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
        return sizeof(uint8_t);
      case ScalarTypeRepresentation::TYPE_INT16:
      case ScalarTypeRepresentation::TYPE_UINT16:
        return sizeof(uint16_t);
      case ScalarTypeRepresentation::TYPE_INT32:
      case ScalarTypeRepresentation::TYPE_UINT32:
      case ScalarTypeRepresentation::TYPE_FLOAT32:
        return sizeof(uint32_t);
      case ScalarTypeRepresentation::TYPE_FLOAT64:
        return sizeof(uint64_t);
      default:
        MOZ_ASSUME_UNREACHABLE("unknown TypedArrayObject type");
    }
}

/*
 * Read in the data for a structured clone version 1 ArrayBuffer, performing
 * endianness-conversion while reading.
 */
bool
JSStructuredCloneReader::readV1ArrayBuffer(uint32_t arrayType, uint32_t nelems, Value *vp)
{
    JS_ASSERT(arrayType <= ScalarTypeRepresentation::TYPE_UINT8_CLAMPED);

    uint32_t nbytes = nelems * bytesPerTypedArrayElement(arrayType);
    JSObject *obj = ArrayBufferObject::create(context(), nbytes);
    if (!obj)
        return false;
    vp->setObject(*obj);
    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();
    JS_ASSERT(buffer.byteLength() == nbytes);

    switch (arrayType) {
      case ScalarTypeRepresentation::TYPE_INT8:
      case ScalarTypeRepresentation::TYPE_UINT8:
      case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
        return in.readArray((uint8_t*) buffer.dataPointer(), nelems);
      case ScalarTypeRepresentation::TYPE_INT16:
      case ScalarTypeRepresentation::TYPE_UINT16:
        return in.readArray((uint16_t*) buffer.dataPointer(), nelems);
      case ScalarTypeRepresentation::TYPE_INT32:
      case ScalarTypeRepresentation::TYPE_UINT32:
      case ScalarTypeRepresentation::TYPE_FLOAT32:
        return in.readArray((uint32_t*) buffer.dataPointer(), nelems);
      case ScalarTypeRepresentation::TYPE_FLOAT64:
        return in.readArray((uint64_t*) buffer.dataPointer(), nelems);
      default:
        MOZ_ASSUME_UNREACHABLE("unknown TypedArrayObject type");
    }
}

bool
JSStructuredCloneReader::startRead(Value *vp)
{
    uint32_t tag, data;

    if (!in.readPair(&tag, &data))
        return false;
    switch (tag) {
      case SCTAG_NULL:
        vp->setNull();
        break;

      case SCTAG_UNDEFINED:
        vp->setUndefined();
        break;

      case SCTAG_BOOLEAN:
      case SCTAG_BOOLEAN_OBJECT:
        vp->setBoolean(!!data);
        if (tag == SCTAG_BOOLEAN_OBJECT && !PrimitiveToObject(context(), vp))
            return false;
        break;

      case SCTAG_STRING:
      case SCTAG_STRING_OBJECT: {
        JSString *str = readString(data);
        if (!str)
            return false;
        vp->setString(str);
        if (tag == SCTAG_STRING_OBJECT && !PrimitiveToObject(context(), vp))
            return false;
        break;
      }

      case SCTAG_NUMBER_OBJECT: {
        double d;
        if (!in.readDouble(&d) || !checkDouble(d))
            return false;
        vp->setDouble(d);
        if (!PrimitiveToObject(context(), vp))
            return false;
        break;
      }

      case SCTAG_DATE_OBJECT: {
        double d;
        if (!in.readDouble(&d) || !checkDouble(d))
            return false;
        if (!IsNaN(d) && d != TimeClip(d)) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                                 JSMSG_SC_BAD_SERIALIZED_DATA, "date");
            return false;
        }
        JSObject *obj = js_NewDateObjectMsec(context(), d);
        if (!obj)
            return false;
        vp->setObject(*obj);
        break;
      }

      case SCTAG_REGEXP_OBJECT: {
        RegExpFlag flags = RegExpFlag(data);
        uint32_t tag2, nchars;
        if (!in.readPair(&tag2, &nchars))
            return false;
        if (tag2 != SCTAG_STRING) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                                 JSMSG_SC_BAD_SERIALIZED_DATA, "regexp");
            return false;
        }
        JSString *str = readString(nchars);
        if (!str)
            return false;
        JSStableString *stable = str->ensureStable(context());
        if (!stable)
            return false;

        size_t length = stable->length();
        const StableCharPtr chars = stable->chars();
        RegExpObject *reobj = RegExpObject::createNoStatics(context(), chars.get(), length,
                                                            flags, nullptr);
        if (!reobj)
            return false;
        vp->setObject(*reobj);
        break;
      }

      case SCTAG_ARRAY_OBJECT:
      case SCTAG_OBJECT_OBJECT: {
        JSObject *obj = (tag == SCTAG_ARRAY_OBJECT)
                        ? NewDenseEmptyArray(context())
                        : NewBuiltinClassInstance(context(), &JSObject::class_);
        if (!obj || !objs.append(ObjectValue(*obj)))
            return false;
        vp->setObject(*obj);
        break;
      }

      case SCTAG_BACK_REFERENCE_OBJECT: {
        if (data >= allObjs.length()) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                                 JSMSG_SC_BAD_SERIALIZED_DATA,
                                 "invalid back reference in input");
            return false;
        }
        *vp = allObjs[data];
        return true;
      }

      case SCTAG_TRANSFER_MAP_HEADER:
        // A map header cannot be here but just at the beginning of the buffer.
        JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                             JSMSG_SC_BAD_SERIALIZED_DATA,
                             "invalid input");
        return false;

      case SCTAG_TRANSFER_MAP_ENTRY:
        // A map cannot be here but just at the beginning of the buffer.
        JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                             JSMSG_SC_BAD_SERIALIZED_DATA,
                             "invalid input");
        return false;

      case SCTAG_ARRAY_BUFFER_OBJECT:
        if (!readArrayBuffer(data, vp))
            return false;
        break;

      case SCTAG_TYPED_ARRAY_OBJECT:
        // readTypedArray adds the array to allObjs
        uint64_t arrayType;
        if (!in.read(&arrayType))
            return false;
        return readTypedArray(arrayType, data, vp);
        break;

      default: {
        if (tag <= SCTAG_FLOAT_MAX) {
            double d = ReinterpretPairAsDouble(tag, data);
            if (!checkDouble(d))
                return false;
            vp->setNumber(d);
            break;
        }

        if (SCTAG_TYPED_ARRAY_V1_MIN <= tag && tag <= SCTAG_TYPED_ARRAY_V1_MAX) {
            // A v1-format typed array
            // readTypedArray adds the array to allObjs
            return readTypedArray(TagToV1ArrayType(tag), data, vp, true);
        }

        if (!callbacks || !callbacks->read) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                                 JSMSG_SC_BAD_SERIALIZED_DATA, "unsupported type");
            return false;
        }
        JSObject *obj = callbacks->read(context(), this, tag, data, closure);
        if (!obj)
            return false;
        vp->setObject(*obj);
      }
    }

    if (vp->isObject() && !allObjs.append(*vp))
        return false;

    return true;
}

bool
JSStructuredCloneReader::readId(jsid *idp)
{
    uint32_t tag, data;
    if (!in.readPair(&tag, &data))
        return false;

    if (tag == SCTAG_INDEX) {
        *idp = INT_TO_JSID(int32_t(data));
        return true;
    }
    if (tag == SCTAG_STRING) {
        JSString *str = readString(data);
        if (!str)
            return false;
        JSAtom *atom = AtomizeString<CanGC>(context(), str);
        if (!atom)
            return false;
        *idp = NON_INTEGER_ATOM_TO_JSID(atom);
        return true;
    }
    if (tag == SCTAG_NULL) {
        *idp = JSID_VOID;
        return true;
    }
    JS_ReportErrorNumber(context(), js_GetErrorMessage, nullptr,
                         JSMSG_SC_BAD_SERIALIZED_DATA, "id");
    return false;
}

bool
JSStructuredCloneReader::readTransferMap()
{
    uint64_t *headerPos = in.tell();

    uint32_t tag, data;
    if (!in.getPair(&tag, &data))
        return false;

    if (tag != SCTAG_TRANSFER_MAP_HEADER || TransferableMapHeader(data) == SCTAG_TM_TRANSFERRED)
        return true;

    uint64_t numTransferables;
    MOZ_ALWAYS_TRUE(in.readPair(&tag, &data));
    if (!in.read(&numTransferables))
        return false;

    for (uint64_t i = 0; i < numTransferables; i++) {
        uint64_t *pos = in.tell();

        if (!in.readPair(&tag, &data))
            return false;
        JS_ASSERT(tag == SCTAG_TRANSFER_MAP_ENTRY);
        JS_ASSERT(data == SCTAG_TM_ALLOC_DATA);

        void *content;
        if (!in.readPtr(&content))
            return false;

        uint64_t userdata;
        if (!in.read(&userdata))
            return false;

        RootedObject obj(context(), JS_NewArrayBufferWithContents(context(), content));
        if (!obj)
            return false;

        // Rewind to the SCTAG_TRANSFER_MAP_ENTRY and mark this entry as unowned by
        // the input buffer.
        uint64_t *next = in.tell();
        in.seek(pos);
        MOZ_ALWAYS_TRUE(in.replacePair(SCTAG_TRANSFER_MAP_ENTRY, SCTAG_TM_UNOWNED));
        in.seek(next);

        if (!allObjs.append(ObjectValue(*obj)))
            return false;
    }

    // Mark the whole transfer map as consumed
    uint64_t *endPos = in.tell();
    in.seek(headerPos);
    MOZ_ALWAYS_TRUE(in.replacePair(SCTAG_TRANSFER_MAP_HEADER, SCTAG_TM_TRANSFERRED));
    in.seek(endPos);

    return true;
}

bool
JSStructuredCloneReader::read(Value *vp)
{
    if (!readTransferMap())
        return false;

    if (!startRead(vp))
        return false;

    while (objs.length() != 0) {
        RootedObject obj(context(), &objs.back().toObject());

        RootedId id(context());
        if (!readId(id.address()))
            return false;

        if (JSID_IS_VOID(id)) {
            objs.popBack();
        } else {
            RootedValue v(context());
            if (!startRead(v.address()) || !JSObject::defineGeneric(context(), obj, id, v))
                return false;
        }
    }

    allObjs.clear();

    return true;
}
using namespace js;

JS_PUBLIC_API(bool)
JS_ReadStructuredClone(JSContext *cx, uint64_t *buf, size_t nbytes,
                       uint32_t version, JS::MutableHandleValue vp,
                       const JSStructuredCloneCallbacks *optionalCallbacks,
                       void *closure)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    if (version > JS_STRUCTURED_CLONE_VERSION) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_CLONE_VERSION);
        return false;
    }
    const JSStructuredCloneCallbacks *callbacks =
        optionalCallbacks ?
        optionalCallbacks :
        cx->runtime()->structuredCloneCallbacks;
    return ReadStructuredClone(cx, buf, nbytes, vp, callbacks, closure);
}

JS_PUBLIC_API(bool)
JS_WriteStructuredClone(JSContext *cx, JS::HandleValue value, uint64_t **bufp, size_t *nbytesp,
                        const JSStructuredCloneCallbacks *optionalCallbacks,
                        void *closure, JS::HandleValue transferable)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, value);

    const JSStructuredCloneCallbacks *callbacks =
        optionalCallbacks ?
        optionalCallbacks :
        cx->runtime()->structuredCloneCallbacks;
    return WriteStructuredClone(cx, value, bufp, nbytesp, callbacks, closure, transferable);
}

JS_PUBLIC_API(bool)
JS_ClearStructuredClone(const uint64_t *data, size_t nbytes)
{
    ClearStructuredClone(data, nbytes);
    return true;
}

JS_PUBLIC_API(bool)
JS_StructuredCloneHasTransferables(const uint64_t *data, size_t nbytes,
                                   bool *hasTransferable)
{
    bool transferable;
    if (!StructuredCloneHasTransferObjects(data, nbytes, &transferable))
        return false;

    *hasTransferable = transferable;
    return true;
}

JS_PUBLIC_API(bool)
JS_StructuredClone(JSContext *cx, JS::HandleValue value, JS::MutableHandleValue vp,
                   const JSStructuredCloneCallbacks *optionalCallbacks,
                   void *closure)
{
    AssertHeapIsIdle(cx);
    CHECK_REQUEST(cx);

    // Strings are associated with zones, not compartments,
    // so we copy the string by wrapping it.
    if (value.isString()) {
      RootedString strValue(cx, value.toString());
      if (!cx->compartment()->wrap(cx, strValue.address())) {
        return false;
      }
      vp.setString(strValue);
      return true;
    }

    const JSStructuredCloneCallbacks *callbacks =
        optionalCallbacks ?
        optionalCallbacks :
        cx->runtime()->structuredCloneCallbacks;

    JSAutoStructuredCloneBuffer buf;
    {
        mozilla::Maybe<AutoCompartment> ac;
        if (value.isObject()) {
            ac.construct(cx, &value.toObject());
        }

        if (!buf.write(cx, value, callbacks, closure))
            return false;
    }

    return buf.read(cx, vp, callbacks, closure);
}

void
JSAutoStructuredCloneBuffer::clear()
{
    if (data_) {
        ClearStructuredClone(data_, nbytes_);
        data_ = nullptr;
        nbytes_ = 0;
        version_ = 0;
    }
}

bool
JSAutoStructuredCloneBuffer::copy(const uint64_t *srcData, size_t nbytes, uint32_t version)
{
    // transferable objects cannot be copied
    bool hasTransferable;
    if (!StructuredCloneHasTransferObjects(data_, nbytes_, &hasTransferable) ||
        hasTransferable)
        return false;

    uint64_t *newData = static_cast<uint64_t *>(js_malloc(nbytes));
    if (!newData)
        return false;

    js_memcpy(newData, srcData, nbytes);

    clear();
    data_ = newData;
    nbytes_ = nbytes;
    version_ = version;
    return true;
}

void
JSAutoStructuredCloneBuffer::adopt(uint64_t *data, size_t nbytes, uint32_t version)
{
    clear();
    data_ = data;
    nbytes_ = nbytes;
    version_ = version;
}

void
JSAutoStructuredCloneBuffer::steal(uint64_t **datap, size_t *nbytesp, uint32_t *versionp)
{
    *datap = data_;
    *nbytesp = nbytes_;
    if (versionp)
        *versionp = version_;

    data_ = nullptr;
    nbytes_ = 0;
    version_ = 0;
}

bool
JSAutoStructuredCloneBuffer::read(JSContext *cx, JS::MutableHandleValue vp,
                                  const JSStructuredCloneCallbacks *optionalCallbacks,
                                  void *closure)
{
    JS_ASSERT(cx);
    JS_ASSERT(data_);
    return !!JS_ReadStructuredClone(cx, data_, nbytes_, version_, vp,
                                    optionalCallbacks, closure);
}

bool
JSAutoStructuredCloneBuffer::write(JSContext *cx, JS::HandleValue value,
                                   const JSStructuredCloneCallbacks *optionalCallbacks,
                                   void *closure)
{
    JS::HandleValue transferable = JS::UndefinedHandleValue;
    return write(cx, value, transferable, optionalCallbacks, closure);
}

bool
JSAutoStructuredCloneBuffer::write(JSContext *cx, JS::HandleValue value,
                                   JS::HandleValue transferable,
                                   const JSStructuredCloneCallbacks *optionalCallbacks,
                                   void *closure)
{
    clear();
    bool ok = !!JS_WriteStructuredClone(cx, value, &data_, &nbytes_,
                                        optionalCallbacks, closure,
                                        transferable);
    if (!ok) {
        data_ = nullptr;
        nbytes_ = 0;
        version_ = JS_STRUCTURED_CLONE_VERSION;
    }
    return ok;
}

void
JSAutoStructuredCloneBuffer::swap(JSAutoStructuredCloneBuffer &other)
{
    uint64_t *data = other.data_;
    size_t nbytes = other.nbytes_;
    uint32_t version = other.version_;

    other.data_ = this->data_;
    other.nbytes_ = this->nbytes_;
    other.version_ = this->version_;

    this->data_ = data;
    this->nbytes_ = nbytes;
    this->version_ = version;
}

JS_PUBLIC_API(void)
JS_SetStructuredCloneCallbacks(JSRuntime *rt, const JSStructuredCloneCallbacks *callbacks)
{
    rt->structuredCloneCallbacks = callbacks;
}

JS_PUBLIC_API(bool)
JS_ReadUint32Pair(JSStructuredCloneReader *r, uint32_t *p1, uint32_t *p2)
{
    return r->input().readPair((uint32_t *) p1, (uint32_t *) p2);
}

JS_PUBLIC_API(bool)
JS_ReadBytes(JSStructuredCloneReader *r, void *p, size_t len)
{
    return r->input().readBytes(p, len);
}

JS_PUBLIC_API(bool)
JS_ReadTypedArray(JSStructuredCloneReader *r, JS::Value *vp)
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
        JS_ReportErrorNumber(r->context(), js_GetErrorMessage, nullptr,
                             JSMSG_SC_BAD_SERIALIZED_DATA, "expected type array");
        return false;
    }
}

JS_PUBLIC_API(bool)
JS_WriteUint32Pair(JSStructuredCloneWriter *w, uint32_t tag, uint32_t data)
{
    return w->output().writePair(tag, data);
}

JS_PUBLIC_API(bool)
JS_WriteBytes(JSStructuredCloneWriter *w, const void *p, size_t len)
{
    return w->output().writeBytes(p, len);
}

JS_PUBLIC_API(bool)
JS_WriteTypedArray(JSStructuredCloneWriter *w, JS::Value v)
{
    JS_ASSERT(v.isObject());
    assertSameCompartment(w->context(), v);
    RootedObject obj(w->context(), &v.toObject());

    // If the object is a security wrapper, see if we're allowed to unwrap it.
    // If we aren't, throw.
    if (obj->is<WrapperObject>())
        obj = CheckedUnwrap(obj);
    if (!obj) {
        JS_ReportError(w->context(), "Permission denied to access object");
        return false;
    }
    return w->writeTypedArray(obj);
}

