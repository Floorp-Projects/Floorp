/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Xdr_h___
#define Xdr_h___

#include "jsapi.h"
#include "jsprvtd.h"
#include "jsnum.h"

#include "vm/NumericConversions.h"

namespace js {

/*
 * Bytecode version number. Increment the subtrahend whenever JS bytecode
 * changes incompatibly.
 *
 * This version number is XDR'd near the front of xdr bytecode and
 * aborts deserialization if there is a mismatch between the current
 * and saved versions. If deserialization fails, the data should be
 * invalidated if possible.
 */
static const uint32_t XDR_BYTECODE_VERSION = uint32_t(0xb973c0de - 117);

class XDRBuffer {
  public:
    XDRBuffer(JSContext *cx)
      : context(cx), base(NULL), cursor(NULL), limit(NULL) { }

    JSContext *cx() const {
        return context;
    }

    void *getData(uint32_t *lengthp) const {
        JS_ASSERT(size_t(cursor - base) <= size_t(UINT32_MAX));
        *lengthp = uint32_t(cursor - base);
        return base;
    }

    void setData(const void *data, uint32_t length) {
        base = static_cast<uint8_t *>(const_cast<void *>(data));
        cursor = base;
        limit = base + length;
    }

    const uint8_t *read(size_t n) {
        JS_ASSERT(n <= size_t(limit - cursor));
        uint8_t *ptr = cursor;
        cursor += n;
        return ptr;
    }

    const char *readCString() {
        char *ptr = reinterpret_cast<char *>(cursor);
        cursor = reinterpret_cast<uint8_t *>(strchr(ptr, '\0')) + 1;
        JS_ASSERT(base < cursor);
        JS_ASSERT(cursor <= limit);
        return ptr;
    }

    uint8_t *write(size_t n) {
        if (n > size_t(limit - cursor)) {
            if (!grow(n))
                return NULL;
        }
        uint8_t *ptr = cursor;
        cursor += n;
        return ptr;
    }

    static bool isUint32Overflow(size_t n) {
        return size_t(-1) > size_t(UINT32_MAX) && n > size_t(UINT32_MAX);
    }

    void freeBuffer();

  private:
    bool grow(size_t n);

