/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "jsversion.h"

#include <string.h>
#include "jstypes.h"
#include "jsutil.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsnum.h"
#include "jsscript.h"
#include "jsstr.h"

#include "Xdr.h"
#include "Debugger.h"

#include "jsobjinlines.h"

using namespace mozilla;
using namespace js;

namespace js {

void
XDRBuffer::freeBuffer()
{
    Foreground::free_(base);
#ifdef DEBUG
    memset(this, 0xe2, sizeof *this);
#endif
}

bool
XDRBuffer::grow(size_t n)
{
    JS_ASSERT(n > size_t(limit - cursor));

    const size_t MEM_BLOCK = 8192;
    size_t offset = cursor - base;
    size_t newCapacity = JS_ROUNDUP(offset + n, MEM_BLOCK);
    if (isUint32Overflow(newCapacity)) {
        JS_ReportErrorNumber(cx(), js_GetErrorMessage, NULL, JSMSG_TOO_BIG_TO_ENCODE);
        return false;
    }

    void *data = OffTheBooks::realloc_(base, newCapacity);
    if (!data) {
        js_ReportOutOfMemory(cx());
        return false;
    }
    base = static_cast<uint8_t *>(data);
    cursor = base + offset;
    limit = base + newCapacity;
    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeChars(jschar *chars, size_t nchars)
{
    size_t nbytes = nchars * sizeof(jschar);
    if (mode == XDR_ENCODE) {
        uint8_t *ptr = buf.write(nbytes);
        if (!ptr)
            return false;
#ifdef IS_LITTLE_ENDIAN
        memcpy(ptr, chars, nbytes);
#else
        for (size_t i = 0; i != nchars; i++) {
            uint16_t tmp = NormalizeByteOrder16(chars[i]);
            memcpy(ptr, &tmp, sizeof tmp);
            ptr += sizeof tmp;
        }
#endif
    } else {
        const uint8_t *ptr = buf.read(nbytes);
#ifdef IS_LITTLE_ENDIAN
        memcpy(chars, ptr, nbytes);
#else
        for (size_t i = 0; i != nchars; i++) {
            uint16_t tmp;
            memcpy(&tmp, ptr, sizeof tmp);
            chars[i] = NormalizeByteOrder16(tmp);
            ptr += sizeof tmp;
        }
#endif
    }
    return true;
}

template<XDRMode mode>
static bool
VersionCheck(XDRState<mode> *xdr)
{
    uint32_t bytecodeVer;
    if (mode == XDR_ENCODE)
        bytecodeVer = XDR_BYTECODE_VERSION;

    if (!xdr->codeUint32(&bytecodeVer))
        return false;

    if (mode == XDR_DECODE && bytecodeVer != XDR_BYTECODE_VERSION) {
        /* We do not provide binary compatibility with older scripts. */
        JS_ReportErrorNumber(xdr->cx(), js_GetErrorMessage, NULL, JSMSG_BAD_SCRIPT_MAGIC);
        return false;
    }

    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeFunction(JSObject **objp)
{
    if (mode == XDR_DECODE)
        *objp = NULL;

    if (!VersionCheck(this))
        return false;

    return XDRInterpretedFunction(this, NullPtr(), NullPtr(), objp);
}

template<XDRMode mode>
bool
XDRState<mode>::codeScript(JSScript **scriptp)
{
    JSScript *script;
    if (mode == XDR_DECODE) {
        script = NULL;
        *scriptp = NULL;
    } else {
        script = *scriptp;
    }

    if (!VersionCheck(this))
        return false;

    if (!XDRScript(this, NullPtr(), NullPtr(), NullPtr(), &script))
        return false;

    if (mode == XDR_DECODE) {
        JS_ASSERT(!script->compileAndGo);
        js_CallNewScriptHook(cx(), script, NULL);
        Debugger::onNewScript(cx(), script, NULL);
        *scriptp = script;
    }

    return true;
}

XDRDecoder::XDRDecoder(JSContext *cx, const void *data, uint32_t length,
                       JSPrincipals *principals, JSPrincipals *originPrincipals)
  : XDRState<XDR_DECODE>(cx)
{
    buf.setData(data, length);
    this->principals = principals;
    this->originPrincipals = JSScript::normalizeOriginPrincipals(principals, originPrincipals);
}

template class XDRState<XDR_ENCODE>;
template class XDRState<XDR_DECODE>;

} /* namespace js */

