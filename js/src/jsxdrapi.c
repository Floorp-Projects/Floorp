/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */
#include "jsstddef.h"

#include <string.h>
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"              /* js_XDRObject */
#include "jsscript.h"           /* js_XDRScript */
#include "jsstr.h"
#include "jsxdrapi.h"

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
                void *data_ = JS_realloc((xdr)->cx, MEM_BASE(xdr), limit_);   \
                if (!data_)                                                   \
                    return 0;                                                 \
                MEM_BASE(xdr) = data_;                                        \
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
mem_getbytes(JSXDRState *xdr, char **bytesp, uint32 len)
{
    MEM_LEFT(xdr, len);
    memcpy(*bytesp, MEM_DATA(xdr), len);
    MEM_INCR(xdr, len);
    return JS_TRUE;
}

static JSBool
mem_setbytes(JSXDRState *xdr, char **bytesp, uint32 len)
{
    MEM_NEED(xdr, len);
    memcpy(MEM_DATA(xdr), *bytesp, len);
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
    JS_free(xdr->cx, MEM_BASE(xdr));
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
}

JS_PUBLIC_API(JSXDRState *)
JS_XDRNewMem(JSContext *cx, JSXDRMode mode)
{
    JSXDRState *xdr = (JSXDRState *) JS_malloc(cx, sizeof(JSXDRMemState));
    if (!xdr)
        return NULL;
    JS_XDRInitBase(xdr, mode, cx);
    if (mode == JSXDR_ENCODE) {
        if (!(MEM_BASE(xdr) = JS_malloc(cx, MEM_BLOCK))) {
            JS_free(cx, xdr);
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
    MEM_BASE(xdr) = data;
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
        JS_free(cx, xdr->registry);
        if (xdr->reghash)
            JS_DHashTableDestroy(xdr->reghash);
    }
    JS_free(cx, xdr);
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
JS_XDRBytes(JSXDRState *xdr, char **bytesp, uint32 len)
{
    uint32 padlen;
    char *padbp;
    static char padbuf[JSXDR_ALIGN-1];

    if (xdr->mode == JSXDR_ENCODE) {
        if (!xdr->ops->setbytes(xdr, bytesp, len))
            return JS_FALSE;
    } else {
        if (!xdr->ops->getbytes(xdr, bytesp, len))
            return JS_FALSE;
    }
    len = xdr->ops->tell(xdr);
    if (len % JSXDR_ALIGN) {
        padlen = JSXDR_ALIGN - (len % JSXDR_ALIGN);
        if (xdr->mode == JSXDR_ENCODE) {
            padbp = padbuf;
            if (!xdr->ops->setbytes(xdr, &padbp, padlen))
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
        if (!(*sp = (char *) JS_malloc(xdr->cx, len + 1)))
            return JS_FALSE;
    }
    if (!JS_XDRBytes(xdr, sp, len)) {
        if (xdr->mode == JSXDR_DECODE)
            JS_free(xdr->cx, *sp);
        return JS_FALSE;
    }
    if (xdr->mode == JSXDR_DECODE) {
        (*sp)[len] = '\0';
    } else if (xdr->mode == JSXDR_FREE) {
        JS_free(xdr->cx, *sp);
        *sp = NULL;
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

/*
 * Convert between a JS (Unicode) string and the XDR representation.
 */
JS_PUBLIC_API(JSBool)
JS_XDRString(JSXDRState *xdr, JSString **strp)
{
    uint32 i, len, padlen, nbytes;
    jschar *chars = NULL, *raw;

    if (xdr->mode == JSXDR_ENCODE)
        len = JSSTRING_LENGTH(*strp);
    if (!JS_XDRUint32(xdr, &len))
        return JS_FALSE;
    nbytes = len * sizeof(jschar);

    if (xdr->mode == JSXDR_DECODE) {
        if (!(chars = (jschar *) JS_malloc(xdr->cx, nbytes + sizeof(jschar))))
            return JS_FALSE;
    } else {
        chars = JSSTRING_CHARS(*strp);
    }

    padlen = nbytes % JSXDR_ALIGN;
    if (padlen) {
        padlen = JSXDR_ALIGN - padlen;
        nbytes += padlen;
    }
    if (!(raw = (jschar *) xdr->ops->raw(xdr, nbytes)))
        goto bad;
    if (xdr->mode == JSXDR_ENCODE) {
        for (i = 0; i < len; i++)
            raw[i] = JSXDR_SWAB16(chars[i]);
        if (padlen)
            memset((char *)raw + nbytes - padlen, 0, padlen);
    } else if (xdr->mode == JSXDR_DECODE) {
        for (i = 0; i < len; i++)
            chars[i] = JSXDR_SWAB16(raw[i]);
        chars[len] = 0;

        if (!(*strp = JS_NewUCString(xdr->cx, chars, len)))
            goto bad;
    }
    return JS_TRUE;

bad:
    if (xdr->mode == JSXDR_DECODE)
        JS_free(xdr->cx, chars);
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

JS_PUBLIC_API(JSBool)
JS_XDRDouble(JSXDRState *xdr, jsdouble **dp)
{
    jsdouble d;
    if (xdr->mode == JSXDR_ENCODE)
        d = **dp;
#if IS_BIG_ENDIAN
    if (!JS_XDRUint32(xdr, (uint32 *)&d + 1) ||
        !JS_XDRUint32(xdr, (uint32 *)&d))
#else
    if (!JS_XDRUint32(xdr, (uint32 *)&d) ||
        !JS_XDRUint32(xdr, (uint32 *)&d + 1))
#endif
        return JS_FALSE;
    if (xdr->mode == JSXDR_DECODE) {
        *dp = JS_NewDouble(xdr->cx, d);
        if (!*dp)
            return JS_FALSE;
    }
    return JS_TRUE;
}

/* These are magic pseudo-tags: see jsapi.h, near the top, for real tags. */
#define JSVAL_XDRNULL   0x8
#define JSVAL_XDRVOID   0xA

JS_PUBLIC_API(JSBool)
JS_XDRValue(JSXDRState *xdr, jsval *vp)
{
    uint32 type;
    
    if (xdr->mode == JSXDR_ENCODE) {
        if (JSVAL_IS_NULL(*vp))
            type = JSVAL_XDRNULL;
        else if (JSVAL_IS_VOID(*vp))
            type = JSVAL_XDRVOID;
        else
            type = JSVAL_TAG(*vp);
    }
    if (!JS_XDRUint32(xdr, &type))
        return JS_FALSE;

    switch (type) {
      case JSVAL_XDRNULL:
        *vp = JSVAL_NULL;
        break;
      case JSVAL_XDRVOID:
        *vp = JSVAL_VOID;
        break;
      case JSVAL_STRING: {
        JSString *str;
        if (xdr->mode == JSXDR_ENCODE)
            str = JSVAL_TO_STRING(*vp);
        if (!JS_XDRString(xdr, &str))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = STRING_TO_JSVAL(str);
        break;
      }
      case JSVAL_DOUBLE: {
        jsdouble *dp;
        if (xdr->mode == JSXDR_ENCODE)
            dp = JSVAL_TO_DOUBLE(*vp);
        if (!JS_XDRDouble(xdr, &dp))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = DOUBLE_TO_JSVAL(dp);
        break;
      }
      case JSVAL_OBJECT: {
        JSObject *obj;
        if (xdr->mode == JSXDR_ENCODE)
            obj = JSVAL_TO_OBJECT(*vp);
        if (!js_XDRObject(xdr, &obj))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = OBJECT_TO_JSVAL(obj);
        break;
      }
      case JSVAL_BOOLEAN: {
        uint32 b;
        if (xdr->mode == JSXDR_ENCODE)
            b = (uint32) JSVAL_TO_BOOLEAN(*vp);
        if (!JS_XDRUint32(xdr, &b))
            return JS_FALSE;
        if (xdr->mode == JSXDR_DECODE)
            *vp = BOOLEAN_TO_JSVAL((JSBool) b);
        break;
      }
      default: {
        uint32 i;

        JS_ASSERT(type & JSVAL_INT);
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
JS_XDRScript(JSXDRState *xdr, JSScript **scriptp)
{
    JSBool hasMagic;

    if (!js_XDRScript(xdr, scriptp, &hasMagic))
        return JS_FALSE;
    if (!hasMagic) {
        JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_SCRIPT_MAGIC);
        return JS_FALSE;
    }
    return JS_TRUE;
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
            JS_realloc(xdr->cx, xdr->registry, maxclasses * sizeof(JSClass *));
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
            JS_DHashTableOperate(xdr->reghash, clasp->name, JS_DHASH_ADD);
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
            xdr->reghash = JS_NewDHashTable(JS_DHashGetStubOps(), NULL,
                                            sizeof(JSRegHashEntry),
                                            numclasses);
            if (xdr->reghash) {
                for (i = 0; i < numclasses; i++) {
                    JSClass *clasp = xdr->registry[i];
                    entry = (JSRegHashEntry *)
                        JS_DHashTableOperate(xdr->reghash, clasp->name,
                                             JS_DHASH_ADD);
                    entry->name = clasp->name;
                    entry->index = i;
                }
            }
        }

        /* If we managed to create reghash, use it for O(1) Find. */
        if (xdr->reghash) {
            entry = (JSRegHashEntry *)
                JS_DHashTableOperate(xdr->reghash, name, JS_DHASH_LOOKUP);
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
