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

#ifndef jsapi_h___
#define jsapi_h___
/*
 * JavaScript API.
 */
#include <stddef.h>
#include "jspubtd.h"

PR_BEGIN_EXTERN_C

/*
 * Type tags stored in the low bits of a jsval.
 */
#define JSVAL_OBJECT            0x0     /* untagged reference to object */
#define JSVAL_INT               0x1     /* tagged 31-bit integer value */
#define JSVAL_DOUBLE            0x2     /* tagged reference to double */
#define JSVAL_STRING            0x4     /* tagged reference to string */
#define JSVAL_BOOLEAN           0x6     /* tagged boolean value */

/* Type tag bitfield length and derived macros. */
#define JSVAL_TAGBITS           3
#define JSVAL_TAGMASK           PR_BITMASK(JSVAL_TAGBITS)
#define JSVAL_TAG(v)            ((v) & JSVAL_TAGMASK)
#define JSVAL_SETTAG(v,t)       ((v) | (t))
#define JSVAL_CLRTAG(v)         ((v) & ~(jsval)JSVAL_TAGMASK)
#define JSVAL_ALIGN             PR_BIT(JSVAL_TAGBITS)

/* Predicates for type testing. */
#define JSVAL_IS_OBJECT(v)      (JSVAL_TAG(v) == JSVAL_OBJECT)
#define JSVAL_IS_NUMBER(v)      (JSVAL_IS_INT(v) || JSVAL_IS_DOUBLE(v))
#define JSVAL_IS_INT(v)         (((v) & JSVAL_INT) && (v) != JSVAL_VOID)
#define JSVAL_IS_DOUBLE(v)      (JSVAL_TAG(v) == JSVAL_DOUBLE)
#define JSVAL_IS_STRING(v)      (JSVAL_TAG(v) == JSVAL_STRING)
#define JSVAL_IS_BOOLEAN(v)     (JSVAL_TAG(v) == JSVAL_BOOLEAN)
#define JSVAL_IS_NULL(v)        ((v) == JSVAL_NULL)
#define JSVAL_IS_VOID(v)        ((v) == JSVAL_VOID)

/* Objects, strings, and doubles are GC'ed. */
#define JSVAL_IS_GCTHING(v)     (!((v) & JSVAL_INT) && !JSVAL_IS_BOOLEAN(v))
#define JSVAL_TO_GCTHING(v)     ((void *)JSVAL_CLRTAG(v))
#define JSVAL_TO_OBJECT(v)      ((JSObject *)JSVAL_TO_GCTHING(v))
#define JSVAL_TO_DOUBLE(v)      ((jsdouble *)JSVAL_TO_GCTHING(v))
#define JSVAL_TO_STRING(v)      ((JSString *)JSVAL_TO_GCTHING(v))
#define OBJECT_TO_JSVAL(obj)    ((jsval)(obj))
#define DOUBLE_TO_JSVAL(dp)     JSVAL_SETTAG((jsval)(dp), JSVAL_DOUBLE)
#define STRING_TO_JSVAL(str)    JSVAL_SETTAG((jsval)(str), JSVAL_STRING)

/* Lock and unlock the GC thing held by a jsval. */
#define JSVAL_LOCK(cx,v)        (JSVAL_IS_GCTHING(v)                          \
				 ? JS_LockGCThing(cx, JSVAL_TO_GCTHING(v))    \
				 : JS_TRUE)
#define JSVAL_UNLOCK(cx,v)      (JSVAL_IS_GCTHING(v)                          \
				 ? JS_UnlockGCThing(cx, JSVAL_TO_GCTHING(v))  \
				 : JS_TRUE)

/* Domain limits for the jsval int type. */
#define JSVAL_INT_POW2(n)       ((jsval)1 << (n))
#define JSVAL_INT_MIN           ((jsval)1 - JSVAL_INT_POW2(30))
#define JSVAL_INT_MAX           (JSVAL_INT_POW2(30) - 1)
#define INT_FITS_IN_JSVAL(i)    ((jsuint)((i)+JSVAL_INT_MAX) <= 2*JSVAL_INT_MAX)
#define JSVAL_TO_INT(v)         ((jsint)(v) >> 1)
#define INT_TO_JSVAL(i)         (((jsval)(i) << 1) | JSVAL_INT)

