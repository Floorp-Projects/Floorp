/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * PerlConnect. Provides means for OO Perl <==> JS communications
 */

/* This is an program written in XSUB. You need to compile it using xsubpp   */
/* usually found in your perl\bin directory. On my machine I do it like this:*/
/*      perl c:\perl\lib\ExtUtils\xsubpp  -typemap  \                        */
/*           c:\perl\lib\extutils\typemap -typemap typemap JS.xs > JS.c      */
/* See perlxs man page for details.                                          */
/* Don't edit the resulting C file directly. See README.html for more info   */
/* on PerlConnect in general                                                 */

#ifdef __cplusplus
    extern "C"; {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
    }
#endif

#include "../jsapi.h"
#include "jsperlpvt.h"

static
JSClass global_class = {
    "Global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

/* Helper functions needed for most JS API routines */
static JSRuntime *
getRuntime()
{
    return (JSRuntime *)SvIV((SV*)SvRV(perl_get_sv("JS::Runtime::rt", FALSE)));
}

static JSContext *
getContext()
{
    return (JSContext *)SvIV((SV*)SvRV(perl_get_sv("JS::Context::this", FALSE)));
}

/*
    The following packages are defined below:
    JS -- main container for all JS functionality
        JS::Runtime -- wrapper around JSRuntime *
        JS::Context -- wrapper around JSContext *
        JS::Object  -- wrapper around JSObject *
 */

MODULE = JS  PACKAGE = JS   PREFIX = JS_
PROTOTYPES:  DISABLE
 # package JS

 #   Most of the functions below have names coinsiding with those of the
 #   corresponding JS API functions. Thus, they are not commented.
JSRuntime *
JS_NewRuntime(maxbytes)
    int maxbytes

void
JS_DestroyRuntime(rt)
    JSRuntime *rt
    CODE:
    /*
        Make sure that the reference count to the runtime is zero.
        O.w. this sequence of commands will cause double-deallocation:
            $rt = new JS::Runtime(10_000);
            $rt1 = $rt;
            [exit here]
        So both $rt->DESTROY and $rt1->DESTROY will cause runtime destruction.
     */
    if(SvREFCNT(ST(0))==0){
        JS_DestroyRuntime(rt);
    }

 # package JS::Runtime
MODULE = JS    PACKAGE = JS::Runtime   PREFIX = JS_

JSContext *
JS_NewContext(rt, stacksize)
    JSRuntime *rt
    int        stacksize

    CODE:
    {
        JSObject *obj;
        jsval v;
        /* Here we are creating the globals object ourselves. */
        JSContext *cx = JS_NewContext(rt, stacksize);
        obj = JS_NewObject(cx, &global_class, NULL, NULL);
        JS_InitStandardClasses(cx, obj);
        RETVAL = cx;
    }
    OUTPUT:
    RETVAL

void
JS_DestroyContext(cx)
    JSContext *cx
    CODE:
    /* See the comment about ref. count above */
    if(SvREFCNT(ST(0))==0){
        JS_DestroyContext(cx);
    }


 # package JS::Context
MODULE = JS    PACKAGE = JS::Context   PREFIX = JS_

jsval
JS_eval(cx, bytes)
    JSContext *cx
    char *bytes

    CODE:
    {
        jsval rval;
		JSObject *obj;
        /* Call on the global object */
        if(!JS_EvaluateScript(cx, JS_GetGlobalObject(cx), bytes, strlen(bytes), "Perl", 0, &rval)){
            croak("Perl eval failed");
            XSRETURN_UNDEF;
        }
        RETVAL = rval;
    }
    OUTPUT:
    RETVAL

 # package JS::Object
MODULE = JS    PACKAGE = JS::Object   PREFIX = JS_

 #
 #   The methods below get used when hash is tied.
 #
JSObject *
JS_TIEHASH(class, obj)
    char *class
    JSObject *obj
    CODE:
        RETVAL = obj;
    OUTPUT:
    RETVAL

jsval
JS_FETCH(obj, key)
    JSObject *obj
    char *key
    PREINIT:
    jsval rval;
    CODE:
    {
        /*printf("in FETCH\n");*/
        JS_GetProperty(getContext(), obj, key, &rval);
        RETVAL = rval;
    }
    OUTPUT:
    RETVAL

void
JS_STORE(obj, key, value)
    JSObject *obj
    char *key
    jsval value
    CODE:
    {
        /*printf("In STORE\n");*/
        JS_SetProperty(getContext(), obj, key, &value);
    }

void
JS_DELETE(obj, key)
    JSObject *obj
    char *key
    CODE:
    {
        /*printf("In DELETE\n");*/
        JS_DeleteProperty(getContext(), obj, key);
    }

void
JS_CLEAR(obj)
    JSObject *obj
    CODE:
    {
        /*printf("In CLEAR\n");*/
        JS_ClearScope(getContext(), obj);
    }

int
JS_EXISTS(obj, key)
    JSObject *obj
    char *key
    CODE:
    {
        jsval v;

        /*printf("In EXISTS\n");*/
        JS_LookupProperty(getContext(), obj, key, &v);
        RETVAL = !JSVAL_IS_VOID(v);
    }
    OUTPUT:
    RETVAL
