/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Xdr.h"

#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsscript.h"

#include "vm/Debugger.h"
#include "vm/EnvironmentObject.h"
#include "vm/TraceLogging.h"

using namespace js;
using mozilla::PodEqual;

template<XDRMode mode>
LifoAlloc&
XDRState<mode>::lifoAlloc() const {
    return buf.cx()->asJSContext()->tempLifoAlloc();
}

template<XDRMode mode>
void
XDRState<mode>::postProcessContextErrors(ExclusiveContext* cx)
{
    if (cx->isJSContext() && cx->asJSContext()->isExceptionPending()) {
        MOZ_ASSERT(resultCode_ == JS::TranscodeResult_Ok);
        resultCode_ = JS::TranscodeResult_Throw;
    }
}

template<XDRMode mode>
bool
XDRState<mode>::codeChars(const Latin1Char* chars, size_t nchars)
{
    static_assert(sizeof(Latin1Char) == sizeof(uint8_t), "Latin1Char must fit in 1 byte");

    MOZ_ASSERT(mode == XDR_ENCODE);

    if (nchars == 0)
        return true;
    uint8_t* ptr = buf.write(nchars);
    if (!ptr)
        return fail(JS::TranscodeResult_Throw);

    mozilla::PodCopy(ptr, chars, nchars);
    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeChars(char16_t* chars, size_t nchars)
{
    if (nchars == 0)
        return true;
    size_t nbytes = nchars * sizeof(char16_t);
    if (mode == XDR_ENCODE) {
        uint8_t* ptr = buf.write(nbytes);
        if (!ptr)
            return fail(JS::TranscodeResult_Throw);
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
    if (!xdr->cx()->buildIdOp() || !xdr->cx()->buildIdOp()(&buildId))
        return xdr->fail(JS::TranscodeResult_Failure_BadBuildId);
    MOZ_ASSERT(!buildId.empty());

    uint32_t buildIdLength;
    if (mode == XDR_ENCODE)
        buildIdLength = buildId.length();

    if (!xdr->codeUint32(&buildIdLength))
        return false;

    if (mode == XDR_DECODE && buildIdLength != buildId.length())
        return xdr->fail(JS::TranscodeResult_Failure_BadBuildId);

    if (mode == XDR_ENCODE) {
        if (!xdr->codeBytes(buildId.begin(), buildIdLength))
            return false;
    } else {
        JS::BuildIdCharVector decodedBuildId;

        // buildIdLength is already checked against the length of current
        // buildId.
        if (!decodedBuildId.resize(buildIdLength)) {
            ReportOutOfMemory(xdr->cx());
            return xdr->fail(JS::TranscodeResult_Throw);
        }

        if (!xdr->codeBytes(decodedBuildId.begin(), buildIdLength))
            return false;

        // We do not provide binary compatibility with older scripts.
        if (!PodEqual(decodedBuildId.begin(), buildId.begin(), buildIdLength))
            return xdr->fail(JS::TranscodeResult_Failure_BadBuildId);
    }

    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeFunction(MutableHandleFunction funp)
{
    TraceLoggerThread* logger = nullptr;
    if (cx()->isJSContext())
        logger = TraceLoggerForMainThread(cx()->asJSContext()->runtime());
    else
        logger = TraceLoggerForCurrentThread();
    TraceLoggerTextId event =
        mode == XDR_DECODE ? TraceLogger_DecodeFunction : TraceLogger_EncodeFunction;
    AutoTraceLog tl(logger, event);

    if (mode == XDR_DECODE)
        funp.set(nullptr);
    else
        MOZ_ASSERT(funp->nonLazyScript()->enclosingScope()->is<GlobalScope>());

    if (!VersionCheck(this)) {
        postProcessContextErrors(cx());
        return false;
    }

    RootedScope scope(cx(), &cx()->global()->emptyGlobalScope());
    if (!XDRInterpretedFunction(this, scope, nullptr, funp)) {
        postProcessContextErrors(cx());
        funp.set(nullptr);
        return false;
    }

    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeScript(MutableHandleScript scriptp)
{
    TraceLoggerThread* logger = nullptr;
    if (cx()->isJSContext())
        logger = TraceLoggerForMainThread(cx()->asJSContext()->runtime());
    else
        logger = TraceLoggerForCurrentThread();
    TraceLoggerTextId event =
        mode == XDR_DECODE ? TraceLogger_DecodeScript : TraceLogger_EncodeScript;
    AutoTraceLog tl(logger, event);

    if (mode == XDR_DECODE)
        scriptp.set(nullptr);
    else
        MOZ_ASSERT(!scriptp->enclosingScope());

    if (!VersionCheck(this)) {
        postProcessContextErrors(cx());
        return false;
    }

    if (!XDRScript(this, nullptr, nullptr, nullptr, scriptp)) {
        postProcessContextErrors(cx());
        scriptp.set(nullptr);
        return false;
    }

    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeConstValue(MutableHandleValue vp)
{
    return XDRScriptConst(this, vp);
}

template class js::XDRState<XDR_ENCODE>;
template class js::XDRState<XDR_DECODE>;