/* Convert between boolean and jsval. */
#define JSVAL_TO_BOOLEAN(v)     ((JSBool)((v) >> JSVAL_TAGBITS))
#define BOOLEAN_TO_JSVAL(b)     JSVAL_SETTAG((jsval)(b) << JSVAL_TAGBITS,     \
					     JSVAL_BOOLEAN)

/* A private data pointer (2-byte-aligned) can be stored as an int jsval. */
#define JSVAL_TO_PRIVATE(v)     ((void *)((v) & ~JSVAL_INT))
#define PRIVATE_TO_JSVAL(p)     ((jsval)(p) | JSVAL_INT)

/* Property flags, set in JSPropertySpec and passed to API functions. */
#define JSPROP_ENUMERATE        0x01    /* property is visible to for/in loop */
#define JSPROP_READONLY         0x02    /* not settable: assignment is error */
#define JSPROP_PERMANENT        0x04    /* property cannot be deleted */
#define JSPROP_EXPORTED         0x08    /* property is exported from object */
#define JSPROP_INDEX            0x20    /* name is actually (jsint) index */
#define JSPROP_ASSIGNHACK       0x40    /* property set by its assign method */
#define JSPROP_TINYIDHACK       0x80    /* prop->id is tinyid, not index */

/* Function flags, set in JSFunctionSpec and passed to JS_NewFunction etc. */
#define JSFUN_BOUND_METHOD      0x40    /* bind this to fun->object's parent */
#define JSFUN_GLOBAL_PARENT     0x80    /* reparent calls to cx->globalObject */

/*
 * Well-known JS values.  The extern'd variables are initialized when the
 * first JSContext is created by JS_NewContext (see below).
 */
#define JSVAL_VOID              INT_TO_JSVAL(0 - JSVAL_INT_POW2(30))
#define JSVAL_NULL              OBJECT_TO_JSVAL(0)
#define JSVAL_ZERO              INT_TO_JSVAL(0)
#define JSVAL_ONE               INT_TO_JSVAL(1)
#define JSVAL_FALSE             BOOLEAN_TO_JSVAL(JS_FALSE)
#define JSVAL_TRUE              BOOLEAN_TO_JSVAL(JS_TRUE)

/* Windows DLLs can't export data, so provide accessors for the above vars. */
PR_EXTERN(jsval)
JS_GetNaNValue(JSContext *cx);

PR_EXTERN(jsval)
JS_GetNegativeInfinityValue(JSContext *cx);

PR_EXTERN(jsval)
JS_GetPositiveInfinityValue(JSContext *cx);

PR_EXTERN(jsval)
JS_GetEmptyStringValue(JSContext *cx);

PR_EXTERN(JSBool)
JS_ConvertValue(JSContext *cx, jsval v, JSType type, jsval *vp);

PR_EXTERN(JSBool)
JS_ValueToObject(JSContext *cx, jsval v, JSObject **objp);

PR_EXTERN(JSFunction *)
JS_ValueToFunction(JSContext *cx, jsval v);

PR_EXTERN(JSString *)
JS_ValueToString(JSContext *cx, jsval v);

PR_EXTERN(JSBool)
JS_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp);

/*
 * Convert a value to a number, then to an int32 if it fits (discarding any
 * fractional part, but failing with an error if the double is out of range
 * or unordered).
 */
PR_EXTERN(JSBool)
JS_ValueToInt32(JSContext *cx, jsval v, int32 *ip);

PR_EXTERN(JSBool)
JS_ValueToUint16(JSContext *cx, jsval v, uint16 *ip);

PR_EXTERN(JSBool)
JS_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp);

PR_EXTERN(JSType)
JS_TypeOfValue(JSContext *cx, jsval v);

PR_EXTERN(const char *)
JS_GetTypeName(JSContext *cx, JSType type);

/************************************************************************/

/*
 * Initialization, locking, contexts, and memory allocation.
 */
PR_EXTERN(JSRuntime *)
JS_Init(uint32 maxbytes);

PR_EXTERN(void)
JS_Finish(JSRuntime *rt);

PR_EXTERN(void)
JS_Lock(JSRuntime *rt);

PR_EXTERN(void)
JS_Unlock(JSRuntime *rt);

PR_EXTERN(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stacksize);

