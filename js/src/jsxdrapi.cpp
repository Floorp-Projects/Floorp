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

#if JS_HAS_XDR

#include <string.h>
#include "jstypes.h"
#include "jsutil.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsnum.h"
#include "jsscript.h"           /* js_XDRScript */
#include "jsstr.h"
#include "jsxdrapi.h"
#include "vm/Debugger.h"

#include "jsobjinlines.h"

using namespace mozilla;
using namespace js;

#ifdef DEBUG
#define DBG(x) x
#else
#define DBG(x) ((void)0)
#endif

typedef struct JSXDRMemState {
    JSXDRState  state;
    char        *base;
    uint32_t    count;
    uint32_t    limit;
} JSXDRMemState;

#define MEM_BLOCK       8192
#define MEM_PRIV(xdr)   ((JSXDRMemState *)(xdr))

#define MEM_BASE(xdr)   (MEM_PRIV(xdr)->base)
#define MEM_COUNT(xdr)  (MEM_PRIV(xdr)->count)
#define MEM_LIMIT(xdr)  (MEM_PRIV(xdr)->limit)

#define MEM_LEFT(xdr, bytes)                                                  \
    JS_BEGIN_MACRO                                                            \
        if ((xdr)->mode == JSXDR_DECODE &&                                    \
            MEM_COUNT(xdr) + bytes > MEM_LIMIT(xdr)) {                        \
            JS_ReportErrorNumber((xdr)->cx, js_GetErrorMessage, NULL,         \
                                 JSMSG_END_OF_DATA);                          \
            return 0;                                                         \
        }                                                                     \
    JS_END_MACRO

#define MEM_NEED(xdr, bytes)                                                  \
    JS_BEGIN_MACRO                                                            \
        if ((xdr)->mode == JSXDR_ENCODE) {                                    \
            if (MEM_LIMIT(xdr) &&                                             \
                MEM_COUNT(xdr) + bytes > MEM_LIMIT(xdr)) {                    \
                uint32_t limit_ = JS_ROUNDUP(MEM_COUNT(xdr) + bytes, MEM_BLOCK);\
                void *data_ = (xdr)->cx->realloc_(MEM_BASE(xdr), limit_);      \
                if (!data_)                                                   \
                    return 0;                                                 \
                MEM_BASE(xdr) = (char *) data_;                               \
                MEM_LIMIT(xdr) = limit_;                                      \
            }                                                                 \
        } else {                                                              \
            MEM_LEFT(xdr, bytes);                                             \
        }                                                                     \
    JS_END_MACRO

#define MEM_DATA(xdr)        ((void *)(MEM_BASE(xdr) + MEM_COUNT(xdr)))
#define MEM_INCR(xdr,bytes)  (MEM_COUNT(xdr) += (bytes))

static JSBool
mem_get32(JSXDRState *xdr, uint32_t *lp)
{
    MEM_LEFT(xdr, 4);
    *lp = *(uint32_t *)MEM_DATA(xdr);
    MEM_INCR(xdr, 4);
    return JS_TRUE;
}

static JSBool
mem_set32(JSXDRState *xdr, uint32_t *lp)
{
    MEM_NEED(xdr, 4);
    *(uint32_t *)MEM_DATA(xdr) = *lp;
    MEM_INCR(xdr, 4);
    return JS_TRUE;
}

static JSBool
mem_getbytes(JSXDRState *xdr, char *bytes, uint32_t len)
{
    MEM_LEFT(xdr, len);
    js_memcpy(bytes, MEM_DATA(xdr), len);
    MEM_INCR(xdr, len);
    return JS_TRUE;
}

static JSBool
mem_setbytes(JSXDRState *xdr, char *bytes, uint32_t len)
{
    MEM_NEED(xdr, len);
    js_memcpy(MEM_DATA(xdr), bytes, len);
    MEM_INCR(xdr, len);
    return JS_TRUE;
}

static void *
mem_raw(JSXDRState *xdr, uint32_t len)
{
    void *data;
    if (xdr->mode == JSXDR_ENCODE) {
        MEM_NEED(xdr, len);
    } else if (xdr->mode == JSXDR_DECODE) {
        MEM_LEFT(xdr, len);
    }
    data = MEM_DATA(xdr);
    MEM_INCR(xdr, len);
    return data;
}

