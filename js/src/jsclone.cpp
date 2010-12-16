/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript structured data serialization.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jsclone.h"
#include "jsdate.h"
#include "jsregexp.h"
#include "jstypedarray.h"

#include "jsregexpinlines.h"

using namespace js;

namespace js
{

bool
WriteStructuredClone(JSContext *cx, const Value &v, uint64 **bufp, size_t *nbytesp)
{
    SCOutput out(cx);
    JSStructuredCloneWriter w(out);
    return w.init() && w.write(v) && out.extractBuffer(bufp, nbytesp);
}

bool
ReadStructuredClone(JSContext *cx, const uint64_t *data, size_t nbytes, Value *vp)
{
    SCInput in(cx, data, nbytes);
    JSStructuredCloneReader r(in);
    return r.read(vp);
}

}

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
    SCTAG_TYPED_ARRAY_MIN = 0xFFFF0100,
    SCTAG_TYPED_ARRAY_MAX = SCTAG_TYPED_ARRAY_MIN + TypedArray::TYPE_MAX - 1,
    SCTAG_END_OF_BUILTIN_TYPES
};

JS_STATIC_ASSERT(SCTAG_END_OF_BUILTIN_TYPES <= JS_SCTAG_USER_MIN);
JS_STATIC_ASSERT(JS_SCTAG_USER_MIN <= JS_SCTAG_USER_MAX);

static uint8_t
SwapBytes(uint8_t u)
{
    return u;
}

static uint16_t
SwapBytes(uint16_t u)
{
#ifdef IS_BIG_ENDIAN
    return ((u & 0x00ff) << 8) | ((u & 0xff00) >> 8);
#else
    return u;
#endif
}

static uint32_t
SwapBytes(uint32_t u)
{
#ifdef IS_BIG_ENDIAN
    return ((u & 0x000000ffU) << 24) |
           ((u & 0x0000ff00U) << 8) |
           ((u & 0x00ff0000U) >> 8) |
           ((u & 0xff000000U) >> 24);
#else
    return u;
#endif
}

static uint64_t
SwapBytes(uint64_t u)
{
#ifdef IS_BIG_ENDIAN
    return ((u & 0x00000000000000ffLLU) << 56) |
           ((u & 0x000000000000ff00LLU) << 40) |
           ((u & 0x0000000000ff0000LLU) << 24) |
           ((u & 0x00000000ff000000LLU) << 8) |
           ((u & 0x000000ff00000000LLU) >> 8) |
           ((u & 0x0000ff0000000000LLU) >> 24) |
           ((u & 0x00ff000000000000LLU) >> 40) |
           ((u & 0xff00000000000000LLU) >> 56);
#else
    return u;
#endif
}

bool
SCInput::eof()
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_SC_BAD_SERIALIZED_DATA, "truncated");
    return false;
}

SCInput::SCInput(JSContext *cx, const uint64_t *data, size_t nbytes)
    : cx(cx), point(data), end(data + nbytes / 8)
{
    JS_ASSERT((uintptr_t(data) & 7) == 0);
    JS_ASSERT((nbytes & 7) == 0);
}

bool
SCInput::read(uint64_t *p)
{
    if (point == end)
        return eof();
    *p = SwapBytes(*point++);
    return true;
}

bool
SCInput::readPair(uint32_t *tagp, uint32_t *datap)
{
    uint64_t u = 0;     /* initialize to shut GCC up */
    bool ok = read(&u);
    if (ok) {
        *tagp = uint32_t(u >> 32);
        *datap = uint32_t(u);
    }
    return ok;
}