PR_EXTERN(void)
JS_DestroyContext(JSContext *cx);

PR_EXTERN(JSRuntime *)
JS_GetRuntime(JSContext *cx);

PR_EXTERN(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp);

PR_EXTERN(JSVersion)
JS_GetVersion(JSContext *cx);

PR_EXTERN(JSVersion)
JS_SetVersion(JSContext *cx, JSVersion version);

PR_EXTERN(JSObject *)
JS_GetGlobalObject(JSContext *cx);

PR_EXTERN(void)
JS_SetGlobalObject(JSContext *cx, JSObject *obj);

/* NB: This sets cx's global object to obj if it was null. */
PR_EXTERN(JSBool)
JS_InitStandardClasses(JSContext *cx, JSObject *obj);

PR_EXTERN(JSObject *)
JS_GetScopeChain(JSContext *cx);

PR_EXTERN(void *)
JS_malloc(JSContext *cx, size_t nbytes);

PR_EXTERN(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes);

PR_EXTERN(void)
JS_free(JSContext *cx, void *p);

PR_EXTERN(char *)
JS_strdup(JSContext *cx, const char *s);

PR_EXTERN(jsdouble *)
JS_NewDouble(JSContext *cx, jsdouble d);

PR_EXTERN(JSBool)
JS_NewDoubleValue(JSContext *cx, jsdouble d, jsval *rval);

PR_EXTERN(JSBool)
JS_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval);

PR_EXTERN(JSBool)
JS_AddRoot(JSContext *cx, void *rp);

PR_EXTERN(JSBool)
JS_RemoveRoot(JSContext *cx, void *rp);

PR_EXTERN(JSBool)
JS_AddNamedRoot(JSContext *cx, void *rp, const char *name);

#ifdef DEBUG
PR_EXTERN(void)
JS_DumpNamedRoots(JSRuntime *rt,
		  void (*dump)(const char *name, void *rp, void *data),
		  void *data);
#endif

PR_EXTERN(JSBool)
JS_LockGCThing(JSContext *cx, void *thing);

PR_EXTERN(JSBool)
JS_UnlockGCThing(JSContext *cx, void *thing);

PR_EXTERN(void)
JS_GC(JSContext *cx);

PR_EXTERN(void)
JS_MaybeGC(JSContext *cx);

/************************************************************************/

/*
 * Classes, objects, and properties.
 */
struct JSClass {
    char            *name;
    uint32          flags;
    JSPropertyOp    addProperty;
    JSPropertyOp    delProperty;
    JSPropertyOp    getProperty;
    JSPropertyOp    setProperty;
    JSEnumerateOp   enumerate;
    JSResolveOp     resolve;
    JSConvertOp     convert;
    JSFinalizeOp    finalize;
    prword          spare[8];
};

#define JSCLASS_HAS_PRIVATE     0x01    /* class instances have private slot */
#define JSCLASS_NEW_RESOLVE     0x02    /* class has JSNewResolveOp method */

PR_EXTERN(JSBool)
JS_PropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

PR_EXTERN(JSBool)
JS_EnumerateStub(JSContext *cx, JSObject *obj);

PR_EXTERN(JSBool)
JS_ResolveStub(JSContext *cx, JSObject *obj, jsval id);

PR_EXTERN(JSBool)
JS_ConvertStub(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

PR_EXTERN(void)
JS_FinalizeStub(JSContext *cx, JSObject *obj);

struct JSConstDoubleSpec {
    jsdouble        dval;
    const char      *name;
    uint8           flags;
    uint8           spare[3];
};

/*
 * To define an array element rather than a named property member, cast the
 * element's index to (const char *) and initialize name with it, and set the
 * JSPROP_INDEX bit in flags.
 */
struct JSPropertySpec {
    const char      *name;
    int8            tinyid;
    uint8           flags;
    JSPropertyOp    getter;
    JSPropertyOp    setter;
};

struct JSFunctionSpec {
    const char      *name;
    JSNative        call;
    uint8           nargs;
    uint8           flags;
};

PR_EXTERN(JSObject *)
JS_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
	     JSClass *clasp, JSNative constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs);

PR_EXTERN(JSClass *)
JS_GetClass(JSObject *obj);

PR_EXTERN(JSBool)
JS_InstanceOf(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv);

