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

#include "jsversion.h"

#if JS_HAS_XDR

#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsnum.h"
#include "jsobj.h"              /* js_XDRObject */
#include "jsscript.h"           /* js_XDRScript */
#include "jsstr.h"
#include "jsxdrapi.h"
#include "vm/Debugger.h"

#include "jsobjinlines.h"

using namespace js;

#ifdef DEBUG
#define DBG(x) x
#else
#define DBG(x) ((void)0)
#endif

typedef struct JSXDRMemState {
    JSXDRState  state;
    char        *base;
    uint32      count;
    uint32      limit;
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
                uint32 limit_ = JS_ROUNDUP(MEM_COUNT(xdr) + bytes, MEM_BLOCK);\
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
mem_get32(JSXDRState *xdr, uint32 *lp)
{
    MEM_LEFT(xdr, 4);
    *lp = *(uint32 *)MEM_DATA(xdr);
    MEM_INCR(xdr, 4);
    return JS_TRUE;
}

static JSBool
mem_set32(JSXDRState *xdr, uint32 *lp)
{
    MEM_NEED(xdr, 4);
    *(uint32 *)MEM_DATA(xdr) = *lp;
    MEM_INCR(xdr, 4);
    return JS_TRUE;
}

static JSBool
mem_getbytes(JSXDRState *xdr, char *bytes, uint32 len)
{
    MEM_LEFT(xdr, len);
    memcpy(bytes, MEM_DATA(xdr), len);
    MEM_INCR(xdr, len);
    return JS_TRUE;
}

static JSBool
mem_setbytes(JSXDRState *xdr, char *bytes, uint32 len)
{
    MEM_NEED(xdr, len);
    memcpy(MEM_DATA(xdr), bytes, len);
    MEM_INCR(xdr, len);
    return JS_TRUE;
}

static void *
mem_raw(JSXDRState *xdr, uint32 len)
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
mem_seek(JSXDRState *xdr, int32 offset, JSXDRWhence whence)
{
    switch (whence) {
      case JSXDR_SEEK_CUR:
        if ((int32)MEM_COUNT(xdr) + offset < 0) {
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
            if ((uint32)offset > MEM_COUNT(xdr))
                MEM_NEED(xdr, offset - MEM_COUNT(xdr));
            MEM_COUNT(xdr) = offset;
        } else {
            if ((uint32)offset > MEM_LIMIT(xdr)) {
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
            (int32)MEM_LIMIT(xdr) + offset < 0) {
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

static uint32
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
    xdr->registry = NULL;
    xdr->numclasses = xdr->maxclasses = 0;
    xdr->reghash = NULL;
    xdr->userdata = NULL;
    xdr->script = NULL;
    xdr->state = NULL;
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
JS_XDRMemGetData(JSXDRState *xdr, uint32 *lp)
{
    if (xdr->ops != &xdrmem_ops)
        return NULL;
    *lp = MEM_COUNT(xdr);
    return MEM_BASE(xdr);
}

JS_PUBLIC_API(void)
JS_XDRMemSetData(JSXDRState *xdr, void *data, uint32 len)
{
    if (xdr->ops != &xdrmem_ops)
        return;
    MEM_LIMIT(xdr) = len;
    MEM_BASE(xdr) = (char *) data;
    MEM_COUNT(xdr) = 0;
}

JS_PUBLIC_API(uint32)
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
    if (xdr->registry) {
        cx->free_(xdr->registry);
        if (xdr->reghash)
            JS_DHashTableDestroy((JSDHashTable *) xdr->reghash);
    }
    cx->free_(xdr);
}

JS_PUBLIC_API(JSBool)
JS_XDRUint8(JSXDRState *xdr, uint8 *b)
{
    uint32 l = *b;
    if (!JS_XDRUint32(xdr, &l))
        return JS_FALSE;
    *b = (uint8) l;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_XDRUint16(JSXDRState *xdr, uint16 *s)
{
    uint32 l = *s;
    if (!JS_XDRUint32(xdr, &l))
        return JS_FALSE;
    *s = (uint16) l;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_XDRUint32(JSXDRState *xdr, uint32 *lp)
{
    JSBool ok = JS_TRUE;
    if (xdr->mode == JSXDR_ENCODE) {
        uint32 xl = JSXDR_SWAB32(*lp);
        ok = xdr->ops->set32(xdr, &xl);
    } else if (xdr->mode == JSXDR_DECODE) {
        ok = xdr->ops->get32(xdr, lp);
        *lp = JSXDR_SWAB32(*lp);
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_XDRBytes(JSXDRState *xdr, char *bytes, uint32 len)
{
    uint32 padlen;
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
    uint32 len;

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

JS_PUBLIC_API(JSBool)
JS_XDRCStringOrNull(JSXDRState *xdr, char **sp)
{
    uint32 null = (*sp == NULL);
    if (!JS_XDRUint32(xdr, &null))
        return JS_FALSE;
    if (null) {
        *sp = NULL;
        return JS_TRUE;
    }
    return JS_XDRCString(xdr, sp);
}

static JSBool
XDRChars(JSXDRState *xdr, jschar *chars, uint32 nchars)
{
    uint32 i, padlen, nbytes;
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
    uint32 nchars;
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
    uint32 null = (*strp == NULL);
    if (!JS_XDRUint32(xdr, &null))
        return JS_FALSE;
    if (null) {
        *strp = NULL;
        return JS_TRUE;
    }
    return JS_XDRString(xdr, strp);
}

static JSBool
XDRDoubleValue(JSXDRState *xdr, jsdouble *dp)
{
    jsdpun u;

    u.d = (xdr->mode == JSXDR_ENCODE) ? *dp : 0.0;
    if (!JS_XDRUint32(xdr, &u.s.lo) || !JS_XDRUint32(xdr, &u.s.hi))
        return JS_FALSE;
    if (xdr->mode == JSXDR_DECODE)
        *dp = u.d;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_XDRDouble(JSXDRState *xdr, jsdouble *dp)
{
    jsdouble d = (xdr->mode == JSXDR_ENCODE) ? *dp : 0.0;
    if (!XDRDoubleValue(xdr, &d))
        return JS_FALSE;
    if (xdr->mode == JSXDR_DECODE)
        *dp = d;
    return JS_TRUE;
}

enum XDRValueTag {
    XDRTAG_OBJECT  = 0,
    XDRTAG_INT     = 1,
    XDRTAG_DOUBLE  = 2,
    XDRTAG_STRING  = 3,
    XDRTAG_SPECIAL = 4,
    XDRTAG_XDRNULL = 5,
    XDRTAG_XDRVOID = 6
};

static XDRValueTag
GetXDRTag(jsval v)
{
    if (JSVAL_IS_NULL(v))
        return XDRTAG_XDRNULL;
    if (JSVAL_IS_VOID(v))
        return XDRTAG_XDRVOID;
    if (JSVAL_IS_OBJECT(v))
        return XDRTAG_OBJECT;
    if (JSVAL_IS_INT(v))
        return XDRTAG_INT;
    if (JSVAL_IS_DOUBLE(v))
        return XDRTAG_DOUBLE;
    if (JSVAL_IS_STRING(v))
        return XDRTAG_STRING;
    JS_ASSERT(JSVAL_IS_BOOLEAN(v));
    return XDRTAG_SPECIAL;
}

static JSBool
XDRValueBody(JSXDRState *xdr, uint32 type, jsval *vp)
{
    switch (type) {
      case XDRTAG_XDRNULL:
        *vp = JSVAL_NULL;
        break;
      case XDRTAG_XDRVOID:
        *vp = JSVAL_VOID;
        break;
      case XDRTAG_STRING: {
        JSString *str;
        if (xdr->mode == JSXDR_ENCODE)
            str = JSVAL_TO_STRING(*vp);
        if (!JS_XDRString(xdr, &str))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = STRING_TO_JSVAL(str);
        break;
      }
      case XDRTAG_DOUBLE: {
        double d = xdr->mode == JSXDR_ENCODE ? JSVAL_TO_DOUBLE(*vp) : 0;
        if (!JS_XDRDouble(xdr, &d))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = DOUBLE_TO_JSVAL(d);
        break;
      }
      case XDRTAG_OBJECT: {
        JSObject *obj;
        if (xdr->mode == JSXDR_ENCODE)
            obj = JSVAL_TO_OBJECT(*vp);
        if (!js_XDRObject(xdr, &obj))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = OBJECT_TO_JSVAL(obj);
        break;
      }
      case XDRTAG_SPECIAL: {
        uint32 b;
        if (xdr->mode == JSXDR_ENCODE)
            b = (uint32) JSVAL_TO_BOOLEAN(*vp);
        if (!JS_XDRUint32(xdr, &b))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = BOOLEAN_TO_JSVAL(!!b);
        break;
      }
      default: {
        uint32 i;

        JS_ASSERT(type == XDRTAG_INT);
        if (xdr->mode == JSXDR_ENCODE)
            i = (uint32) JSVAL_TO_INT(*vp);
        if (!JS_XDRUint32(xdr, &i))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = INT_TO_JSVAL((int32) i);
        break;
      }
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_XDRValue(JSXDRState *xdr, jsval *vp)
{
    uint32 type;

    if (xdr->mode == JSXDR_ENCODE)
        type = GetXDRTag(*vp);
    return JS_XDRUint32(xdr, &type) && XDRValueBody(xdr, type, vp);
}

extern JSBool
js_XDRAtom(JSXDRState *xdr, JSAtom **atomp)
{
    JSString *str;
    uint32 nchars;
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
    if (nchars <= JS_ARRAY_LENGTH(stackChars)) {
        chars = stackChars;
    } else {
        /*
         * This is very uncommon. Don't use the tempPool arena for this as
         * most allocations here will be bigger than tempPool's arenasize.
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

XDRScriptState::XDRScriptState(JSXDRState *x)
    : xdr(x)
    , filename(NULL)
    , filenameSaved(false)
{
    JS_ASSERT(!xdr->state);

    xdr->state = this;
}

XDRScriptState::~XDRScriptState()
{
    xdr->state = NULL;
    if (xdr->mode == JSXDR_DECODE && filename && !filenameSaved)
        xdr->cx->free_((void *)filename);
}

JS_PUBLIC_API(JSBool)
JS_XDRScript(JSXDRState *xdr, JSScript **scriptp)
{
    JS_ASSERT(!xdr->state);

    JSScript *script;
    uint32 magic;
    if (xdr->mode == JSXDR_DECODE) {
        script = NULL;
        *scriptp = NULL;
    } else {
        script = *scriptp;
        magic = JSXDR_MAGIC_SCRIPT_CURRENT;
    }

    if (!JS_XDRUint32(xdr, &magic))
        return false;

    if (magic != JSXDR_MAGIC_SCRIPT_CURRENT) {
        /* We do not provide binary compatibility with older scripts. */
        JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL, JSMSG_BAD_SCRIPT_MAGIC);
        return false;
    }

    XDRScriptState state(xdr);
    if (!xdr->state)
        return false;

    if (xdr->mode == JSXDR_ENCODE)
        state.filename = script->filename;
    if (!JS_XDRCStringOrNull(xdr, (char **) &state.filename))
        return false;

    if (!js_XDRScript(xdr, &script))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        if (!js_NewScriptObject(xdr->cx, script))
            return false;
        js_CallNewScriptHook(xdr->cx, script, NULL);
        Debugger::onNewScript(xdr->cx, script, script->u.object, Debugger::NewHeldScript);
        *scriptp = script;
    }

    return true;
}

#define CLASS_REGISTRY_MIN      8
#define CLASS_INDEX_TO_ID(i)    ((i)+1)
#define CLASS_ID_TO_INDEX(id)   ((id)-1)

typedef struct JSRegHashEntry {
    JSDHashEntryHdr hdr;
    const char      *name;
    uint32          index;
} JSRegHashEntry;

JS_PUBLIC_API(JSBool)
JS_XDRRegisterClass(JSXDRState *xdr, JSClass *clasp, uint32 *idp)
{
    uintN numclasses, maxclasses;
    JSClass **registry;

    numclasses = xdr->numclasses;
    maxclasses = xdr->maxclasses;
    if (numclasses == maxclasses) {
        maxclasses = (maxclasses == 0) ? CLASS_REGISTRY_MIN : maxclasses << 1;
        registry = (JSClass **)
            xdr->cx->realloc_(xdr->registry, maxclasses * sizeof(JSClass *));
        if (!registry)
            return JS_FALSE;
        xdr->registry = registry;
        xdr->maxclasses = maxclasses;
    } else {
        JS_ASSERT(numclasses && numclasses < maxclasses);
        registry = xdr->registry;
    }

    registry[numclasses] = clasp;
    if (xdr->reghash) {
        JSRegHashEntry *entry = (JSRegHashEntry *)
            JS_DHashTableOperate((JSDHashTable *) xdr->reghash,
                                 clasp->name, JS_DHASH_ADD);
        if (!entry) {
            JS_ReportOutOfMemory(xdr->cx);
            return JS_FALSE;
        }
        entry->name = clasp->name;
        entry->index = numclasses;
    }
    *idp = CLASS_INDEX_TO_ID(numclasses);
    xdr->numclasses = ++numclasses;
    return JS_TRUE;
}

JS_PUBLIC_API(uint32)
JS_XDRFindClassIdByName(JSXDRState *xdr, const char *name)
{
    uintN i, numclasses;

    numclasses = xdr->numclasses;
    if (numclasses >= 10) {
        JSRegHashEntry *entry;

        /* Bootstrap reghash from registry on first overpopulated Find. */
        if (!xdr->reghash) {
            xdr->reghash =
                JS_NewDHashTable(JS_DHashGetStubOps(), NULL,
                                 sizeof(JSRegHashEntry),
                                 JS_DHASH_DEFAULT_CAPACITY(numclasses));
            if (xdr->reghash) {
                for (i = 0; i < numclasses; i++) {
                    JSClass *clasp = xdr->registry[i];
                    entry = (JSRegHashEntry *)
                        JS_DHashTableOperate((JSDHashTable *) xdr->reghash,
                                             clasp->name, JS_DHASH_ADD);
                    entry->name = clasp->name;
                    entry->index = i;
                }
            }
        }

        /* If we managed to create reghash, use it for O(1) Find. */
        if (xdr->reghash) {
            entry = (JSRegHashEntry *)
                JS_DHashTableOperate((JSDHashTable *) xdr->reghash,
                                     name, JS_DHASH_LOOKUP);
            if (JS_DHASH_ENTRY_IS_BUSY(&entry->hdr))
                return CLASS_INDEX_TO_ID(entry->index);
        }
    }

    /* Only a few classes, or we couldn't malloc reghash: use linear search. */
    for (i = 0; i < numclasses; i++) {
        if (!strcmp(name, xdr->registry[i]->name))
            return CLASS_INDEX_TO_ID(i);
    }
    return 0;
}

JS_PUBLIC_API(JSClass *)
JS_XDRFindClassById(JSXDRState *xdr, uint32 id)
{
    uintN i = CLASS_ID_TO_INDEX(id);

    if (i >= xdr->numclasses)
        return NULL;
    return xdr->registry[i];
}

#endif /* JS_HAS_XDR */
