/* -*- Mode: C; tab-width: 8; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <string.h>
#include "prtypes.h"
#include "prlog.h"
#include "jsapi.h"
#include "jsobj.h"		/* js_XDRObject */
#include "jsstr.h"
#include "jsxdrapi.h"

#ifdef DEBUG
#define DBG(x) x
#else
#define DBG(x) ((void)0)
#endif

typedef struct JSXDRMemState {
    JSXDRState  state;
    uint32      count;
    uint32      limit;
} JSXDRMemState;

#define MEM_BLOCK       8192
#define MEM_PRIV(xdr)   ((JSXDRMemState *)(xdr))

#define MEM_COUNT(xdr)  (MEM_PRIV(xdr)->count)
#define MEM_LIMIT(xdr)  (MEM_PRIV(xdr)->limit)

static char unexpected_end_of_data_str[] = "unexpected end of data";

#define MEM_LEFT(xdr, bytes)                                                  \
    PR_BEGIN_MACRO                                                            \
        if ((xdr)->mode == JSXDR_DECODE &&                                    \
	    MEM_COUNT(xdr) + bytes > MEM_LIMIT(xdr)) {                        \
	    JS_ReportError((xdr)->cx, unexpected_end_of_data_str);            \
	    return 0;                                                         \
	}                                                                     \
    PR_END_MACRO

/* XXXbe why does NEED even allow or cope with non-ENCODE mode? */
#define MEM_NEED(xdr, bytes)                                                  \
    PR_BEGIN_MACRO                                                            \
        if ((xdr)->mode == JSXDR_ENCODE) {                                    \
	    if (MEM_LIMIT(xdr) &&                                             \
		MEM_COUNT(xdr) + bytes > MEM_LIMIT(xdr)) {                    \
		void *_data = JS_realloc((xdr)->cx,                           \
					 (xdr)->data,                         \
					 MEM_LIMIT(xdr) + MEM_BLOCK);         \
		if (!_data)                                                   \
		    return 0;                                                 \
		(xdr)->data = _data;                                          \
		MEM_LIMIT(xdr) += MEM_BLOCK;                                  \
	    }                                                                 \
	} else {                                                              \
	    if (MEM_LIMIT(xdr) < MEM_COUNT(xdr) + bytes) {                    \
		JS_ReportError((xdr)->cx, unexpected_end_of_data_str);        \
		return 0;                                                     \
	    }                                                                 \
	}                                                                     \
    PR_END_MACRO

#define MEM_DATA(xdr)        ((void *)((char *)(xdr)->data + MEM_COUNT(xdr)))
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
	    JS_ReportError(xdr->cx, "illegal seek beyond start");
	    return JS_FALSE;
	}
	if (offset > 0)
	    MEM_NEED(xdr, offset);
	MEM_COUNT(xdr) += offset;
	return JS_TRUE;
      case JSXDR_SEEK_SET:
	if (offset < 0) {
	    JS_ReportError(xdr->cx, "illegal seek beyond start");
	    return JS_FALSE;
	}
	if (xdr->mode == JSXDR_ENCODE) {
	    if ((uint32)offset > MEM_COUNT(xdr))
		MEM_NEED(xdr, offset - MEM_COUNT(xdr));
	    MEM_COUNT(xdr) = offset;
	} else {
	    if ((uint32)offset > MEM_LIMIT(xdr)) {
		JS_ReportError(xdr->cx, "illegal seek beyond end");
		return JS_FALSE;
	    }
	    MEM_COUNT(xdr) = offset;
	}
	return JS_TRUE;
      case JSXDR_SEEK_END:
	if (offset >= 0 ||
	    xdr->mode == JSXDR_ENCODE ||
	    (int32)MEM_LIMIT(xdr) + offset < 0) {
	    JS_ReportError(xdr->cx, "illegal end-based seek");
	    return JS_FALSE;
	}
	MEM_COUNT(xdr) = MEM_LIMIT(xdr) + offset;
	return JS_TRUE;
      default:
	JS_ReportError(xdr->cx, "unknown seek whence: %d", whence);
	return JS_FALSE;
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
    JSContext *cx = xdr->cx;
    JS_free(cx, xdr->data);
}

static JSXDROps xdrmem_ops = {
    mem_get32,      mem_set32,      mem_getbytes,   mem_setbytes,
    mem_raw,        mem_seek,       mem_tell,       mem_finalize
};