bool
SCInput::readDouble(jsdouble *p)
{
    union {
        uint64_t u;
        jsdouble d;
    } pun;
    if (!read(&pun.u))
        return false;
    *p = JS_CANONICALIZE_NAN(pun.d);
    return true;
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

    if (sizeof(T) == 1) {
        memcpy(p, point, nelems);
    } else {
        const T *q = (const T *) point;
        const T *qend = q + nelems;
        while (q != qend)
            *p++ = SwapBytes(*q++);
    }
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

SCOutput::SCOutput(JSContext *cx) : cx(cx), buf(cx) {}

bool
SCOutput::write(uint64_t u)
{
    return buf.append(SwapBytes(u));
}

static inline uint64_t
PairToUInt64(uint32_t tag, uint32_t data)
{
    return uint64_t(data) | (uint64_t(tag) << 32);
}

bool
SCOutput::writePair(uint32_t tag, uint32_t data)
{
    /*
     * As it happens, the tag word appears after the data word in the output.
     * This is because exponents occupy the last 2 bytes of jsdoubles on the
     * little-endian platforms we care most about.
     *
     * For example, JSVAL_TRUE is written using writePair(SCTAG_BOOLEAN, 1).
     * PairToUInt64 produces the number 0xFFFF000200000001.
     * That is written out as the bytes 01 00 00 00 02 00 FF FF.
     */
    return write(PairToUInt64(tag, data));
}

static inline uint64_t
ReinterpretDoubleAsUInt64(jsdouble d)
{
    union {
        jsdouble d;
        uint64_t u;
    } pun;
    pun.d = d;
    return pun.u;
}

static inline jsdouble
ReinterpretUInt64AsDouble(uint64_t u)
{
    union {
        uint64_t u;
        jsdouble d;
    } pun;
    pun.u = u;
    return pun.d;
}

static inline jsdouble
ReinterpretPairAsDouble(uint32_t tag, uint32_t data)
{
    return ReinterpretUInt64AsDouble(PairToUInt64(tag, data));
}

static inline bool
IsNonCanonicalizedNaN(jsdouble d)
{
    return ReinterpretDoubleAsUInt64(d) != ReinterpretDoubleAsUInt64(JS_CANONICALIZE_NAN(d));
}

bool
SCOutput::writeDouble(jsdouble d)
{
    JS_ASSERT(!IsNonCanonicalizedNaN(d));
    return write(ReinterpretDoubleAsUInt64(d));
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
    if (sizeof(T) == 1) {
        memcpy(q, p, nelems);
    } else {
        const T *pend = p + nelems;
        while (p != pend)
            *q++ = SwapBytes(*p++);
    }
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
SCOutput::extractBuffer(uint64_t **datap, size_t *sizep)
{
    *sizep = buf.length() * sizeof(uint64_t);
    return (*datap = buf.extractRawBuffer()) != NULL;
}

JS_STATIC_ASSERT(JSString::MAX_LENGTH < UINT32_MAX);

bool
JSStructuredCloneWriter::writeString(uint32_t tag, JSString *str)
{
    const jschar *chars;
    size_t length;
    str->getCharsAndLength(chars, length);
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

    size_t limit = JS_MIN(counts.length(), MAX);
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

    JS_ASSERT(memory.count() == objs.length());
    size_t j = objs.length();
    for (size_t i = 0; i < limit; i++)
        JS_ASSERT(memory.has(&objs[--j].toObject()));
#endif
}

static inline uint32_t
ArrayTypeToTag(uint32_t type)
{
    /*
     * As long as these are all true, we can just add.  Note that for backward
     * compatibility, the tags cannot change.  So if the ArrayType type codes
     * change, this function and TagToArrayType will have to do more work.
     */
    JS_STATIC_ASSERT(TypedArray::TYPE_INT8 == 0);
    JS_STATIC_ASSERT(TypedArray::TYPE_UINT8 == 1);
    JS_STATIC_ASSERT(TypedArray::TYPE_INT16 == 2);
    JS_STATIC_ASSERT(TypedArray::TYPE_UINT16 == 3);
    JS_STATIC_ASSERT(TypedArray::TYPE_INT32 == 4);
    JS_STATIC_ASSERT(TypedArray::TYPE_UINT32 == 5);
    JS_STATIC_ASSERT(TypedArray::TYPE_FLOAT32 == 6);
    JS_STATIC_ASSERT(TypedArray::TYPE_FLOAT64 == 7);
    JS_STATIC_ASSERT(TypedArray::TYPE_UINT8_CLAMPED == 8);
    JS_STATIC_ASSERT(TypedArray::TYPE_MAX == TypedArray::TYPE_UINT8_CLAMPED + 1);

    JS_ASSERT(type < TypedArray::TYPE_MAX);
    return SCTAG_TYPED_ARRAY_MIN + type;
}

static inline uint32_t
TagToArrayType(uint32_t tag)
{
    JS_ASSERT(SCTAG_TYPED_ARRAY_MIN <= tag && tag <= SCTAG_TYPED_ARRAY_MAX);
    return tag - SCTAG_TYPED_ARRAY_MIN;
}

bool
JSStructuredCloneWriter::writeTypedArray(JSObject *obj)
{
    TypedArray *arr = TypedArray::fromJSObject(obj);
    if (!out.writePair(ArrayTypeToTag(arr->type), arr->length))
        return false;

    switch (arr->type) {
    case TypedArray::TYPE_INT8:
    case TypedArray::TYPE_UINT8:
    case TypedArray::TYPE_UINT8_CLAMPED:
        return out.writeArray((const uint8_t *) arr->data, arr->length);
    case TypedArray::TYPE_INT16:
    case TypedArray::TYPE_UINT16:
        return out.writeArray((const uint16_t *) arr->data, arr->length);
    case TypedArray::TYPE_INT32:
    case TypedArray::TYPE_UINT32:
    case TypedArray::TYPE_FLOAT32:
        return out.writeArray((const uint32_t *) arr->data, arr->length);
    case TypedArray::TYPE_FLOAT64:
        return out.writeArray((const uint64_t *) arr->data, arr->length);
    default:
        JS_NOT_REACHED("unknown TypedArray type");
        return false;
    }
}

bool
JSStructuredCloneWriter::writeArrayBuffer(JSObject *obj)
{
    ArrayBuffer *abuf = ArrayBuffer::fromJSObject(obj);
    return out.writePair(SCTAG_ARRAY_BUFFER_OBJECT, abuf->byteLength) &&
           out.writeBytes(abuf->data, abuf->byteLength);
}

bool
JSStructuredCloneWriter::startObject(JSObject *obj)
{
    JS_ASSERT(obj->isArray() || obj->isObject());

    /* Fail if obj is already on the stack. */
    HashSet<JSObject *>::AddPtr p = memory.lookupForAdd(obj);
    if (p) {
        JSContext *cx = context();
        const JSStructuredCloneCallbacks *cb = cx->runtime->structuredCloneCallbacks;
        if (cb)
            cb->reportError(cx, JS_SCERR_RECURSION);
        else
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_SC_RECURSION);
        return false;
    }
    if (!memory.add(p, obj))
        return false;

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
    return out.writePair(obj->isArray() ? SCTAG_ARRAY_OBJECT : SCTAG_OBJECT_OBJECT, 0);
}

bool
JSStructuredCloneWriter::startWrite(const js::Value &v)
{
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
        JSObject *obj = &v.toObject();
        if (obj->isRegExp()) {
            RegExp *re = RegExp::extractFrom(obj);
            return out.writePair(SCTAG_REGEXP_OBJECT, re->getFlags()) &&
                   writeString(SCTAG_STRING, re->getSource());
        } else if (obj->isDate()) {
            jsdouble d = js_DateGetMsecSinceEpoch(context(), obj);
            return out.writePair(SCTAG_DATE_OBJECT, 0) && out.writeDouble(d);
        } else if (obj->isObject() || obj->isArray()) {
            return startObject(obj);
        } else if (js_IsTypedArray(obj)) {
            return writeTypedArray(obj);
        } else if (js_IsArrayBuffer(obj) && ArrayBuffer::fromJSObject(obj)) {
            return writeArrayBuffer(obj);
        } else if (obj->isBoolean()) {
            return out.writePair(SCTAG_BOOLEAN_OBJECT, obj->getPrimitiveThis().toBoolean());
        } else if (obj->isNumber()) {
            return out.writePair(SCTAG_NUMBER_OBJECT, 0) &&
                   out.writeDouble(obj->getPrimitiveThis().toNumber());
        } else if (obj->isString()) {
            return writeString(SCTAG_STRING_OBJECT, obj->getPrimitiveThis().toString());
        }

        const JSStructuredCloneCallbacks *cb = context()->runtime->structuredCloneCallbacks;
        if (cb)
            return cb->write(context(), this, obj);
        /* else fall through */
    }

    JS_ReportErrorNumber(context(), js_GetErrorMessage, NULL, JSMSG_SC_UNSUPPORTED_TYPE);
    return false;
}