static JSBool
mem_seek(JSXDRState *xdr, int32_t offset, JSXDRWhence whence)
{
    switch (whence) {
      case JSXDR_SEEK_CUR:
        if ((int32_t)MEM_COUNT(xdr) + offset < 0) {
            JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL,
                                 JSMSG_SEEK_BEYOND_START);
            return JS_FALSE;
        }
        if (offset > 0)
            MEM_NEED(xdr, offset);
        MEM_COUNT(xdr) += offset;
        return JS_TRUE;
      case JSXDR_SEEK_SET:
        if (offset < 0) {
            JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL,
                                 JSMSG_SEEK_BEYOND_START);
            return JS_FALSE;
        }
        if (xdr->mode == JSXDR_ENCODE) {
            if ((uint32_t)offset > MEM_COUNT(xdr))
                MEM_NEED(xdr, offset - MEM_COUNT(xdr));
            MEM_COUNT(xdr) = offset;
        } else {
            if ((uint32_t)offset > MEM_LIMIT(xdr)) {
                JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL,
                                     JSMSG_SEEK_BEYOND_END);
                return JS_FALSE;
            }
            MEM_COUNT(xdr) = offset;
        }
        return JS_TRUE;
      case JSXDR_SEEK_END:
        if (offset >= 0 ||
            xdr->mode == JSXDR_ENCODE ||
            (int32_t)MEM_LIMIT(xdr) + offset < 0) {
            JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL,
                                 JSMSG_END_SEEK);
            return JS_FALSE;
        }
        MEM_COUNT(xdr) = MEM_LIMIT(xdr) + offset;
        return JS_TRUE;
      default: {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%d", whence);
        JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL,
                             JSMSG_WHITHER_WHENCE, numBuf);
        return JS_FALSE;
      }
    }
}

static uint32_t
mem_tell(JSXDRState *xdr)
{
    return MEM_COUNT(xdr);
}

static void
mem_finalize(JSXDRState *xdr)
{
    xdr->cx->free_(MEM_BASE(xdr));
}

static JSXDROps xdrmem_ops = {
    mem_get32,      mem_set32,      mem_getbytes,   mem_setbytes,
    mem_raw,        mem_seek,       mem_tell,       mem_finalize
};

JS_PUBLIC_API(void)
JS_XDRInitBase(JSXDRState *xdr, JSXDRMode mode, JSContext *cx)
{
    xdr->mode = mode;
    xdr->cx = cx;
    xdr->userdata = NULL;
    xdr->sharedFilename = NULL;
    xdr->principals = NULL;
    xdr->originPrincipals = NULL;
}

JS_PUBLIC_API(JSXDRState *)
JS_XDRNewMem(JSContext *cx, JSXDRMode mode)
{
    JSXDRState *xdr = (JSXDRState *) cx->malloc_(sizeof(JSXDRMemState));
    if (!xdr)
        return NULL;
    JS_XDRInitBase(xdr, mode, cx);
    if (mode == JSXDR_ENCODE) {
        if (!(MEM_BASE(xdr) = (char *) cx->malloc_(MEM_BLOCK))) {
            cx->free_(xdr);
            return NULL;
        }
    } else {
        /* XXXbe ok, so better not deref MEM_BASE(xdr) if not ENCODE */
        MEM_BASE(xdr) = NULL;
    }
    xdr->ops = &xdrmem_ops;
    MEM_COUNT(xdr) = 0;
    MEM_LIMIT(xdr) = MEM_BLOCK;
    return xdr;
}

JS_PUBLIC_API(void *)
JS_XDRMemGetData(JSXDRState *xdr, uint32_t *lp)
{
    if (xdr->ops != &xdrmem_ops)
        return NULL;
    *lp = MEM_COUNT(xdr);
    return MEM_BASE(xdr);
}

JS_PUBLIC_API(void)
JS_XDRMemSetData(JSXDRState *xdr, void *data, uint32_t len)
{
    if (xdr->ops != &xdrmem_ops)
        return;
    MEM_LIMIT(xdr) = len;
    MEM_BASE(xdr) = (char *) data;
    MEM_COUNT(xdr) = 0;
}

