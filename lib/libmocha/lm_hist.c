/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*
 * JS reflection of the Navigator Session History.
 *
 * Brendan Eich, 9/8/95
 */
#include "lm.h"
#include "shist.h"
#include "structs.h"

enum hist_slot {
    HIST_LENGTH     = -1,
    HIST_CURRENT    = -2,
    HIST_PREVIOUS   = -3,
    HIST_NEXT       = -4
};

static JSPropertySpec hist_props[] = {
    {lm_length_str,     HIST_LENGTH,    JSPROP_ENUMERATE|JSPROP_READONLY},
    {"current",         HIST_CURRENT,   JSPROP_ENUMERATE|JSPROP_READONLY},
    {"previous",        HIST_PREVIOUS,  JSPROP_ENUMERATE|JSPROP_READONLY},
    {"next",            HIST_NEXT,      JSPROP_ENUMERATE|JSPROP_READONLY},
    {0}
};

typedef struct JSHistory {
    MochaDecoder    *decoder;
    JSObject     *object;
} JSHistory;

/*
 * DO NOT REMOVE: 
 *   forward decl of hist_go and early def of hist_forward to work
 *   around a Win16 compiler bug! 
 */
JSBool PR_CALLBACK
hist_go(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval);

PR_STATIC_CALLBACK(JSBool)
hist_forward(JSContext *cx, JSObject *obj, unsigned int argc, jsval *argv, jsval *rval)
{
    jsval plus_one;

    plus_one = INT_TO_JSVAL(1);
    return hist_go(cx, obj, 1, &plus_one, rval);
}

extern JSClass lm_hist_class;

PR_STATIC_CALLBACK(JSBool)
hist_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSHistory *hist;
    MochaDecoder *decoder;
    MWContext *context;
    History_entry *he, *he2;
    JSString *str;
    XP_List *histlist;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    hist = JS_GetInstancePrivate(cx, obj, &lm_hist_class, NULL);
    if (!hist)
	return JS_TRUE;
    decoder = hist->decoder;
    context = decoder->window_context;
    if (!context)
	return JS_TRUE;

    str = JSVAL_TO_STRING(JS_GetEmptyStringValue(cx));
    switch (slot) {
      case HIST_LENGTH:
	histlist = SHIST_GetList(context);
	*vp = INT_TO_JSVAL(XP_ListCount(histlist));
	return JS_TRUE;

      case HIST_CURRENT:
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    break;
	he = SHIST_GetCurrent(&context->hist);
	if (he && he->address)
	    str = JS_NewStringCopyZ(cx, he->address);
	break;

      case HIST_PREVIOUS:
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    break;
	he2 = SHIST_GetPrevious(context);
	if (he2 && he2->address)
	    str = JS_NewStringCopyZ(cx, he2->address);
	break;

      case HIST_NEXT:
	if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	    break;
	he2 = SHIST_GetNext(context);
	if (he2 && he2->address)
	    str = JS_NewStringCopyZ(cx, he2->address);
	break;

      default:
	if (slot < 0) {
	    /* Don't mess with user-defined or method properties. */
	    return JS_TRUE;
	}
	if (lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ)) {
	    /* Access granted against potential privacy attack */
	    he = SHIST_GetObjectNum(&context->hist, 1 + slot);
	    if (he && he->address) {
		str = JS_NewStringCopyZ(cx, he->address);
	    }
	}
	break;
    }

    /* Common tail code for string-type properties. */
    if (!str)
	return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
hist_finalize(JSContext *cx, JSObject *obj)
{
    JSHistory *hist;

    hist = JS_GetPrivate(cx, obj);
    if (!hist)
	return;
    DROP_BACK_COUNT(hist->decoder);
    XP_DELETE(hist);
}

JSClass lm_hist_class = {
    "History", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, hist_getProperty, hist_getProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, hist_finalize
};