bool
JSStructuredCloneWriter::write(const Value &v)
{
    if (!startWrite(v))
        return false;

    while (!counts.empty()) {
        JSObject *obj = &objs.back().toObject();
        if (counts.back()) {
            counts.back()--;
            jsid id = ids.back();
            ids.popBack();
            checkStack();
            if (JSID_IS_STRING(id) || JSID_IS_INT(id)) {
                /*
                 * If obj still has an own property named id, write it out.
                 * The cost of re-checking could be avoided by using
                 * NativeIterators.
                 */
                JSObject *obj2;
                JSProperty *prop;
                if (!js_HasOwnProperty(context(), obj->getOps()->lookupProperty, obj, id,
                                       &obj2, &prop)) {
                    return false;
                }

                if (prop) {
                    Value val;
                    if (!writeId(id) || !obj->getProperty(context(), id, &val) || !startWrite(val))
                        return false;
                }
            }
        } else {
            out.writePair(SCTAG_NULL, 0);
            memory.remove(obj);
            objs.popBack();
            counts.popBack();
        }
    }
    return true;
}

bool
JSStructuredCloneReader::checkDouble(jsdouble d)
{
    if (IsNonCanonicalizedNaN(d)) {
        JS_ReportErrorNumber(context(), js_GetErrorMessage, NULL,
                             JSMSG_SC_BAD_SERIALIZED_DATA, "unrecognized NaN");
        return false;
    }
    return true;
}