JS_PUBLIC_API(uint32_t)
JS_XDRMemDataLeft(JSXDRState *xdr)
{
    if (xdr->ops != &xdrmem_ops)
        return 0;
    return MEM_LIMIT(xdr) - MEM_COUNT(xdr);
}

JS_PUBLIC_API(void)
JS_XDRMemResetData(JSXDRState *xdr)
{
    if (xdr->ops != &xdrmem_ops)
        return;
    MEM_COUNT(xdr) = 0;
}

JS_PUBLIC_API(void)
JS_XDRDestroy(JSXDRState *xdr)
{
    JSContext *cx = xdr->cx;
    xdr->ops->finalize(xdr);
    cx->free_(xdr);
}

JS_PUBLIC_API(JSBool)
JS_XDRUint8(JSXDRState *xdr, uint8_t *b)
{
    uint32_t l = *b;
    if (!JS_XDRUint32(xdr, &l))
        return JS_FALSE;
    *b = (uint8_t) l;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_XDRUint16(JSXDRState *xdr, uint16_t *s)
{
    uint32_t l = *s;
    if (!JS_XDRUint32(xdr, &l))
        return JS_FALSE;
    *s = (uint16_t) l;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_XDRUint32(JSXDRState *xdr, uint32_t *lp)
{
    JSBool ok = JS_TRUE;
    if (xdr->mode == JSXDR_ENCODE) {
        uint32_t xl = JSXDR_SWAB32(*lp);
        ok = xdr->ops->set32(xdr, &xl);
    } else if (xdr->mode == JSXDR_DECODE) {
        ok = xdr->ops->get32(xdr, lp);
        *lp = JSXDR_SWAB32(*lp);
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_XDRBytes(JSXDRState *xdr, char *bytes, uint32_t len)
{
    uint32_t padlen;
    static char padbuf[JSXDR_ALIGN-1];

    if (xdr->mode == JSXDR_ENCODE) {
        if (!xdr->ops->setbytes(xdr, bytes, len))
            return JS_FALSE;
    } else {
        if (!xdr->ops->getbytes(xdr, bytes, len))
            return JS_FALSE;
    }
    len = xdr->ops->tell(xdr);
    if (len % JSXDR_ALIGN) {
        padlen = JSXDR_ALIGN - (len % JSXDR_ALIGN);
        if (xdr->mode == JSXDR_ENCODE) {
            if (!xdr->ops->setbytes(xdr, padbuf, padlen))
                return JS_FALSE;
        } else {
            if (!xdr->ops->seek(xdr, padlen, JSXDR_SEEK_CUR))
                return JS_FALSE;
        }
    }
    return JS_TRUE;
}

/**
 * Convert between a C string and the XDR representation:
 * leading 32-bit count, then counted vector of chars,
 * then possibly \0 padding to multiple of 4.
 */
JS_PUBLIC_API(JSBool)
JS_XDRCString(JSXDRState *xdr, char **sp)
{
    uint32_t len;

    if (xdr->mode == JSXDR_ENCODE)
        len = strlen(*sp);
    JS_XDRUint32(xdr, &len);
    if (xdr->mode == JSXDR_DECODE) {
        if (!(*sp = (char *) xdr->cx->malloc_(len + 1)))
            return JS_FALSE;
    }
    if (!JS_XDRBytes(xdr, *sp, len)) {
        if (xdr->mode == JSXDR_DECODE)
            xdr->cx->free_(*sp);
        return JS_FALSE;
    }
    if (xdr->mode == JSXDR_DECODE) {
        (*sp)[len] = '\0';
    }
    return JS_TRUE;
}

static JSBool
XDRChars(JSXDRState *xdr, jschar *chars, uint32_t nchars)
{
    uint32_t i, padlen, nbytes;
    jschar *raw;

    nbytes = nchars * sizeof(jschar);
    padlen = nbytes % JSXDR_ALIGN;
    if (padlen) {
        padlen = JSXDR_ALIGN - padlen;
        nbytes += padlen;
    }
    if (!(raw = (jschar *) xdr->ops->raw(xdr, nbytes)))
        return JS_FALSE;
    if (xdr->mode == JSXDR_ENCODE) {
        for (i = 0; i != nchars; i++)
            raw[i] = JSXDR_SWAB16(chars[i]);
        if (padlen)
            memset((char *)raw + nbytes - padlen, 0, padlen);
    } else if (xdr->mode == JSXDR_DECODE) {
        for (i = 0; i != nchars; i++)
            chars[i] = JSXDR_SWAB16(raw[i]);
    }
    return JS_TRUE;
}

/*
 * Convert between a JS (Unicode) string and the XDR representation.
 */
JS_PUBLIC_API(JSBool)
JS_XDRString(JSXDRState *xdr, JSString **strp)
{
    uint32_t nchars;
    jschar *chars;

    if (xdr->mode == JSXDR_ENCODE)
        nchars = (*strp)->length();
    if (!JS_XDRUint32(xdr, &nchars))
        return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE)
        chars = (jschar *) xdr->cx->malloc_((nchars + 1) * sizeof(jschar));
    else
        chars = const_cast<jschar *>((*strp)->getChars(xdr->cx));
    if (!chars)
        return JS_FALSE;

    if (!XDRChars(xdr, chars, nchars))
        goto bad;
    if (xdr->mode == JSXDR_DECODE) {
        chars[nchars] = 0;
        *strp = JS_NewUCString(xdr->cx, chars, nchars);
        if (!*strp)
            goto bad;
    }
    return JS_TRUE;

bad:
    if (xdr->mode == JSXDR_DECODE)
        xdr->cx->free_(chars);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_XDRStringOrNull(JSXDRState *xdr, JSString **strp)
{
    uint32_t null = (*strp == NULL);
    if (!JS_XDRUint32(xdr, &null))
        return JS_FALSE;
    if (null) {
        *strp = NULL;
        return JS_TRUE;
    }
    return JS_XDRString(xdr, strp);
}

JS_PUBLIC_API(JSBool)
JS_XDRDouble(JSXDRState *xdr, double *dp)
{
    jsdpun u;

    u.d = (xdr->mode == JSXDR_ENCODE) ? *dp : 0.0;
    if (!JS_XDRUint32(xdr, &u.s.lo) || !JS_XDRUint32(xdr, &u.s.hi))
        return false;
    if (xdr->mode == JSXDR_DECODE)
        *dp = u.d;
    return true;
}

extern JSBool
js_XDRAtom(JSXDRState *xdr, JSAtom **atomp)
{
    JSString *str;
    uint32_t nchars;
    JSAtom *atom;
    JSContext *cx;
    jschar *chars;
    jschar stackChars[256];

    if (xdr->mode == JSXDR_ENCODE) {
        str = *atomp;
        return JS_XDRString(xdr, &str);
    }

    /*
     * Inline JS_XDRString when decoding to avoid JSString allocation
     * for already existing atoms. See bug 321985.
     */
    if (!JS_XDRUint32(xdr, &nchars))
        return JS_FALSE;
    atom = NULL;
    cx = xdr->cx;
    if (nchars <= ArrayLength(stackChars)) {
        chars = stackChars;
    } else {
        /*
         * This is very uncommon. Don't use the tempLifoAlloc arena for this as
         * most allocations here will be bigger than tempLifoAlloc's default
         * chunk size.
         */
        chars = (jschar *) cx->malloc_(nchars * sizeof(jschar));
        if (!chars)
            return JS_FALSE;
    }

    if (XDRChars(xdr, chars, nchars))
        atom = js_AtomizeChars(cx, chars, nchars);
    if (chars != stackChars)
        cx->free_(chars);

    if (!atom)
        return JS_FALSE;
    *atomp = atom;
    return JS_TRUE;
}

static bool
XDRPrincipals(JSXDRState *xdr)
{
    const uint8_t HAS_PRINCIPALS   = 1;
    const uint8_t HAS_ORIGIN       = 2;

    uint8_t flags = 0;
    if (xdr->mode == JSXDR_ENCODE) {
        if (xdr->principals)
            flags |= HAS_PRINCIPALS;

        /*
         * For the common case when principals == originPrincipals we want to
         * avoid serializing the same principal twice. As originPrincipals are
         * normalized and principals imply originPrincipals we simply set
         * HAS_ORIGIN only if originPrincipals is set and different from
         * principals. During decoding we re-normalize originPrincipals.
         */
        JS_ASSERT_IF(xdr->principals, xdr->originPrincipals);
        if (xdr->originPrincipals && xdr->originPrincipals != xdr->principals)
            flags |= HAS_ORIGIN;
    }

    if (!JS_XDRUint8(xdr, &flags))
        return false;

    if (flags & (HAS_PRINCIPALS | HAS_ORIGIN)) {
        const JSSecurityCallbacks *scb = JS_GetSecurityCallbacks(xdr->cx->runtime);
        if (xdr->mode == JSXDR_DECODE) {
            if (!scb || !scb->principalsTranscoder) {
                JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL,
                                     JSMSG_CANT_DECODE_PRINCIPALS);
                return false;
            }
        } else {
            JS_ASSERT(scb);
            JS_ASSERT(scb->principalsTranscoder);
        }

        if (flags & HAS_PRINCIPALS) {
            if (!scb->principalsTranscoder(xdr, &xdr->principals))
                return false;
        }

        if (flags & HAS_ORIGIN) {
            if (!scb->principalsTranscoder(xdr, &xdr->originPrincipals))
                return false;
        } else if (xdr->mode == JSXDR_DECODE && xdr->principals) {
            xdr->originPrincipals = xdr->principals;
            JS_HoldPrincipals(xdr->principals);
        }
    }

    return true;
}

namespace {

struct AutoDropXDRPrincipals {
    JSXDRState *const xdr;

    AutoDropXDRPrincipals(JSXDRState *xdr)
      : xdr(xdr) { }

    ~AutoDropXDRPrincipals() {
        if (xdr->mode == JSXDR_DECODE) {
            if (xdr->principals)
                JS_DropPrincipals(xdr->cx->runtime, xdr->principals);
            if (xdr->originPrincipals)
                JS_DropPrincipals(xdr->cx->runtime, xdr->originPrincipals);
        }
        xdr->principals = NULL;
        xdr->originPrincipals = NULL;
    }
};

} /* namespace anonymous */

JS_PUBLIC_API(JSBool)
JS_XDRFunctionObject(JSXDRState *xdr, JSObject **objp)
{
    AutoDropXDRPrincipals drop(xdr);
    if (xdr->mode == JSXDR_ENCODE) {
        JSScript *script = (*objp)->toFunction()->script();
        xdr->principals = script->principals;
        xdr->originPrincipals = script->originPrincipals;
    }

    return XDRPrincipals(xdr) && XDRFunctionObject(xdr, objp);
}

JS_PUBLIC_API(JSBool)
JS_XDRScript(JSXDRState *xdr, JSScript **scriptp)
{
    JSScript *script;
    uint32_t magic;
    uint32_t bytecodeVer;
    if (xdr->mode == JSXDR_DECODE) {
        script = NULL;
        *scriptp = NULL;
    } else {
        script = *scriptp;
        magic = JSXDR_MAGIC_SCRIPT_CURRENT;
        bytecodeVer = JSXDR_BYTECODE_VERSION;
    }

    if (!JS_XDRUint32(xdr, &magic))
        return false;
    if (!JS_XDRUint32(xdr, &bytecodeVer))
        return false;

    if (magic != JSXDR_MAGIC_SCRIPT_CURRENT ||
        bytecodeVer != JSXDR_BYTECODE_VERSION) {
        /* We do not provide binary compatibility with older scripts. */
        JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL, JSMSG_BAD_SCRIPT_MAGIC);
        return false;
    }

    {
        AutoDropXDRPrincipals drop(xdr);
        if (xdr->mode == JSXDR_ENCODE) {
            xdr->principals = script->principals;
            xdr->originPrincipals = script->originPrincipals;
        }
        
        if (!XDRPrincipals(xdr) || !XDRScript(xdr, &script))
            return false;
    }

    if (xdr->mode == JSXDR_DECODE) {
        JS_ASSERT(!script->compileAndGo);
        script->globalObject = GetCurrentGlobal(xdr->cx);
        js_CallNewScriptHook(xdr->cx, script, NULL);
        Debugger::onNewScript(xdr->cx, script, NULL);
        *scriptp = script;
    }

    return true;
}

#endif /* JS_HAS_XDR */