    JSContext   *const context;
    uint8_t     *base;
    uint8_t     *cursor;
    uint8_t     *limit;
};

/* We use little-endian byteorder for all encoded data */

#if defined IS_LITTLE_ENDIAN

inline uint32_t
NormalizeByteOrder32(uint32_t x)
{
    return x;
}

inline uint16_t
NormalizeByteOrder16(uint16_t x)
{
    return x;
}

#elif defined IS_BIG_ENDIAN

inline uint32_t
NormalizeByteOrder32(uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

inline uint16_t
NormalizeByteOrder16(uint16_t x)
{
    return (x >> 8) | (x << 8);
}

#else
#error "unknown byte order"
#endif

template <XDRMode mode>
class XDRState {
  public:
    XDRBuffer buf;

  protected:
    JSPrincipals *principals;
    JSPrincipals *originPrincipals;

    XDRState(JSContext *cx)
      : buf(cx), principals(NULL), originPrincipals(NULL) {
    }

  public:
    JSContext *cx() const {
        return buf.cx();
    }

    bool codeUint8(uint8_t *n) {
        if (mode == XDR_ENCODE) {
            uint8_t *ptr = buf.write(sizeof *n);
            if (!ptr)
                return false;
            *ptr = *n;
        } else {
            *n = *buf.read(sizeof *n);
        }
        return true;
    }

    bool codeUint16(uint16_t *n) {
        uint16_t tmp;
        if (mode == XDR_ENCODE) {
            uint8_t *ptr = buf.write(sizeof tmp);
            if (!ptr)
                return false;
            tmp = NormalizeByteOrder16(*n);
            memcpy(ptr, &tmp, sizeof tmp);
        } else {
            memcpy(&tmp, buf.read(sizeof tmp), sizeof tmp);
            *n = NormalizeByteOrder16(tmp);
        }
        return true;
    }

    bool codeUint32(uint32_t *n) {
        uint32_t tmp;
        if (mode == XDR_ENCODE) {
            uint8_t *ptr = buf.write(sizeof tmp);
            if (!ptr)
                return false;
            tmp = NormalizeByteOrder32(*n);
            memcpy(ptr, &tmp, sizeof tmp);
        } else {
            memcpy(&tmp, buf.read(sizeof tmp), sizeof tmp);
            *n = NormalizeByteOrder32(tmp);
        }
        return true;
    }

    bool codeUint64(uint64_t *n) {
        if (mode == XDR_ENCODE) {
            uint8_t *ptr = buf.write(sizeof(*n));
            if (!ptr)
                return false;
            ptr[0] = (*n >>  0) & 0xFF;
            ptr[1] = (*n >>  8) & 0xFF;
            ptr[2] = (*n >> 16) & 0xFF;
            ptr[3] = (*n >> 24) & 0xFF;
            ptr[4] = (*n >> 32) & 0xFF;
            ptr[5] = (*n >> 40) & 0xFF;
            ptr[6] = (*n >> 48) & 0xFF;
            ptr[7] = (*n >> 56) & 0xFF;
        } else {
            const uint8_t *ptr = buf.read(sizeof(*n));
            *n = (uint64_t(ptr[0]) <<  0) |
                 (uint64_t(ptr[1]) <<  8) |
                 (uint64_t(ptr[2]) << 16) |
                 (uint64_t(ptr[3]) << 24) |
                 (uint64_t(ptr[4]) << 32) |
                 (uint64_t(ptr[5]) << 40) |
                 (uint64_t(ptr[6]) << 48) |
                 (uint64_t(ptr[7]) << 56);
        }
        return true;
    }

    bool codeDouble(double *dp) {
        union DoublePun {
            double d;
            uint64_t u;
        } pun;
        if (mode == XDR_ENCODE)
            pun.d = *dp;
        if (!codeUint64(&pun.u))
            return false;
        if (mode == XDR_DECODE)
            *dp = pun.d;
        return true;
    }

    bool codeBytes(void *bytes, size_t len) {
        if (mode == XDR_ENCODE) {
            uint8_t *ptr = buf.write(len);
            if (!ptr)
                return false;
            memcpy(ptr, bytes, len);
        } else {
            memcpy(bytes, buf.read(len), len);
        }
        return true;
    }

    /*
     * During encoding the string is written into the buffer together with its
     * terminating '\0'. During decoding the method returns a pointer into the
     * decoding buffer and the caller must copy the string if it will outlive
     * the decoding buffer.
     */
    bool codeCString(const char **sp) {
        if (mode == XDR_ENCODE) {
            size_t n = strlen(*sp) + 1;
            uint8_t *ptr = buf.write(n);
            if (!ptr)
                return false;
            memcpy(ptr, *sp, n);
        } else {
            *sp = buf.readCString();
        }
        return true;
    }

    bool codeChars(jschar *chars, size_t nchars);

    bool codeFunction(JSObject **objp);
    bool codeScript(JSScript **scriptp);

    void initScriptPrincipals(JSScript *script) {
        JS_ASSERT(mode == XDR_DECODE);

        /* The origin principals must be normalized at this point. */
        JS_ASSERT_IF(principals, originPrincipals);
        JS_ASSERT(!script->principals);
        JS_ASSERT(!script->originPrincipals);
        if (principals) {
            script->principals = principals;
            JS_HoldPrincipals(principals);
        }
        if (originPrincipals) {
            script->originPrincipals = originPrincipals;
            JS_HoldPrincipals(originPrincipals);
        }
    }
};

class XDREncoder : public XDRState<XDR_ENCODE> {
  public:
    XDREncoder(JSContext *cx)
      : XDRState<XDR_ENCODE>(cx) {
    }

    ~XDREncoder() {
        buf.freeBuffer();
    }

    const void *getData(uint32_t *lengthp) const {
        return buf.getData(lengthp);
    }

    void *forgetData(uint32_t *lengthp) {
        void *data = buf.getData(lengthp);
        buf.setData(NULL, 0);
        return data;
    }
};

class XDRDecoder : public XDRState<XDR_DECODE> {
  public:
    XDRDecoder(JSContext *cx, const void *data, uint32_t length,
               JSPrincipals *principals, JSPrincipals *originPrincipals);

};

} /* namespace js */

#endif /* Xdr_h___ */