PR_EXTERN(void *)
JS_GetPrivate(JSContext *cx, JSObject *obj);

PR_EXTERN(JSBool)
JS_SetPrivate(JSContext *cx, JSObject *obj, void *data);

PR_EXTERN(void *)
JS_GetInstancePrivate(JSContext *cx, JSObject *obj, JSClass *clasp,
                      jsval *argv);

PR_EXTERN(JSObject *)
JS_GetPrototype(JSContext *cx, JSObject *obj);

PR_EXTERN(JSBool)
JS_SetPrototype(JSContext *cx, JSObject *obj, JSObject *proto);

PR_EXTERN(JSObject *)
JS_GetParent(JSContext *cx, JSObject *obj);

PR_EXTERN(JSBool)
JS_SetParent(JSContext *cx, JSObject *obj, JSObject *parent);

PR_EXTERN(JSObject *)
JS_GetConstructor(JSContext *cx, JSObject *proto);

PR_EXTERN(JSObject *)
JS_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent);

PR_EXTERN(JSObject *)
JS_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
		   JSObject *parent);

PR_EXTERN(JSObject *)
JS_DefineObject(JSContext *cx, JSObject *obj, const char *name, JSClass *clasp,
		JSObject *proto, uintN flags);

PR_EXTERN(JSBool)
JS_DefineConstDoubles(JSContext *cx, JSObject *obj, JSConstDoubleSpec *cds);

PR_EXTERN(JSBool)
JS_DefineProperties(JSContext *cx, JSObject *obj, JSPropertySpec *ps);

PR_EXTERN(JSBool)
JS_DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
		  JSPropertyOp getter, JSPropertyOp setter, uintN flags);

PR_EXTERN(JSBool)
JS_DefinePropertyWithTinyId(JSContext *cx, JSObject *obj, const char *name,
			    int8 tinyid, jsval value,
			    JSPropertyOp getter, JSPropertyOp setter,
			    uintN flags);

PR_EXTERN(JSBool)
JS_AliasProperty(JSContext *cx, JSObject *obj, const char *name,
		 const char *alias);

PR_EXTERN(JSBool)
JS_LookupProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp);

PR_EXTERN(JSBool)
JS_GetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp);

PR_EXTERN(JSBool)
JS_SetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp);

PR_EXTERN(JSBool)
JS_DeleteProperty(JSContext *cx, JSObject *obj, const char *name);

PR_EXTERN(JSObject *)
JS_NewArrayObject(JSContext *cx, jsint length, jsval *vector);

PR_EXTERN(JSBool)
JS_IsArrayObject(JSContext *cx, JSObject *obj);

PR_EXTERN(JSBool)
JS_GetArrayLength(JSContext *cx, JSObject *obj, jsint *lengthp);

PR_EXTERN(JSBool)
JS_SetArrayLength(JSContext *cx, JSObject *obj, jsint length);

PR_EXTERN(JSBool)
JS_HasLengthProperty(JSContext *cx, JSObject *obj);

PR_EXTERN(JSBool)
JS_DefineElement(JSContext *cx, JSObject *obj, jsint index, jsval value,
		 JSPropertyOp getter, JSPropertyOp setter, uintN flags);

PR_EXTERN(JSBool)
JS_AliasElement(JSContext *cx, JSObject *obj, const char *name, jsint alias);

PR_EXTERN(JSBool)
JS_LookupElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp);

PR_EXTERN(JSBool)
JS_GetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp);

PR_EXTERN(JSBool)
JS_SetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp);

PR_EXTERN(JSBool)
JS_DeleteElement(JSContext *cx, JSObject *obj, jsint index);

PR_EXTERN(void)
JS_ClearScope(JSContext *cx, JSObject *obj);

/************************************************************************/

/*
 * Security protocol.
 */

typedef struct JSPrincipals {
    char *codebase;
    void *(*getPrincipalArray)(JSContext *cx, struct JSPrincipals *);
    JSBool (*globalPrivilegesEnabled)(JSContext *cx,
                                      struct JSPrincipals *);
    /* Don't call "destroy"; use reference counting macros below. */
    uintN refcount;
    void (*destroy)(JSContext *cx, struct JSPrincipals *);
} JSPrincipals;

#define JSPRINCIPALS_HOLD(cx, principals)               \
    ((principals)->refcount++)
