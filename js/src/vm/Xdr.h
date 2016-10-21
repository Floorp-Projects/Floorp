/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Xdr_h
#define vm_Xdr_h

#include "mozilla/EndianUtils.h"
#include "mozilla/TypeTraits.h"

#include "jsatom.h"
#include "jsfriendapi.h"

namespace js {

class XDRBuffer {
  public:
    XDRBuffer(JSContext* cx, JS::TranscodeBuffer& buffer, size_t cursor = 0)
      : context_(cx), buffer_(buffer), cursor_(cursor) { }

    JSContext* cx() const {
        return context_;
    }

    const uint8_t* read(size_t n) {
        MOZ_ASSERT(cursor_ < buffer_.length());
        uint8_t* ptr = &buffer_[cursor_];
        cursor_ += n;
        return ptr;
    }

    const char* readCString() {
        char* ptr = reinterpret_cast<char*>(&buffer_[cursor_]);
        uint8_t* end = reinterpret_cast<uint8_t*>(strchr(ptr, '\0')) + 1;
        MOZ_ASSERT(buffer_.begin() < end);
        MOZ_ASSERT(end <= buffer_.end());
        cursor_ = end - buffer_.begin();
        return ptr;
    }

    uint8_t* write(size_t n) {
        MOZ_ASSERT(n != 0);
        if (!buffer_.growByUninitialized(n)) {
            JS_ReportOutOfMemory(cx());
            return nullptr;
        }
        uint8_t* ptr = &buffer_[cursor_];
        cursor_ += n;
        return ptr;
    }

  private:
    JSContext* const context_;
    JS::TranscodeBuffer& buffer_;
    size_t cursor_;
};

/*
 * XDR serialization state.  All data is encoded in little endian.
 */
template <XDRMode mode>
class XDRState {
  public:
    XDRBuffer buf;
    JS::TranscodeResult resultCode_;

    XDRState(JSContext* cx, JS::TranscodeBuffer& buffer, size_t cursor = 0)
      : buf(cx, buffer, cursor), resultCode_(JS::TranscodeResult_Ok) { }

    JSContext* cx() const {
        return buf.cx();
    }

    // Record logical failures of XDR.
    void postProcessContextErrors(JSContext* cx);
    JS::TranscodeResult resultCode() const {
        return resultCode_;
    }
    bool fail(JS::TranscodeResult code) {
        MOZ_ASSERT(resultCode_ == JS::TranscodeResult_Ok);
        resultCode_ = code;
        return false;
    }

    bool codeUint8(uint8_t* n) {
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(sizeof(*n));
            if (!ptr)
                return false;
            *ptr = *n;
        } else {
            *n = *buf.read(sizeof(*n));
        }
        return true;
    }

    bool codeUint16(uint16_t* n) {
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(sizeof(*n));
            if (!ptr)
                return false;
            mozilla::LittleEndian::writeUint16(ptr, *n);
        } else {
            const uint8_t* ptr = buf.read(sizeof(*n));
            *n = mozilla::LittleEndian::readUint16(ptr);
        }
        return true;
    }

    bool codeUint32(uint32_t* n) {
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(sizeof(*n));
            if (!ptr)
                return false;
            mozilla::LittleEndian::writeUint32(ptr, *n);
        } else {
            const uint8_t* ptr = buf.read(sizeof(*n));
            *n = mozilla::LittleEndian::readUint32(ptr);
        }
        return true;
    }

    bool codeUint64(uint64_t* n) {
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(sizeof(*n));
            if (!ptr)
                return false;
            mozilla::LittleEndian::writeUint64(ptr, *n);
        } else {
            const uint8_t* ptr = buf.read(sizeof(*n));
            *n = mozilla::LittleEndian::readUint64(ptr);
        }
        return true;
    }

    /*
     * Use SFINAE to refuse any specialization which is not an enum.  Uses of
     * this function do not have to specialize the type of the enumerated field
     * as C++ will extract the parameterized from the argument list.
     */
    template <typename T>
    bool codeEnum32(T* val, typename mozilla::EnableIf<mozilla::IsEnum<T>::value, T>::Type * = NULL)
    {
        uint32_t tmp;
        if (mode == XDR_ENCODE)
            tmp = uint32_t(*val);
        if (!codeUint32(&tmp))
            return false;
        if (mode == XDR_DECODE)
            *val = T(tmp);
        return true;
    }

    bool codeDouble(double* dp) {
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

    bool codeBytes(void* bytes, size_t len) {
        if (len == 0)
            return true;
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(len);
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
    bool codeCString(const char** sp) {
        if (mode == XDR_ENCODE) {
            size_t n = strlen(*sp) + 1;
            uint8_t* ptr = buf.write(n);
            if (!ptr)
                return false;
            memcpy(ptr, *sp, n);
        } else {
            *sp = buf.readCString();
        }
        return true;
    }

    bool codeChars(const JS::Latin1Char* chars, size_t nchars);
    bool codeChars(char16_t* chars, size_t nchars);

    bool codeFunction(JS::MutableHandleFunction objp);
    bool codeScript(MutableHandleScript scriptp);
    bool codeConstValue(MutableHandleValue vp);
};

using XDREncoder = XDRState<XDR_ENCODE>;
using XDRDecoder = XDRState<XDR_DECODE>;

} /* namespace js */

#endif /* vm_Xdr_h */