JSBool PR_CALLBACK
hist_go(JSContext *cx, JSObject *obj,
	unsigned int argc, jsval *argv, jsval *rval)
{
    JSHistory *hist;
    MochaDecoder *decoder;
    MWContext *context;
    History_entry *he;
    int index, delta;
    XP_List *histlist;
    URL_Struct *url_struct;

    if (!JS_InstanceOf(cx, obj, &lm_hist_class, argv))
        return JS_FALSE;
    hist = JS_GetPrivate(cx, obj);
    if (!hist)
	return JS_TRUE;
    decoder = hist->decoder;
    context = decoder->window_context;
    if (!context)
	return JS_TRUE;
    he = SHIST_GetCurrent(&context->hist);

    if (JSVAL_IS_INT(argv[0])) {
	index = SHIST_GetIndex(&context->hist, he);
	delta = JSVAL_TO_INT(argv[0]);
	index += delta;
	he = SHIST_GetObjectNum(&context->hist, index);
    }
    else if (JSVAL_IS_STRING(argv[0])) {
	char * argv_str = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	histlist = SHIST_GetList(context);
	if (histlist)
	    histlist = histlist->next;

	for (index = XP_ListCount(histlist); index > 0; index--) {
	    he = SHIST_GetObjectNum(&context->hist, index);
	    if (XP_STRSTR(he->address, argv_str) ||
		XP_STRSTR(he->title, argv_str)) {
		goto out;
	    }
	}
	he = 0;
    }
    else {
	he = 0;
    }

    if (!he)
	return JS_TRUE;
out:
    url_struct = SHIST_CreateURLStructFromHistoryEntry(context, he);
    if (!url_struct) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    return lm_GetURL(cx, decoder, url_struct);
}

PR_STATIC_CALLBACK(JSBool)
hist_back(JSContext *cx, JSObject *obj,
	  unsigned int argc, jsval *argv, jsval *rval)
{
    jsval minus_one;

    minus_one = INT_TO_JSVAL(-1);
    return hist_go(cx, obj, 1, &minus_one, rval);
}

PR_STATIC_CALLBACK(JSBool)
hist_toString(JSContext *cx, JSObject *obj,
	      unsigned int argc, jsval *argv, jsval *rval)
{
    JSHistory *hist;
    MochaDecoder *decoder;
    MWContext *context;
    XP_List *histlist;
    History_entry *he;
    char *bytes;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &lm_hist_class, argv))
        return JS_FALSE;

    hist = JS_GetPrivate(cx, obj);
    if (!hist)
	return JS_TRUE;
    decoder = hist->decoder;
    context = decoder->window_context;
    if (!context)
	return JS_TRUE;
    histlist = SHIST_GetList(context);

    /* only build a string if we have permission */
    if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
	return JS_TRUE;

    bytes = 0;
    StrAllocCopy(bytes, "<TITLE>Window History</TITLE>"
		      "<TABLE BORDER=0 ALIGN=center VALIGN=top HSPACE=8>");
    while ((he = XP_ListNextObject(histlist)) != 0) {
	StrAllocCat(bytes, "<TR><TD VALIGN=top><STRONG>");
	StrAllocCat(bytes, he->title);
	StrAllocCat(bytes, "</STRONG></TD><TD>&nbsp;</TD>"
			 "<TD VALIGN=top><A HREF=\"");
	StrAllocCat(bytes, he->address);
	StrAllocCat(bytes, "\">");
	StrAllocCat(bytes, he->address);
	StrAllocCat(bytes, "</A></TD></TR>");
    }
    StrAllocCat(bytes, "</TABLE>");
    if (!bytes) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }

    str = JS_NewString(cx, bytes, XP_STRLEN(bytes));
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSFunctionSpec hist_methods[] = {
    {"go",              hist_go,                1},
    {"back",            hist_back,              0},
    {"forward",         hist_forward,           0},
    {lm_toString_str,   hist_toString,         1},
    {0}
};

/* XXX avoid ntypes.h History typedef name clash */
PR_STATIC_CALLBACK(JSBool)
History_ctor(JSContext *cx, JSObject *obj,
	     uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}


JSObject *
lm_DefineHistory(MochaDecoder *decoder)
{
    JSObject *obj;
    JSContext *cx;
    JSHistory *hist;

    obj = decoder->history;
    if (obj)
	return obj;

    cx = decoder->js_context;
    hist = JS_malloc(cx, sizeof *hist);
    if (!hist)
	return NULL;
    XP_BZERO(hist, sizeof *hist);

    obj = JS_InitClass(cx, decoder->window_object, NULL, &lm_hist_class,
		       History_ctor, 0, hist_props, hist_methods, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, hist)) {
	JS_free(cx, hist);
	return NULL;
    }

    if (!JS_DefineProperty(cx, decoder->window_object, "history",
			   OBJECT_TO_JSVAL(obj), NULL, NULL,
			   JSPROP_ENUMERATE|JSPROP_READONLY)) {
	return NULL;
    }

    hist->object = obj;
    hist->decoder = HOLD_BACK_COUNT(decoder);
    decoder->history = obj;
    return obj;
}