#define JSPRINCIPALS_DROP(cx, principals)               \
    ((--((principals)->refcount) == 0)                  \
        ? (*(principals)->destroy)((cx), (principals))  \
        : (void) 0)

/************************************************************************/

/*
 * Functions and scripts.
 */
PR_EXTERN(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative call, uintN nargs, uintN flags,
	       JSObject *parent, const char *name);

PR_EXTERN(JSObject *)
JS_GetFunctionObject(JSFunction *fun);

PR_EXTERN(const char *)
JS_GetFunctionName(JSFunction *fun);

PR_EXTERN(JSBool)
JS_DefineFunctions(JSContext *cx, JSObject *obj, JSFunctionSpec *fs);

PR_EXTERN(JSFunction *)
JS_DefineFunction(JSContext *cx, JSObject *obj, const char *name, JSNative call,
		  uintN nargs, uintN flags);

PR_EXTERN(JSScript *)
JS_CompileScript(JSContext *cx, JSObject *obj,
		 const char *bytes, size_t length,
		 const char *filename, uintN lineno);

PR_EXTERN(JSScript *)
JS_CompileScriptForPrincipals(JSContext *cx, JSObject *obj,
			      JSPrincipals *principals,
			      const char *bytes, size_t length,
			      const char *filename, uintN lineno);

PR_EXTERN(JSScript *)
JS_CompileUCScript(JSContext *cx, JSObject *obj,
		   const jschar *chars, size_t length,
		   const char *filename, uintN lineno);

PR_EXTERN(JSScript *)
JS_CompileUCScriptForPrincipals(JSContext *cx, JSObject *obj,
				JSPrincipals *principals,
				const jschar *chars, size_t length,
				const char *filename, uintN lineno);

#ifdef JSFILE
PR_EXTERN(JSScript *)
JS_CompileFile(JSContext *cx, JSObject *obj, const char *filename);
#endif

PR_EXTERN(void)
JS_DestroyScript(JSContext *cx, JSScript *script);

PR_EXTERN(JSFunction *)
JS_CompileFunction(JSContext *cx, JSObject *obj, const char *name,
		   uintN nargs, const char **argnames,
		   const char *bytes, size_t length,
		   const char *filename, uintN lineno);

PR_EXTERN(JSFunction *)
JS_CompileFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals, const char *name,
                                uintN nargs, const char **argnames,
                                const char *bytes, size_t length,
                                const char *filename, uintN lineno);

PR_EXTERN(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JSObject *obj, const char *name,
		     uintN nargs, const char **argnames,
		     const jschar *chars, size_t length,
		     const char *filename, uintN lineno);

PR_EXTERN(JSFunction *)
JS_CompileUCFunctionForPrincipals(JSContext *cx, JSObject *obj,
				  JSPrincipals *principals, const char *name,
				  uintN nargs, const char **argnames,
				  const jschar *chars, size_t length,
				  const char *filename, uintN lineno);

PR_EXTERN(JSString *)
JS_DecompileScript(JSContext *cx, JSScript *script, const char *name,
		   uintN indent);

PR_EXTERN(JSString *)
JS_DecompileFunction(JSContext *cx, JSFunction *fun, uintN indent);

PR_EXTERN(JSString *)
JS_DecompileFunctionBody(JSContext *cx, JSFunction *fun, uintN indent);

PR_EXTERN(JSBool)
JS_ExecuteScript(JSContext *cx, JSObject *obj, JSScript *script, jsval *rval);

PR_EXTERN(JSBool)
JS_EvaluateScript(JSContext *cx, JSObject *obj,
		  const char *bytes, uintN length,
		  const char *filename, uintN lineno,
		  jsval *rval);

PR_EXTERN(JSBool)
JS_EvaluateScriptForPrincipals(JSContext *cx, JSObject *obj,
			       JSPrincipals *principals,
			       const char *bytes, uintN length,
			       const char *filename, uintN lineno,
			       jsval *rval);

PR_EXTERN(JSBool)
JS_EvaluateUCScript(JSContext *cx, JSObject *obj,
		    const jschar *chars, uintN length,
		    const char *filename, uintN lineno,
		    jsval *rval);

