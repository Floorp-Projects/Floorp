/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * Convert between a JS (Unicode) string and the XDR representation.
 */
template<XDRMode mode>
bool
XDRState<mode>::codeString(JSString **strp)
{
    uint32_t nchars;
    jschar *chars;

    if (mode == XDR_ENCODE)
        nchars = (*strp)->length();
    if (!codeUint32(&nchars))
        return false;

    if (mode == XDR_DECODE)
        chars = (jschar *) cx()->malloc_((nchars + 1) * sizeof(jschar));
    else
        chars = const_cast<jschar *>((*strp)->getChars(cx()));
    if (!chars)
        return false;

    if (!codeChars(chars, nchars))
        goto bad;
    if (mode == XDR_DECODE) {
        chars[nchars] = 0;
        *strp = JS_NewUCString(cx(), chars, nchars);
        if (!*strp)
            goto bad;
    }
    return true;

bad:
    if (mode == XDR_DECODE)
        Foreground::free_(chars);
    return false;
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
        
    return VersionCheck(this) && XDRInterpretedFunction(this, objp, NULL);
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

    if (!VersionCheck(this) || !XDRScript(this, &script, NULL))
        return false;

    if (mode == XDR_DECODE) {
        JS_ASSERT(!script->compileAndGo);
        script->globalObject = GetCurrentGlobal(cx());
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