class Chars {
    JSContext *cx;
    jschar *p;
  public:
    Chars() : p(NULL) {}
    ~Chars() { if (p) cx->free(p); }

    bool allocate(JSContext *cx, size_t len) {
        JS_ASSERT(!p);
        // We're going to null-terminate!
        p = (jschar *) cx->malloc((len + 1) * sizeof(jschar));
        this->cx = cx;
        if (p) {
            p[len] = jschar(0);
            return true;
        }
        return false;
    }
    jschar *get() { return p; }
    void forget() { p = NULL; }
};

JSString *
JSStructuredCloneReader::readString(uint32_t nchars)
{
    if (nchars > JSString::MAX_LENGTH) {
        JS_ReportErrorNumber(context(), js_GetErrorMessage, NULL, JSMSG_SC_BAD_SERIALIZED_DATA,
                             "string length");
        return NULL;
    }
    Chars chars;
    if (!chars.allocate(context(), nchars) || !in.readChars(chars.get(), nchars))
        return NULL;
    JSString *str = js_NewString(context(), chars.get(), nchars);
    if (str)
        chars.forget();
    return str;
}

bool
JSStructuredCloneReader::readTypedArray(uint32_t tag, uint32_t nelems, Value *vp)
{
    uint32_t atype = TagToArrayType(tag);
    JSObject *obj = js_CreateTypedArray(context(), atype, nelems);
    if (!obj)
        return false;
    vp->setObject(*obj);

    TypedArray *arr = TypedArray::fromJSObject(obj);
    JS_ASSERT(arr->length == nelems);
    JS_ASSERT(arr->type == atype);
    switch (atype) {
      case TypedArray::TYPE_INT8:
      case TypedArray::TYPE_UINT8:
      case TypedArray::TYPE_UINT8_CLAMPED:
        return in.readArray((uint8_t *) arr->data, nelems);
      case TypedArray::TYPE_INT16:
      case TypedArray::TYPE_UINT16:
        return in.readArray((uint16_t *) arr->data, nelems);
      case TypedArray::TYPE_INT32:
      case TypedArray::TYPE_UINT32:
      case TypedArray::TYPE_FLOAT32:
        return in.readArray((uint32_t *) arr->data, nelems);
      case TypedArray::TYPE_FLOAT64:
        return in.readArray((uint64_t *) arr->data, nelems);
      default:
        JS_NOT_REACHED("unknown TypedArray type");
        return false;
    }
}