PR_EXTERN(JSBool)
JS_EvaluateUCScriptForPrincipals(JSContext *cx, JSObject *obj,
				 JSPrincipals *principals,
				 const jschar *chars, uintN length,
				 const char *filename, uintN lineno,
				 jsval *rval);

PR_EXTERN(JSBool)
JS_CallFunction(JSContext *cx, JSObject *obj, JSFunction *fun, uintN argc,
		jsval *argv, jsval *rval);

PR_EXTERN(JSBool)
JS_CallFunctionName(JSContext *cx, JSObject *obj, const char *name, uintN argc,
		    jsval *argv, jsval *rval);

PR_EXTERN(JSBool)
JS_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval, uintN argc,
		     jsval *argv, jsval *rval);

PR_EXTERN(JSBranchCallback)
JS_SetBranchCallback(JSContext *cx, JSBranchCallback cb);

PR_EXTERN(JSBool)
JS_IsRunning(JSContext *cx);

/************************************************************************/

/*
 * Strings.
 */
PR_EXTERN(JSString *)
JS_NewString(JSContext *cx, char *bytes, size_t length);

PR_EXTERN(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n);

PR_EXTERN(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s);

PR_EXTERN(JSString *)
JS_InternString(JSContext *cx, const char *s);

PR_EXTERN(JSString *)
JS_NewUCString(JSContext *cx, jschar *chars, size_t length);

PR_EXTERN(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const jschar *s, size_t n);

PR_EXTERN(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const jschar *s);

PR_EXTERN(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s);

PR_EXTERN(char *)
JS_GetStringBytes(JSString *str);

PR_EXTERN(jschar *)
JS_GetStringChars(JSString *str);

PR_EXTERN(size_t)
JS_GetStringLength(JSString *str);

PR_EXTERN(intN)
JS_CompareStrings(JSString *str1, JSString *str2);

/************************************************************************/

/*
 * Error reporting.
 */

/*
 * Report an exception represented by the sprintf-like conversion of format
 * and its arguments.  This exception message string is passed to a pre-set
 * MochaErrorReporter function (see immediately below).
 */
PR_EXTERN(void)
JS_ReportError(JSContext *cx, const char *format, ...);

/*
 * Complain when out of memory.
 */
PR_EXTERN(void)
JS_ReportOutOfMemory(JSContext *cx);

struct JSErrorReport {
    const char      *filename;  /* source file name, URL, etc., or null */
    uintN           lineno;     /* source line number */
    const char      *linebuf;   /* offending source line without final '\n' */
    const char      *tokenptr;  /* pointer to error token in linebuf */
    const jschar    *uclinebuf; /* unicode (original) line buffer */
    const jschar    *uctokenptr;/* unicode (original) token pointer */
};

PR_EXTERN(JSErrorReporter)
JS_SetErrorReporter(JSContext *cx, JSErrorReporter er);

/************************************************************************/

/*
 * Regular Expressions.
 */
#define JSREG_FOLD      0x01    /* fold uppercase to lowercase */
#define JSREG_GLOB      0x02    /* global exec, creates array of matches */

PR_EXTERN(JSObject *)
JS_NewRegExpObject(JSContext *cx, char *bytes, size_t length, uintN flags);

PR_EXTERN(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, jschar *chars, size_t length, uintN flags);

PR_EXTERN(void)
JS_SetRegExpInput(JSContext *cx, JSString *input, JSBool multiline);

PR_EXTERN(void)
JS_ClearRegExpStatics(JSContext *cx);

PR_EXTERN(void)
JS_ClearRegExpRoots(JSContext *cx);

/* TODO: compile, execute, get/set other statics... */

/************************************************************************/

#ifdef NETSCAPE_INTERNAL
/*
 * This function and related data are compiled only ifdef NETSCAPE_INTERNAL.
 * A future version of the JS runtime will use Unicode strings; the API will
 * provide JS_EncodeString (from Unicode to a given character set encoding)
 * and JS_DecodeString (reverse) functions.
 */
PR_EXTERN(void)
JS_SetCharSetInfo(JSContext *cx, const char *csName, int csId,
		  JSCharScanner scanner);

/*
 * Returns true if a script is executing and its current bytecode is a set
 * (assignment) operation.
 */
PR_EXTERN(JSBool)
JS_IsAssigning(JSContext *cx);
#endif

PR_END_EXTERN_C

#endif /* jsapi_h___ */
