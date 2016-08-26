/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Xdr.h"

#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsapi.h"
#include "jsscript.h"

#include "vm/Debugger.h"
#include "vm/EnvironmentObject.h"

using namespace js;
using mozilla::PodEqual;

void
XDRBuffer::freeBuffer()
{
    js_free(base);
#ifdef DEBUG
    memset(this, 0xe2, sizeof *this);
#endif
}

bool
XDRBuffer::grow(size_t n)
{
    MOZ_ASSERT(n > size_t(limit - cursor));

    const size_t MIN_CAPACITY = 8192;
    const size_t MAX_CAPACITY = size_t(INT32_MAX) + 1;
    size_t offset = cursor - base;
    MOZ_ASSERT(offset <= MAX_CAPACITY);
    if (n > MAX_CAPACITY - offset) {
        js::gc::AutoSuppressGC suppressGC(cx());
        JS_ReportErrorNumber(cx(), GetErrorMessage, nullptr, JSMSG_TOO_BIG_TO_ENCODE);
        return false;
    }
    size_t newCapacity = mozilla::RoundUpPow2(offset + n);
    if (newCapacity < MIN_CAPACITY)
        newCapacity = MIN_CAPACITY;

    MOZ_ASSERT(newCapacity <= MAX_CAPACITY);
    void* data = js_realloc(base, newCapacity);
    if (!data) {
        ReportOutOfMemory(cx());
        return false;
    }
    base = static_cast<uint8_t*>(data);
    cursor = base + offset;
    limit = base + newCapacity;
    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeChars(const Latin1Char* chars, size_t nchars)
{
    static_assert(sizeof(Latin1Char) == sizeof(uint8_t), "Latin1Char must fit in 1 byte");

    MOZ_ASSERT(mode == XDR_ENCODE);

    uint8_t* ptr = buf.write(nchars);
    if (!ptr)
        return false;

    mozilla::PodCopy(ptr, chars, nchars);
    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeChars(char16_t* chars, size_t nchars)
{
    size_t nbytes = nchars * sizeof(char16_t);
    if (mode == XDR_ENCODE) {
        uint8_t* ptr = buf.write(nbytes);
        if (!ptr)
            return false;
        mozilla::NativeEndian::copyAndSwapToLittleEndian(ptr, chars, nchars);
    } else {
        const uint8_t* ptr = buf.read(nbytes);
        mozilla::NativeEndian::copyAndSwapFromLittleEndian(chars, ptr, nchars);
    }
    return true;
}

template<XDRMode mode>
static bool
VersionCheck(XDRState<mode>* xdr)
{
    JS::BuildIdCharVector buildId;
    if (!xdr->cx()->buildIdOp() || !xdr->cx()->buildIdOp()(&buildId)) {
        JS_ReportErrorNumber(xdr->cx(), GetErrorMessage, nullptr, JSMSG_BUILD_ID_NOT_AVAILABLE);
        return false;
    }
    MOZ_ASSERT(!buildId.empty());

    uint32_t buildIdLength;
    if (mode == XDR_ENCODE)
        buildIdLength = buildId.length();

    if (!xdr->codeUint32(&buildIdLength))
        return false;

    if (mode == XDR_DECODE && buildIdLength != buildId.length()) {
        JS_ReportErrorNumber(xdr->cx(), GetErrorMessage, nullptr, JSMSG_BAD_BUILD_ID);
        return false;
    }

    if (mode == XDR_ENCODE) {
        if (!xdr->codeBytes(buildId.begin(), buildIdLength))
            return false;
    } else {
        JS::BuildIdCharVector decodedBuildId;

        // buildIdLength is already checked against the length of current
        // buildId.
        if (!decodedBuildId.resize(buildIdLength)) {
            ReportOutOfMemory(xdr->cx());
            return false;
        }

        if (!xdr->codeBytes(decodedBuildId.begin(), buildIdLength))
            return false;

        if (!PodEqual(decodedBuildId.begin(), buildId.begin(), buildIdLength)) {
            // We do not provide binary compatibility with older scripts.
            JS_ReportErrorNumber(xdr->cx(), GetErrorMessage, nullptr, JSMSG_BAD_BUILD_ID);
            return false;
        }
    }

    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeFunction(MutableHandleFunction objp)
{
    if (mode == XDR_DECODE)
        objp.set(nullptr);
    else
        MOZ_ASSERT(objp->nonLazyScript()->enclosingScope()->is<GlobalScope>());

    if (!VersionCheck(this))
        return false;

    RootedScope scope(cx(), &cx()->global()->emptyGlobalScope());
    return XDRInterpretedFunction(this, scope, nullptr, objp);
}

template<XDRMode mode>
bool
XDRState<mode>::codeScript(MutableHandleScript scriptp)
{
    if (mode == XDR_DECODE)
        scriptp.set(nullptr);
    else
        MOZ_ASSERT(!scriptp->enclosingScope());

    if (!VersionCheck(this))
        return false;

    return XDRScript(this, nullptr, nullptr, nullptr, scriptp);
}

template<XDRMode mode>
bool
XDRState<mode>::codeConstValue(MutableHandleValue vp)
{
    return XDRScriptConst(this, vp);
}

XDRDecoder::XDRDecoder(JSContext* cx, const void* data, uint32_t length)
  : XDRState<XDR_DECODE>(cx)
{
    buf.setData(data, length);
}

template class js::XDRState<XDR_ENCODE>;
template class js::XDRState<XDR_DECODE>;