void
JS_XDRNewBase(JSContext *cx, JSXDRState *xdr, JSXDRMode mode)
{
    xdr->cx = cx;
    xdr->mode = mode;
    xdr->registry = NULL;
    xdr->nclasses = 0;
}

JSXDRState *
JS_XDRNewMem(JSContext *cx, JSXDRMode mode)
{
    JSXDRState *xdr = JS_malloc(cx, sizeof(JSXDRMemState));
    if (!xdr)
	return NULL;
    JS_XDRNewBase(cx, xdr, mode);
    if (mode == JSXDR_ENCODE) {
	if (!(xdr->data = JS_malloc(cx, MEM_BLOCK))) {
	    JS_free(cx, xdr);
	    return NULL;
	}
    } else {
	/* XXXbe ok, so better not deref xdr->data if not ENCODE */
	xdr->data = NULL;
    }
    xdr->ops = &xdrmem_ops;
    MEM_PRIV(xdr)->count = 0;
    MEM_PRIV(xdr)->limit = MEM_BLOCK;
    return xdr;
}

void *
JS_XDRMemGetData(JSXDRState *xdr, uint32 *lp)
{
    if (xdr->ops != &xdrmem_ops)
	return NULL;
    *lp = MEM_PRIV(xdr)->count;
    return xdr->data;
}

void
JS_XDRMemSetData(JSXDRState *xdr, void *data, uint32 len)
{
    if (xdr->ops != &xdrmem_ops)
	return;
    MEM_PRIV(xdr)->limit = len;
    xdr->data = data;
    MEM_PRIV(xdr)->count = 0;
}

JSBool
JS_XDRUint8(JSXDRState *xdr, uint8 *b)
{
    uint32 l = *b;
    if (!JS_XDRUint32(xdr, &l))
	return JS_FALSE;
    *b = (uint8) l & 0xff;
    return JS_TRUE;
}

JSBool
JS_XDRUint16(JSXDRState *xdr, uint16 *s)
{
    uint32 l = *s;
    if (!JS_XDRUint32(xdr, &l))
	return JS_FALSE;
    *s = (uint16) l & 0xffff;
    return JS_TRUE;
}

JSBool
JS_XDRUint32(JSXDRState *xdr, uint32 *lp)
{
    JSBool ok;
    if (xdr->mode == JSXDR_ENCODE) {
	uint32 xl = JSXDR_SWAB32(*lp);
	ok = xdr->ops->set32(xdr, &xl);
    } else if (xdr->mode == JSXDR_DECODE) {
	ok = xdr->ops->get32(xdr, lp);
	*lp = JSXDR_SWAB32(*lp);
    }
    return ok;
}