bool
JSStructuredCloneReader::readArrayBuffer(uint32_t nbytes, Value *vp)
{
    JSObject *obj = js_CreateArrayBuffer(context(), nbytes);
    if (!obj)
        return false;
    vp->setObject(*obj);
    ArrayBuffer *abuf = ArrayBuffer::fromJSObject(obj);
    JS_ASSERT(abuf->byteLength == nbytes);
    return in.readArray((uint8_t *) abuf->data, nbytes);
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
        if (tag == SCTAG_BOOLEAN_OBJECT && !js_PrimitiveToObject(context(), vp))
            return false;
        break;

      case SCTAG_STRING:
      case SCTAG_STRING_OBJECT: {
        JSString *str = readString(data);
        if (!str)
            return false;
        vp->setString(str);
        if (tag == SCTAG_STRING_OBJECT && !js_PrimitiveToObject(context(), vp))
            return false;
        break;
      }

      case SCTAG_NUMBER_OBJECT: {
        jsdouble d;
        if (!in.readDouble(&d) || !checkDouble(d))
            return false;
        vp->setDouble(d);
        if (!js_PrimitiveToObject(context(), vp))
            return false;
        break;
      }

      case SCTAG_DATE_OBJECT: {
        jsdouble d;
        if (!in.readDouble(&d) || !checkDouble(d))
            return false;
        if (d == d && d != TIMECLIP(d)) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, NULL, JSMSG_SC_BAD_SERIALIZED_DATA,
                                 "date");
            return false;
        }
        JSObject *obj = js_NewDateObjectMsec(context(), d);
        if (!obj)
            return false;
        vp->setObject(*obj);
        break;
      }

      case SCTAG_REGEXP_OBJECT: {
        uint32_t tag2, nchars;
        if (!in.readPair(&tag2, &nchars))
            return false;
        if (tag2 != SCTAG_STRING) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, NULL, JSMSG_SC_BAD_SERIALIZED_DATA,
                                 "regexp");
            return false;
        }
        JSString *str = readString(nchars);
        if (!str)
            return false;
        const jschar *chars;
        size_t length;
        str->getCharsAndLength(chars, length);
        JSObject *obj = RegExp::createObjectNoStatics(context(), chars, length, data);
        if (!obj)
            return false;
        vp->setObject(*obj);
        break;
      }

      case SCTAG_ARRAY_OBJECT:
      case SCTAG_OBJECT_OBJECT: {
        JSObject *obj = (tag == SCTAG_ARRAY_OBJECT)
                        ? NewDenseEmptyArray(context())
                        : NewBuiltinClassInstance(context(), &js_ObjectClass);
        if (!obj || !objs.append(ObjectValue(*obj)))
            return false;
        vp->setObject(*obj);
        break;
      }

      case SCTAG_ARRAY_BUFFER_OBJECT:
        return readArrayBuffer(data, vp);

      default: {
        if (tag <= SCTAG_FLOAT_MAX) {
            jsdouble d = ReinterpretPairAsDouble(tag, data);
            if (!checkDouble(d))
                return false;
            vp->setNumber(d);
            break;
        }

        if (SCTAG_TYPED_ARRAY_MIN <= tag && tag <= SCTAG_TYPED_ARRAY_MAX)
            return readTypedArray(tag, data, vp);

        const JSStructuredCloneCallbacks *cb = context()->runtime->structuredCloneCallbacks;
        if (!cb) {
            JS_ReportErrorNumber(context(), js_GetErrorMessage, NULL, JSMSG_SC_BAD_SERIALIZED_DATA,
                                 "unsupported type");
            return false;
        }
        JSObject *obj = cb->read(context(), this, tag, data);
        if (!obj)
            return false;
        vp->setObject(*obj);
      }
    }
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
        JSAtom *atom = js_AtomizeString(context(), str, 0);
        if (!atom)
            return false;
        *idp = ATOM_TO_JSID(atom);
        return true;
    }
    if (tag == SCTAG_NULL) {
        *idp = JSID_VOID;
        return true;
    }
    JS_ReportErrorNumber(context(), js_GetErrorMessage, NULL, JSMSG_SC_BAD_SERIALIZED_DATA, "id");
    return false;
}

bool
JSStructuredCloneReader::read(Value *vp)
{
    if (!startRead(vp))
        return false;

    while (objs.length() != 0) {
        JSObject *obj = &objs.back().toObject();

        jsid id;
        if (!readId(&id))
            return false;

        if (JSID_IS_VOID(id)) {
            objs.popBack();
        } else {
            Value v;
            if (!startRead(&v) || !obj->defineProperty(context(), id, v))
                return false;
        }
    }
    return true;
}