JSBool
JS_XDRBytes(JSXDRState *xdr, char **bytesp, uint32 len)
{
    if (xdr->mode == JSXDR_ENCODE) {
	if (!xdr->ops->setbytes(xdr, bytesp, len))
	    return JS_FALSE;
    } else {
	if (!xdr->ops->getbytes(xdr, bytesp, len))
	    return JS_FALSE;
    }
    len = xdr->ops->tell(xdr);
    if (len % JSXDR_ALIGN) {
	if (!xdr->ops->seek(xdr, JSXDR_ALIGN - (len % JSXDR_ALIGN),
			    JSXDR_SEEK_CUR)) {
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
JSBool
JS_XDRCString(JSXDRState *xdr, char **sp)
{
    uint32 len;

    if (xdr->mode == JSXDR_ENCODE)
	len = strlen(*sp);
    JS_XDRUint32(xdr, &len);
    if (xdr->mode == JSXDR_DECODE) {
	if (!(*sp = JS_malloc(xdr->cx, len + 1)))
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

JSBool
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
JSBool
JS_XDRString(JSXDRState *xdr, JSString **strp)
{
    uint32 i, len, nbytes;
    jschar *chars, *raw;

    if (xdr->mode == JSXDR_ENCODE)
	len = (*strp)->length;
    if (!JS_XDRUint32(xdr, &len))
	return JS_FALSE;
    nbytes = len * sizeof(jschar);

    if (xdr->mode == JSXDR_ENCODE) {
	chars = (*strp)->chars;
    } else if (xdr->mode == JSXDR_DECODE) {
	if (!(chars = JS_malloc(xdr->cx, nbytes + sizeof(jschar))))
	    return JS_FALSE;
    }

    if (nbytes % JSXDR_ALIGN)
    	nbytes += JSXDR_ALIGN - (nbytes % JSXDR_ALIGN);
    if (!(raw = xdr->ops->raw(xdr, nbytes)))
	goto bad;
    if (xdr->mode == JSXDR_ENCODE) {
	for (i = 0; i < len; i++)
	    raw[i] = JSXDR_SWAB16(chars[i]);
    } else if (xdr->mode == JSXDR_DECODE) {
	for (i = 0; i < len; i++)
	    chars[i] = JSXDR_SWAB16(raw[i]);
	if (!(*strp = JS_NewUCString(xdr->cx, chars, len)))
	    goto bad;
    }
    return JS_TRUE;

bad:
    if (xdr->mode == JSXDR_DECODE)
	JS_free(xdr->cx, chars);
    return JS_FALSE;
}

JSBool
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

JSBool
JS_XDRDouble(JSXDRState *xdr, jsdouble **dp)
{
    jsdouble d;
    if (xdr->mode == JSXDR_ENCODE)
	d = **dp;
#if IS_BIG_ENDIAN
    if (!JS_XDRUint32(xdr, (uint32 *)&d + 1) ||
	!JS_XDRUint32(xdr, (uint32 *)&d))
#else /* !IS_BIG_ENDIAN */
    if (!JS_XDRUint32(xdr, (uint32 *)&d) ||
	!JS_XDRUint32(xdr, (uint32 *)&d + 1))
#endif /* IS_BIG_ENDIAN */
	return JS_FALSE;
    if (xdr->mode == JSXDR_DECODE) {
	*dp = JS_NewDouble(xdr->cx, d);
	if (!*dp)
	    return JS_FALSE;
    }
    return JS_TRUE;
}
    
JSBool
JS_XDRValue(JSXDRState *xdr, jsval *vp)
{
    uint32 type = JSVAL_TAG(*vp);
    if (!JS_XDRUint32(xdr, &type))
	return JS_FALSE;

    switch (type) {
      case JSVAL_STRING: {
	JSString *str = JSVAL_TO_STRING(*vp);
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
        uint32 bool;
	if (xdr->mode == JSXDR_ENCODE)
	    bool = (uint32)JSVAL_TO_BOOLEAN(*vp);
	if (!JS_XDRUint32(xdr, &bool))
	    return JS_FALSE;
	if (xdr->mode == JSXDR_DECODE)
	    *vp = BOOLEAN_TO_JSVAL((JSBool)bool);
	break;
      }
      case JSVAL_VOID:
	if (!JS_XDRUint32(xdr, (uint32 *)vp))
	    return JS_FALSE;
	break;
      default:
	JS_ReportError(xdr->cx, "unknown jsval type %#lx for XDR", type);
	return JS_FALSE;
    }
    return JS_TRUE;
}


void
JS_XDRDestroy(JSXDRState *xdr)
{
    JSContext *cx = xdr->cx;
    xdr->ops->finalize(xdr);
    if (xdr->registry)
	JS_free(cx, xdr->registry);
    JS_free(cx, xdr);
}

#define REGISTRY_CHUNK 4

JSBool
JS_RegisterClass(JSXDRState *xdr, JSClass *clasp, uint32 *idp)
{
    uintN nclasses;
    JSClass **registry;

    nclasses = xdr->nclasses;
    if (nclasses == 0) {
	registry = JS_malloc(xdr->cx, REGISTRY_CHUNK * sizeof(JSClass *));
    } else if (nclasses % REGISTRY_CHUNK == 0) {
	registry = JS_realloc(xdr->cx,
			      xdr->registry,
			      (nclasses + REGISTRY_CHUNK) * sizeof(JSClass *));
    }
    if (!registry)
	return JS_FALSE;
    registry[nclasses++] = clasp;
    xdr->registry = registry;
    xdr->nclasses = nclasses;
    *idp = nclasses;
    return JS_TRUE;
}

uint32
JS_FindClassIdByName(JSXDRState *xdr, const char *name)
{
    uintN i;

    for (i = 0; i < xdr->nclasses; i++) {
	if (!strcmp(name, xdr->registry[i]->name))
	    return i+1;
    }
    return 0;
}

JSClass *
JS_FindClassById(JSXDRState *xdr, uint32 id)
{
    if (id > xdr->nclasses)
	return NULL;
    return xdr->registry[id-1];
}
