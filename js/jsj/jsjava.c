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
/* ** */
/*
 * JS reflection of Java objects and vice versa
 *
 *   javapackage is a java namespace
 *   java is a java class or object
 *   javaarray is a java array
 *   javaslot represents an object+name that may resolve to
 *     either a field or a method depending on later usage.
 *
 *   netscape.javascript.JSObject is a java reflection of a JS object
 */

#ifdef JAVA

#include <stdlib.h>
#include <string.h>

/* java joy */
#include "oobj.h"
#include "interpreter.h"
#include "tree.h"
#include "opcodes.h"
#include "javaString.h"
#include "exceptions.h"
#include "jri.h"

/* nspr stuph */
#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif
#include "prmon.h"      /* for PR_XLock and PR_XUNlock */
#include "prlog.h"
#include "prprf.h"
#include "prgc.h"

#include "jsapi.h"
#include "jsjava.h"
#include "jscntxt.h"    /* for cx->savedErrors */
#include "jsscript.h"   /* for script->javaData */
#include "jsobj.h"      /* for OBJ_GET_SLOT and JSSLOT_PRIVATE */
#include "jsfun.h"      /* for JSFUN_BOUND_METHOD */
#include "jslock.h"     /* for JS_LOCK_RUNTIME/JS_UNLOCK_RUNTIME */

#ifdef MACLINUX
#define VARARGS_ASSIGN(foo, bar) foo[0] = bar[0]
#else
#define VARARGS_ASSIGN(foo, bar) (foo) = (bar)
#endif /*MACLINUX*/

#ifndef NSPR20
#define WAIT_FOREVER LL_MAXINT
#else
#define WAIT_FOREVER PR_INTERVAL_NO_TIMEOUT
/* PR_LOG hacks */
#define debug PR_LOG_MAX
#define error PR_LOG_ERROR
#define warn PR_LOG_WARNING
#endif

/*
 * "exception" is defined by some Windows OS headers.
 * Redefine it for our own purposes.
 */
#ifdef exception
#undef exception
#endif

/*
 * we don't have access to the classJavaLangObject from the java
 * runtime dll, so we need a different version of this macro
 */
static ClassClass *js_classJavaLangObject = 0;
#undef obj_array_classblock
#define obj_array_classblock(obj) \
    ((obj_flags((obj)) == T_NORMAL_OBJECT) ? (obj)->methods->classdescriptor \
                                           : ObjectClassBlock)

/* JSObject generated header */
#define IMPLEMENT_netscape_javascript_JSObject
#include "netscape_javascript_JSObject.h"
#define IMPLEMENT_netscape_javascript_JSException
#ifdef XP_MAC
#include "n_javascript_JSException.h"
#else
#include "netscape_javascript_JSException.h"
#endif
#include "java_lang_Throwable.h"

/*
 * types of reflected java objects
 */

/* a package is basically just a name, since the jdk doesn't
 * allow you to find out whether a particular name represents
 * a package or not */
typedef struct JSJavaPackage JSJavaPackage;

/* either a java class or a java object.
 *   class.field is a static field or method
 *   class(...) constructs an object that is an instance of the class
 *   object.field is a field or method */
typedef struct JSJava JSJava;

/* type associated with a JSJava structure */
/* the JSClass java_class uses OBJECT and CLASS
 * javaarray_class uses ARRAY */
typedef enum JSJavaType {
    JAVA_UNDEF,
    JAVA_OBJECT,
    JAVA_CLASS,
    JAVA_ARRAY
} JSJavaType;

/* JSJClassData holds the info necessary for js to represent
 * itself to java.  When js calls java, it leaves a call to
 * the method referred to by mb on the java stack.  Each
 * JSScript that calls java contains its own copy of this,
 * though the classloader and class may be shared. */
typedef struct {
    int nrefs;
    jglobal loader;
    jglobal clazz;
    struct methodblock *mb;
} JSJClassData;

/* fields and methods are initially represented with a slot object:
 * this allows us to cope with fields and methods that have the same
 * name.  if the slot is used in a function context it will call the
 * appropriate method (dynamically choosing between overloaded methods).
 * if it is used in any other context it will convert itself to the
 * value of the field at the time it was looked up. */
typedef struct JSJavaSlot JSJavaSlot;

/*
 * globals for convenience, set up in the init routine
 */
static ClassClass * ObjectClassBlock = 0;
static ClassClass * JSObjectClassBlock = 0;
static ClassClass * JSExceptionClassBlock = 0;
static ClassClass * StringClassBlock = 0;
static ClassClass * BooleanClassBlock = 0;
static ClassClass * DoubleClassBlock = 0;
static ClassClass * ThrowableClassBlock = 0;
static JRIFieldID JSObjectInternalField;

/* note that these hash tables are only stable because we
 * use a non-moving garbage collector! */

/*
 * this is used to ensure that there is at most one reflection
 * of any java object.	objects are keyed by handle, classes are
 * keyed by classblock pointer.  the value for each is the JS
 * object reflection.  there is a root finder registered with the
 * gc that marks all the java objects (keys).  the JS objects
 * are not in the GC's root set, and are responsible for removing
 * themselves from the table upon finalization.
 */
static PRHashTable *javaReflections = NULL;
static PRMonitor *javaReflectionsMonitor = 0;
/* this jsContext is used when scanning the reflection table */

/*
 * similarly for java reflections of JS objects - in this case
 * the keys are JS objects.  when the corresponding java instance
 * is finalized, the entry is removed from the table, and a GC
 * root for the JS object is removed.
 */
static PRHashTable *jsReflections = NULL;
static PRMonitor *jsReflectionsMonitor = 0;

/*
 * Because of the magic of windows DLLs we need to get this passed in
 */
static JSJCallbacks *jsj_callbacks = NULL;

/*
 * we keep track of the "main" runtime in order to use it for
 * finalization (see js_RemoveReflection).  this depends on there
 * being only one JSRuntime which can run java, which may not be
 * true eventually.
 */
JSRuntime *finalizeRuntime = 0;

#ifndef NSPR20
#ifdef MOZILLA_CLIENT
PR_LOG_DEFINE(MojaSrc);
#else
PRLogModuleInfo *MojaSrc;
#endif
#else
PRLogModuleInfo *MojaSrc = NULL;
#endif

/* lazy initialization */
static void jsj_FinishInit(JSContext *cx, JRIEnv *env);

/* check if a field is static/nonstatic */
/* this now accepts static fields for non-static references */
#define CHECK_STATIC(isStatic, fb) (((fb)->access & ACC_STATIC) \
				    ? (isStatic) : !(isStatic))

/* forward declarations */
typedef void (*JSJCallback)(void*);

static JSBool
js_CallJava(JSContext *cx, JSJCallback doit, void *d, JSBool pushSafeFrame);

static JSBool
js_FindSystemClass(JSContext *cx, char *name, ClassClass **clazz);

static ClassClass *
js_FindJavaClass(JSContext *cx, char *name, ClassClass *from);

static JSBool
js_ExecuteJavaMethod(JSContext *cx, void *raddr, size_t rsize,
		     HObject *ho, char *name, char *sig,
		     struct methodblock *mb, JSBool isStaticCall, ...);

static HObject *
js_ConstructJava(JSContext *cx, char *name, ClassClass *cb, char *sig, ...);

static HObject *
js_ConstructJavaPrivileged(JSContext *cx, char *name, ClassClass *cb,
			   char *sig, ...);

static JSObject *
js_ReflectJava(JSContext *cx, JSJavaType type, HObject *handle,
	       ClassClass *cb, char *sig, JSObject *useMe);

static JSBool
js_reflectJavaSlot(JSContext *cx, JSObject *obj, JSString *str, jsval *vp);

static JSBool
js_javaMethodWrapper(JSContext *cx, JSObject *obj,
		     PRUintn argc, jsval *argv, jsval *rval);

static JSBool
js_javaConstructorWrapper(JSContext *cx, JSObject *obj,
			  PRUintn argc, jsval *argv, jsval *rval);

static JSBool
js_JArrayElementType(HObject *handle, char **sig, ClassClass **classp);

static JSBool
js_convertJSValueToJSObject(JSContext *cx, JSObject *mo, ClassClass *paramcb,
			    JSBool checkOnly, HObject **objp);

static JSBool
js_convertJSValueToJElement(JSContext *cx, jsval v,
			    char *addr, char *sig, ClassClass *fromclass,
			    char **sigRest);

static JSBool
js_convertJSValueToJValue(JSContext *cx, jsval v,
			  OBJECT *addr, char *sig, ClassClass *fromclass,
			  JSBool checkOnly, char **sigRestPtr,
			  int *cost);

static JSBool
js_convertJSValueToJField(JSContext *cx, jsval v,
			  HObject *ho, struct fieldblock *fb);

static JSBool
js_convertJElementToJSValue(JSContext *cx, char *addr, char *sig,
			    jsval *v, JSType desired);

static JSBool
js_convertJValueToJSValue(JSContext *cx, OBJECT *addr, char *sig,
			  jsval *vp, JSType desired);

static JSBool
js_convertJFieldToJSValue(JSContext *cx, HObject *ho, struct fieldblock *fb,
			  jsval *vp, JSType desired);

static JSBool
js_convertJObjectToJSString(JSContext *cx, HObject *ho, JSBool isClass,
			    jsval *vp);

static JSBool
js_convertJObjectToJSNumber(JSContext *cx, HObject *ho, JSBool isClass,
			    jsval *vp);

static JSBool
js_convertJObjectToJSBoolean(JSContext *cx, HObject *ho, JSBool isClass,
			     jsval *vp);

static JSJClassData *
jsj_MakeClassData(JSContext *cx);

static void
jsj_DestroyClassData(JSContext *cx, JSJClassData *data);

static JSBool
js_pushSafeFrame(JSContext *cx, ExecEnv *ee, JSJClassData **classData);

static void
js_popSafeFrame(JSContext *cx, ExecEnv *ee, JSJClassData *classData);

/* handy macro from agent.c */
#define obj_getoffset(o, off) (*(OBJECT *)((char *)unhand(o)+(off)))

/* Java threads call this to find the JSContext to use for
 * executing JavaScript */
PR_IMPLEMENT(int)
JSJ_IsEnabled()
{
    return jsj_callbacks->isEnabled();
}

/* Java threads call this to find the JSContext to use for
 * executing JavaScript */
PR_IMPLEMENT(JSContext *)
JSJ_CurrentContext(JRIEnv *env, char **errp)
{
    return jsj_callbacks->currentContext(env, errp);
}

/* Java threads call this before executing JS code (to preserve
 * run-to-completion in the client */
PR_IMPLEMENT(void)
JSJ_EnterJS(void)
{
    jsj_callbacks->enterJS();
}

/* Java threads call this when finished executing JS code */
PR_IMPLEMENT(void)
JSJ_ExitJS(void)
{
    jsj_callbacks->exitJS();
}

/* Java threads call this when finished executing JS code */
PR_IMPLEMENT(JSObject *)
JSJ_GetDefaultObject(JRIEnv *env, jobject hint)
{
    return jsj_callbacks->getDefaultObject(env, hint);
}

static ExecEnv *
jsj_GetCurrentEE(JSContext *cx)
{
    JRIEnv *env = 0;
    static int js_java_initialized = 0;

    if (js_java_initialized != 2) {
	/* use a cached monitor to make sure that we only
	 * initialize once */
	PR_CEnterMonitor(&js_java_initialized);
	switch (js_java_initialized) {
          case 0:   /* we're first */
	    js_java_initialized = 1;
	    PR_CExitMonitor(&js_java_initialized);

	    /* force both Java and JS to be initialized */
	    env = jsj_callbacks->currentEnv(cx);

            if (!env)
                goto out;

	    /* initialize the hash tables and classes we need */
	    jsj_FinishInit(cx, env);

	    PR_CEnterMonitor(&js_java_initialized);
	    js_java_initialized = 2;
	    PR_CNotifyAll(&js_java_initialized);
	    break;
	  case 1:   /* in progress */
	    PR_CWait(&js_java_initialized, WAIT_FOREVER);
	    break;
	  case 2:   /* done */
	    break;
	}
	PR_CExitMonitor(&js_java_initialized);
    }

    if (!env)
	env = jsj_callbacks->currentEnv(cx);

  out:
    if (!env)
        JS_ReportError(cx, "unable to get Java execution context for JavaScript");

    return (ExecEnv *) env;
}

/****	****	****	****	****	****	****	****	****
 *
 *   java packages are just strings
 *
 ****	****	****	****	****	****	****	****	****/

struct JSJavaPackage {
    char *name;         /* e.g. "java/lang" or 0 if it's the top level */
};

/* javapackage uses standard getProperty */

static JSBool
javapackage_setProperty(JSContext *cx, JSObject *obj, jsval slot, jsval *vp)
{
    JSJavaPackage *package = JS_GetPrivate(cx, obj);

    JS_ReportError(cx, "%s doesn't refer to any Java value",
		   package->name);
    return JS_FALSE;
}

static JSBool
javapackage_enumerate(JSContext *cx, JSObject *obj)
{
    /* FIXME can't do this without reading directories... */
    return JS_TRUE;
}

/* forward declaration */
static JSBool
javapackage_resolve(JSContext *cx, JSObject *obj, jsval id);

static JSBool
javapackage_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JSJavaPackage *package = JS_GetPrivate(cx, obj);
    JSString *str;
    char *name, *cp;

    switch (type) {
    case JSTYPE_STRING:
        /* convert '/' to '.' so it looks like the entry syntax */
	if (!package->name)
	    break;
        name = PR_smprintf("[JavaPackage %s]", package->name);
	if (!name) {
	    JS_ReportOutOfMemory(cx);
	    return JS_FALSE;
	}
        for (cp = name; *cp != '\0'; cp++)
            if (*cp == '/')
                *cp = '.';
	str = JS_NewString(cx, name, strlen(name));
	if (!str) {
	    free(name);
	    JS_ReportOutOfMemory(cx);
	    return JS_FALSE;
	}

	*vp = STRING_TO_JSVAL(str);
	break;
    case JSTYPE_OBJECT:
	*vp = OBJECT_TO_JSVAL(obj);
	break;
    default:
	break;
    }
    return JS_TRUE;
}

static void
javapackage_finalize(JSContext *cx, JSObject *obj)
{
    JSJavaPackage *package = JS_GetPrivate(cx, obj);

    /* get rid of the private data */
    if (package->name)
	JS_free(cx, package->name);
    JS_free(cx, package);
}

static JSClass javapackage_class = {
    "JavaPackage",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    JS_PropertyStub, javapackage_setProperty, javapackage_enumerate,
    javapackage_resolve, javapackage_convert, javapackage_finalize
};

/* needs pointer to javapackage_class */
static JSBool
javapackage_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSJavaPackage *package = JS_GetPrivate(cx, obj);
    char *fullname;
    char *name;
    int namelen;
    ClassClass *cb;
    JSObject *mo;
    JSJClassData *classData;
    JRIEnv *env;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));
    namelen = JS_GetStringLength(JSVAL_TO_STRING(id));

    if (package->name) {
	int packagelen = strlen(package->name);
	fullname = JS_malloc(cx, packagelen + namelen + 2);
	if (!fullname)
	    return JS_FALSE;
	strcpy(fullname, package->name);
        fullname[packagelen] = '/';
	strcpy(fullname + packagelen + 1, name);
    } else {
	fullname = JS_malloc(cx, namelen + 1);
	if (!fullname)
	    return JS_FALSE;
	strcpy(fullname, name);
    }

    PR_LOG(MojaSrc, debug, ("looking for java class \"%s\"", fullname));

    /* get the class whose classloader we use to find more classes */
    if (!jsj_callbacks->jsClassLoader) {
        classData = NULL;
    } else if (!(classData = jsj_MakeClassData(cx))) {
    	JS_free(cx, fullname);
	return JS_FALSE;
    }

    env = (JRIEnv *) jsj_GetCurrentEE(cx);
    if (!env) {
	JS_free(cx, fullname);
	return JS_FALSE;
    }

    /* see if the name is a class */
    cb = js_FindJavaClass(cx, fullname,
			  classData
                          ? JRI_GetGlobalRef(env, classData->clazz)
                          : NULL);

    if (jsj_callbacks->jsClassLoader)
        jsj_DestroyClassData(cx, classData);

    /* if not, it's a package */
    if (!cb) {
	JSJavaPackage *newpackage = JS_malloc(cx, sizeof(JSJavaPackage));
	if (!newpackage) {
	    JS_free(cx, fullname);
	    return JS_FALSE;
	}
        PR_LOG(MojaSrc, debug, ("creating package %s", fullname));
	newpackage->name = fullname;
	mo = JS_NewObject(cx, &javapackage_class, 0, 0); /* FIXME proto and parent ok? */
	if (!mo) {
	    JS_free(cx, fullname);
	    JS_free(cx, newpackage);
	    return JS_FALSE;
	}
	JS_SetPrivate(cx, mo, newpackage);
    } else {
	/* reflect the Class object */
	mo = js_ReflectJClassToJSObject(cx, cb);
	if (!mo) {
	    /* FIXME error message? */
	    return JS_FALSE;
	}
	JS_free(cx, fullname);
    }

    return JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(mo),
			     0, 0, JSPROP_READONLY);
}

/****	****	****	****	****	****	****	****	****/

struct JSJava {
    JSJavaType	type;			/* object / array / class */
    HObject	*handle;		/* handle to the java Object */
    ClassClass	*cb;			/* classblock, or element cb if array */
    char	*signature;		/* array element signature */
};

static struct fieldblock *
java_lookup_field(JSContext *cx, ClassClass *cb, JSBool isStatic,
		  const char *name)
{
    while (cb) {
	int i;
	for (i = 0; i < (int)cbFieldsCount(cb); i++) {
	    struct fieldblock *fb = cbFields(cb) + i;
	    if (CHECK_STATIC(isStatic, fb)
		&& !strcmp(fieldname(fb), name)) {
		return fb;
	    }
	}
	/* check the parent */
	if (cbSuperclass(cb))
	    cb = cbSuperclass(cb);
	else
	    cb = 0;
    }

    return 0;
}

static struct fieldblock *
java_lookup_name(JSContext *cx, ClassClass *cb, JSBool isStatic,
		 const char *name)
{
    while (cb) {
	int i;
	for (i = 0; i < (int)cbFieldsCount(cb); i++) {
	    struct fieldblock *fb = cbFields(cb) + i;
	    if (CHECK_STATIC(isStatic, fb)
		&& !strcmp(fieldname(fb), name)) {
		return fb;
	    }
	}
	for (i = cbMethodsCount(cb); i--;) {
	    struct methodblock *mb = cbMethods(cb) + i;
	    if (CHECK_STATIC(isStatic, &mb->fb)
		&& !strcmp(fieldname(&mb->fb), name)) {
		return &mb->fb;
	    }
	}
	/* check the parent */
	if (cbSuperclass(cb))
	    cb = cbSuperclass(cb);
	else
	    cb = 0;
    }
    return 0;
}

static JSBool
java_getProperty(JSContext *cx, JSObject *obj, jsval slot, jsval *vp)
{
    JSString *str;
    JSJava *java = JS_GetPrivate(cx, obj);

    if (!java) {
        JS_ReportError(cx, "illegal operation on Java prototype object");
	return JS_FALSE;
    }

    /* FIXME reflect a getter/setter pair as a property! */

    if (!JSVAL_IS_STRING(slot)) {
        JS_ReportError(cx, "invalid Java property expression");
	return JS_FALSE;
    }

    str = JSVAL_TO_STRING(slot);
    PR_LOG(MojaSrc, debug, ("looked up slot \"%s\"", JS_GetStringBytes(str)));

    /* utter hack so that the assign hack doesn't kill us */
    if (slot == STRING_TO_JSVAL(ATOM_TO_STRING(cx->runtime->atomState.assignAtom))) {
	*vp = JSVAL_VOID;
	return JS_TRUE;
    }

    return js_reflectJavaSlot(cx, obj, str, vp);
}

static JSBool
java_setProperty(JSContext *cx, JSObject *obj, jsval slot, jsval *vp)
{
    JSJava *java = JS_GetPrivate(cx, obj);
    ClassClass *cb;
    struct fieldblock *fb = 0;
    JSString *str;
    const char *name;

    if (!java) {
        JS_ReportError(cx, "illegal operation on Java prototype object");
	return JS_FALSE;
    }

    cb = java->cb;

    if (!JSVAL_IS_STRING(slot)) {
        JS_ReportError(cx, "invalid Java property assignment");
	return JS_FALSE;
    }

    str = JSVAL_TO_STRING(slot);
    name = JS_GetStringBytes(str);
    PR_LOG(MojaSrc, debug, ("looked up slot \"%s\"", name));

    fb = java_lookup_field(cx, cb, java->type == JAVA_CLASS, name);
    if (!fb) {
        JS_ReportError(cx, "no Java %sfield found with name %s",
                          (java->type == JAVA_CLASS ? "static " : ""),
			  name);
	return JS_FALSE;
    }

    if (!js_convertJSValueToJField(cx, *vp, java->handle, fb)) {
        JS_ReportError(cx, "can't set Java field %s", name);
	return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
java_enumerate_property(JSContext *cx, JSObject *obj, struct fieldblock *fb)
{
    JSJava *java = JS_GetPrivate(cx, obj);
    jsval junk;

    PR_ASSERT(java->type == JAVA_OBJECT || java->type == JAVA_CLASS);
    if (!(fb->access & ACC_PUBLIC) ||
	!CHECK_STATIC(java->type == JAVA_CLASS, fb)) {
	return JS_TRUE;
    }

    return JS_GetProperty(cx, obj, fieldname(fb), &junk);
/*  FIXME this is what i would prefer, but it seems the get is necessary
    return JS_DefineProperty(cx, obj, fieldname(fb), JSVAL_VOID,
			     0, 0, JSPROP_ENUMERATE);
 */
}

static JSBool
java_enumerate(JSContext *cx, JSObject *obj)
{
    JSJava *java = JS_GetPrivate(cx, obj);
    ClassClass *cb;

    if (!java)
	return JS_TRUE;

    cb = java->cb;

    while (cb) {
	int i;
	for (i = 0; i < (int)cbFieldsCount(cb); i++) {
	    struct fieldblock *fb, *outerfb;

	    fb = cbFields(cb) + i;
	    outerfb = java_lookup_name(cx, java->cb, java->type == JAVA_CLASS,
					fieldname(fb));
	    if (outerfb && outerfb != fb)
		continue;

	    if (!java_enumerate_property(cx, obj, fb))
		return JS_FALSE;
	}
	for (i = cbMethodsCount(cb); i--;) {
	    struct methodblock *mb;
	    struct fieldblock *outerfb;

	    mb = cbMethods(cb) + i;
	    outerfb = java_lookup_name(cx, java->cb, java->type == JAVA_CLASS,
				       fieldname(&mb->fb));
	    if (outerfb && outerfb != &mb->fb)
		continue;

	    if (!java_enumerate_property(cx, obj, &mb->fb))
		return JS_FALSE;
	}
	/* check the parent */
	if (cbSuperclass(cb))
	    cb = cbSuperclass(cb);
	else
	    cb = 0;
    }
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
java_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSJava *java = JS_GetPrivate(cx, obj);
    char *name;
    jsval v;
    JSErrorReporter oldReporter;
    JSBool hasProperty;

    if (!java || !JSVAL_IS_STRING(id))
	return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));
    PR_LOG(MojaSrc, debug, ("resolve(0x%x, %s) (handle 0x%x)",
			 obj, name, java->handle));

    oldReporter = JS_SetErrorReporter(cx, NULL);
    hasProperty = java_getProperty(cx, obj, id, &v);
    JS_SetErrorReporter(cx, oldReporter);
    if (!hasProperty)
        return JS_TRUE;

    return JS_DefineProperty(cx, obj, name, v, 0, 0, JSPROP_ENUMERATE);
}

PR_STATIC_CALLBACK(PRHashNumber)
java_hashHandle(void *key)
{
    PRHashNumber num = (PRHashNumber) key ;	/* help lame MSVC1.5 on Win16 */
    /* win16 compiler can't shift right by 2, but it will do it by 1 twice. */
    num = num >> 1;
    return num >> 1;
}

static int
java_pointerEq(void *v1, void *v2)
{
    return v1 == v2;
}

PR_STATIC_CALLBACK(void)
java_finalize(JSContext *cx, JSObject *obj)
{
    JSJava *java = JS_GetPrivate(cx, obj);
    void *key;
    PRHashEntry *he, **hep;

    if (!java)
	return;

    /* remove it from the reflection table */
    if (java->type == JAVA_CLASS) {
        PR_LOG(MojaSrc, debug, ("removing class 0x%x from table", java->cb));
	key = cbName(java->cb);
    } else {
        PR_LOG(MojaSrc, debug, ("removing handle 0x%x from table", java->handle));
	key = java->handle;
    }
    PR_EnterMonitor(javaReflectionsMonitor);
    if (javaReflections) {
	hep = PR_HashTableRawLookup(javaReflections,
                                    java_hashHandle(key), key);
	he = *hep;
	if (he) {
	    PR_HashTableRawRemove(javaReflections, hep, he);
	}
    }
    PR_ExitMonitor(javaReflectionsMonitor);

    /* get rid of the private data */
    JS_free(cx, java);
}

PR_STATIC_CALLBACK(JSBool)
java_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JSJava *java = JS_GetPrivate(cx, obj);

    if (!java) {
	if (type == JSTYPE_OBJECT) {
	    *vp = OBJECT_TO_JSVAL(obj);
	    return JS_TRUE;
	}

        JS_ReportError(cx, "illegal operation on Java prototype object");
	return JS_FALSE;
    }

    PR_LOG(MojaSrc, debug, ("java_convert to %d", type));

    switch (type) {
    case JSTYPE_OBJECT:
	*vp = OBJECT_TO_JSVAL(obj);
	break;
    case JSTYPE_FUNCTION:
	/* only classes convert to functions (constructors) */
	if (java->type != JAVA_CLASS) {
	    /* random java objects do not convert to functions */
            JS_ReportError(cx, "can't convert Java object to function");
	    return JS_FALSE;
	} else {
	    JSString *str;
	    JSFunction *fun;
	    JSObject *funobj;
	    JSObject *globalobj;
	    jsval javaPrototype;

            PR_LOG(MojaSrc, debug, ("making a constructor\n"));

	    str = JS_NewStringCopyZ(cx, cbName(java->cb));
	    if (!str)
		return JS_FALSE;

	    fun = JS_NewFunction(cx, js_javaConstructorWrapper, 0, 0, 0,
				 JS_GetStringBytes(str));

	    /* FIXME a private property of the function object points to the
	     * classblock: gc problem? */
	    funobj = JS_GetFunctionObject(fun);
            if (!JS_DefineProperty(cx, funobj, "#javaClass",
				   PRIVATE_TO_JSVAL(java->cb), 0, 0,
				   JSPROP_READONLY)) {
		return JS_FALSE;
	    }

	    /* set the prototype so that objects constructed with
             * "new" have the right JSClass */
	    if (!(globalobj = JS_GetGlobalObject(cx))
		|| !JS_GetProperty(cx, globalobj,
                                   "#javaPrototype", &javaPrototype)
		|| !JS_DefineProperty(cx, funobj, "prototype",
				      javaPrototype, 0, 0,
				      JSPROP_READONLY)) {
		return JS_FALSE;
	    }

	    *vp = OBJECT_TO_JSVAL(funobj);
	}
	break;
    case JSTYPE_STRING:
	/* either pull out the string or call toString */
	return js_convertJObjectToJSString(cx, java->handle,
					   java->type==JAVA_CLASS, vp);
    case JSTYPE_NUMBER:
	/* call doubleValue() */
	return js_convertJObjectToJSNumber(cx, java->handle,
					   java->type==JAVA_CLASS, vp);
    case JSTYPE_BOOLEAN:
	/* call booleanValue() */
	return js_convertJObjectToJSBoolean(cx, java->handle,
					    java->type==JAVA_CLASS, vp);
    default:
	break;
    }
    return JS_TRUE;
}

static JSClass java_class = {
    "Java",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    java_getProperty, java_setProperty, java_enumerate,
    java_resolve, java_convert, java_finalize
};

/****	****	****	****	****	****	****	****	****/

static JSBool
javaarray_getProperty(JSContext *cx, JSObject *obj, jsval slot, jsval *vp)
{
    JSJava *array = JS_GetPrivate(cx, obj);
    HObject *ho = array->handle;
    PRInt32 index;
    PRInt32 size;
    char *addr;

    if (JSVAL_IS_STRING(slot)) {
	char *name = JS_GetStringBytes(JSVAL_TO_STRING(slot));
        if (0 == strcmp(name, "length")) {
	    *vp = INT_TO_JSVAL((jsint)obj_length(ho));
	    return JS_TRUE;
	}
        JS_ReportError(cx, "invalid Java array element %s", name);
	return JS_FALSE;
    }

    if (!JSVAL_IS_INT(slot)) {
        JS_ReportError(cx, "invalid Java array index expression");
	return JS_FALSE;
    }
    index = JSVAL_TO_INT(slot);
    if (index < 0 || index >= (PRInt32) obj_length(ho)) {
        JS_ReportError(cx, "Java array index %d out of range", index);
	return JS_FALSE;
    }

    size = sizearray(obj_flags(ho), 1);
    addr = ((char*) unhand(ho)) + index * size;

    if (!js_convertJElementToJSValue(cx, addr, array->signature,
				     vp, JSTYPE_VOID)) {
	JS_ReportError(cx,
            "can't convert Java array element %d to JavaScript value", index);
	return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool
javaarray_setProperty(JSContext *cx, JSObject *obj, jsval slot, jsval *vp)
{
    JSJava *array = JS_GetPrivate(cx, obj);
    HObject *ho = array->handle;
    PRInt32 index;
    PRInt32 size;
    char *addr;

    if (JSVAL_IS_STRING(slot)) {
	char *name = JS_GetStringBytes(JSVAL_TO_STRING(slot));
        if (0 == strcmp(name, "length")) {
            JS_ReportError(cx, "can't set length of a Java array");
	    return JS_FALSE;
	}
        JS_ReportError(cx, "invalid assignment to Java array element %s", name);
	return JS_FALSE;
    }

    if (!JSVAL_IS_INT(slot)) {
        JS_ReportError(cx, "invalid Java array index expression");
	return JS_FALSE;
    }
    index = JSVAL_TO_INT(slot);
    if (index < 0 || index >= (PRInt32) obj_length(ho)) {
        JS_ReportError(cx, "Java array index %d out of range", index);
	return JS_FALSE;
    }

    size = sizearray(obj_flags(ho), 1);
    addr = ((char*) unhand(ho)) + index * size;

    if (!js_convertJSValueToJElement(cx, *vp, addr,
				     array->signature, array->cb, 0)) {
        JS_ReportError(cx, "invalid assignment to Java array element %d",
		       index);
	return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool
javaarray_enumerate(JSContext *cx, JSObject *obj)
{
    JSJava *array = JS_GetPrivate(cx, obj);
    HObject *ho = array->handle;
    PRUint32 i;
    jsval v;

    for (i = 0; i < obj_length(ho); i++) {
	PRInt32 size = sizearray(obj_flags(ho), 1);
	char *addr = ((char*) unhand(ho)) + i * size;

	if (!js_convertJElementToJSValue(cx, addr, array->signature,
					 &v, JSTYPE_VOID)) {
	    return JS_FALSE;
	}
	if (!JS_SetElement(cx, obj, (jsint) i, &v)) {
	    return JS_FALSE;
	}
    }
    return JS_TRUE;
}

static JSBool
javaarray_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSJava *array = JS_GetPrivate(cx, obj);
    HObject *ho = array->handle;
    jsint index;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    index = JSVAL_TO_INT(id);
    if (index < 0 || index >= (PRInt32) obj_length(ho))
	return JS_TRUE;

    return JS_DefineElement(cx, obj, index, JSVAL_VOID,
			    0, 0, JSPROP_ENUMERATE);
}

static void
javaarray_finalize(JSContext *cx, JSObject *obj)
{
    JSJava *array = JS_GetPrivate(cx, obj);

    /* get rid of the private data */
    /* array->signature is a static string */ ;
    array->signature = 0;

    /* remove it from the reflection table */
    java_finalize(cx, obj);
}

static JSBool
javaarray_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    switch (type) {
    case JSTYPE_OBJECT:
	*vp = OBJECT_TO_JSVAL(obj);
	break;
    case JSTYPE_STRING:
	/* FIXME how should arrays convert to strings? */
    default:
	break;
    }
    return JS_TRUE;
}


static JSClass javaarray_class = {
    "JavaArray",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    javaarray_getProperty, javaarray_setProperty, javaarray_enumerate,
    javaarray_resolve, javaarray_convert, javaarray_finalize
};

/****	****	****	****	****	****	****	****	****/

/* this lameness is brought to you by the decision
 * to use strings for signatures in the JDK */
static char *
getSignatureBase(char *sig)
{
    int len;
    char *ret;
    char end;

    switch (*sig) {
    case SIGNATURE_CLASS:
	end = SIGNATURE_ENDCLASS;
	break;
    case SIGNATURE_FUNC:
	end = SIGNATURE_ENDFUNC;
	break;
    default:
	return 0;
	break;
    }

    len = strchr(sig+1, end) - (sig+1);
    ret = malloc(len+1);
    if (!ret)
	return 0;
    strncpy(ret, sig+1, len);
    ret[len] = '\0';

    return ret;
}

static ClassClass *
getSignatureClass(JSContext *cx, char *sig, ClassClass *from)
{
    char *name;
    ClassClass *cb;

    if (sig[1] == '\0') {
        /* special hack: if the signature is "L" it means
	 * just use the from class */
	return from;
    }
    name = getSignatureBase(sig);
    if (!name) {
	JS_ReportOutOfMemory(cx);
	return 0;
    }
    cb = js_FindJavaClass(cx, name, from);
    free(name);
    return cb;
}

/* this doesn't need to run in a java env, but it may throw an
 * exception so we need a temporary one */
static JSBool
js_isSubclassOf(JSContext *cx, ClassClass *cb1, ClassClass *cb2)
{
    ExecEnv *ee = jsj_GetCurrentEE(cx);
    JSBool ret;

    /* FIXME what to do with the exception? */
    if (!ee) {
	return JS_FALSE;
    }

    exceptionClear(ee);
    ret = (JSBool) is_subclass_of(cb1, cb2, ee);
    if (exceptionOccurred(ee)) {
#ifdef DEBUG
	char *message;

	/* exceptionDescribe(ee); */
        /* FIXME this could fail: we don't check if it's throwable,
	 * but i assume that is_subclass_of is well-behaved */
	HString *hdetail =
	  unhand((Hjava_lang_Throwable *)
		 ee->exception.exc)->detailMessage;
	ClassClass *cb = obj_array_classblock(ee->exception.exc);

	/*
	 * FIXME JRI_GetStringPlatformChars and JRI_GetStringUTFChars both
	 * return a pointer into an otherwise unreferenced, unrooted Java
	 * array's body, without returning the Java array's handle.  This
	 * requires fairly conservative GC, which we have in NSPR1.0, but
	 * if we lose it, this code may break.
	 */
	message = (char *)
	    JRI_GetStringPlatformChars((JRIEnv *) ee,
				       (struct java_lang_String *) hdetail,
				       (const jbyte *) cx->charSetName,
				       (jint) cx->charSetNameLength);

	PR_LOG(MojaSrc, error,
               ("exception in is_subclass_of %s (\"%s\")",
		cbName(cb), message));
#endif
	exceptionClear(ee);
	return JS_FALSE;
    }
    return ret ? JS_TRUE : JS_FALSE;
}

static JSBool
js_convertJSValueToJSObject(JSContext *cx, JSObject *mo, ClassClass *paramcb,
			    JSBool checkOnly, HObject **objp)
{
    /* check if a JSObject would be an acceptable argument */
    if (!js_isSubclassOf(cx, JSObjectClassBlock, paramcb))
	return JS_FALSE;

    /* JSObject is ok, so convert mo to one.
     * a JS object which is not really a java object
     * converts to JSObject */
    if (!checkOnly) {
	*objp = js_ReflectJSObjectToJObject(cx, mo);
	if (!*objp)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
js_convertJSValueToJArray(HObject **objp, JSContext *cx,
			  jsval v, char *sig, ClassClass *fromclass,
			  JSBool checkOnly, int *cost)
{
    /* FIXME bump the cost counter when necessary */
    JSJava *java;
    HArrayOfObject *harr;
    ClassClass *acb;
    ClassClass *paramcb;
    JSObject *mo;
    char *elementsig;
    ClassClass *elementClazz;

    /* the only legal conversions are from null or from a java array */
    if (!JSVAL_IS_OBJECT(v))
	return JS_FALSE;

    /* can always pass null */
    mo = JSVAL_TO_OBJECT(v);
    if (!mo) {
	if (!checkOnly)
	    *objp = 0;
	return JS_TRUE;
    }

    /* otherwise, it must be a JS reflection of a java array */
    if (! JS_InstanceOf(cx, mo, &javaarray_class, 0))
	return JS_FALSE;

    java = JS_GetPrivate(cx, mo);

    sig++; /* skip to array element type */

    switch (*sig) {
    case SIGNATURE_CLASS:	/* object array */
	paramcb = getSignatureClass(cx, sig, fromclass);
        PR_LOG(MojaSrc, debug, ("desired array element signature \"%s\"(0x%x)",
			     sig, paramcb));
	if (!paramcb) {
	    PR_LOG(MojaSrc, warn,
                   ("couldn't find class for signature \"%s\"\n", sig));
	    return JS_FALSE;
	}
	harr = (HArrayOfObject *) java->handle;
        acb = (ClassClass *)(unhand(harr)->body[class_offset(harr)]);
	if (!acb) {
	    PR_LOG(MojaSrc, warn,
                   ("couldn't find class of array element\n"));
	    return JS_FALSE;
	}

	/* elements must convert */
	if (js_isSubclassOf(cx, acb, paramcb)) {
	    if (!checkOnly)
		*objp = (HObject *) harr;
	    return JS_TRUE;
	}
	break;
    case SIGNATURE_ARRAY:	/* nested array */
        /* FIXME nested arrays can't be supported, because the
	 * jdk runtime treats them all as flat
	 *
	 * This just in... Actually, you can get the dimensions of a
         * java array because they aren't flattened. */
	/* FIXME throw an exception */
	break;
    default:			/* primitive array */
	/* for any other array, the signature must match exactly */
	if (js_JArrayElementType(java->handle, &elementsig, &elementClazz)) {
	    if (elementsig[0] == sig[0]) {
		if (!checkOnly)
		    *objp = java->handle;
		return JS_TRUE;
	    }
	}
	break;
    }

    return JS_FALSE;
}

PR_IMPLEMENT(JSBool)
js_convertJSValueToJObject(HObject **objp, JSContext *cx,
			   jsval v, char *sig, ClassClass *fromclass,
			   JSBool checkOnly, int *cost)
{
    /* FIXME bump the cost counter when necessary */

    ClassClass *paramcb;

    paramcb = sig ? getSignatureClass(cx, sig, fromclass)
        : ObjectClassBlock;

    PR_LOG(MojaSrc, debug, ("desired argument class signature \"%s\"(0x%x)",
	    sig, paramcb));

    if (!paramcb) {
	PR_LOG(MojaSrc, warn,
               ("couldn't find class for signature \"%s\"", sig));
	return JS_FALSE;
    }

    /* JS wrappers around java objects do the same check
     * as the java compiler */
    /* FIXME except classes, which become JSObject (circular problem?) */
    if (JSVAL_IS_OBJECT(v)) {
	JSObject *mo = JSVAL_TO_OBJECT(v);
	/* null is always a valid object */
	if (!mo) {
	    if (!checkOnly)
		*objp = 0;
	    return JS_TRUE;
	}
	if (JS_TypeOfValue(cx, v) == JSTYPE_FUNCTION) {
	    if (js_convertJSValueToJSObject(cx, mo, paramcb, checkOnly, objp))
		return JS_TRUE;
	} else if (JS_InstanceOf(cx, mo, &java_class, 0) ||
		   JS_InstanceOf(cx, mo, &javaarray_class, 0)) {
	    JSJava *java = JS_GetPrivate(cx, mo);
	    HObject *ho = java->handle;
	    ClassClass *cb;

	    /* class reflections convert to JSObject or String only */
	    if (java->type == JAVA_CLASS) {
		if (js_convertJSValueToJSObject(cx, mo, paramcb, checkOnly,
						objp)) {
		    return JS_TRUE;
		}
	    } else {
		/* get the classblock for the java argument type */
		cb = obj_array_classblock(ho);
                PR_LOG(MojaSrc, debug, ("actual argument 0x%x, class 0x%x",
				     ho, cb));

		/* check against the expected class */
		if (js_isSubclassOf(cx, cb, paramcb)) {
		    if (!checkOnly)
			*objp = ho;
		    return JS_TRUE;
		}
	    }
	} else {
	    /* otherwise see if it will take a JSObject */
	    if (js_convertJSValueToJSObject(cx, mo, paramcb, checkOnly, objp))
		return JS_TRUE;
	}
    } else if (JSVAL_IS_NUMBER(v)) {	/* java.lang.Double */
	if (js_isSubclassOf(cx, DoubleClassBlock, paramcb)) {
	    /* Float is ok */
	    if (!checkOnly)
                *objp = js_ConstructJava(cx, "java/lang/Double",
                                            0, "(D)",
					    JSVAL_IS_INT(v)
					    ? (double) JSVAL_TO_INT(v)
					    : *JSVAL_TO_DOUBLE(v));
	    return JS_TRUE;
	}
    } else if (JSVAL_IS_BOOLEAN(v)) {  /* java.lang.Boolean */
	/* FIXME this should return Boolean.JS_TRUE or Boolean.FALSE
	 * instead of constructing a new one? */
	if (js_isSubclassOf(cx, BooleanClassBlock, paramcb)) {
	    if (!checkOnly)
                *objp = js_ConstructJava(cx, "java/lang/Boolean",
                                         0, "(Z)", (long) JSVAL_TO_BOOLEAN(v));
	    return JS_TRUE;
	}
    }

    /* last ditch attempt: is a String acceptable? */
    if (js_isSubclassOf(cx, StringClassBlock, paramcb)) {
	JSString *str;
	/* string is ok, convert to one */

	str = JS_ValueToString(cx, v);
	if (str) {
	    /* make a java String from str */
	    if (!checkOnly) {
		ExecEnv *ee = jsj_GetCurrentEE(cx);
		if (!ee)
		    return JS_FALSE;
		*objp = (JHandle *)
		  JRI_NewStringPlatform((JRIEnv *) ee,
					(const jbyte *) JS_GetStringBytes(str),
					(jint) JS_GetStringLength(str),
					(const jbyte *) cx->charSetName,
					(jint) cx->charSetNameLength);
	    }
	    return JS_TRUE;
	}
    }
    return JS_FALSE;
}

PR_IMPLEMENT(JSBool)
js_convertJObjectToJSValue(JSContext *cx, jsval *vp, HObject *ho)
{
    ExecEnv *ee;
    JRIEnv *env;
    JSObject *mo;

    ee = jsj_GetCurrentEE(cx);
    if (!ee) return JS_FALSE;

    env = (JRIEnv *) ee;

    if (!ho) {
	*vp = OBJECT_TO_JSVAL(0);
	return JS_TRUE;
    }

    /*
     * If it's a JSObject, pull out the original JS object when
     * it comes back into the JS world.
     */
    if (obj_array_classblock(ho) == JSObjectClassBlock) {
	jref jso = (jref) ho /* FIXME */;
	*vp = OBJECT_TO_JSVAL(
	    (JSObject *) JRI_GetFieldInt(env, jso, JSObjectInternalField));
	return JS_TRUE;
    }

    /*
     * Instances of java.lang.String are wrapped so we can
     * call methods on them, but they convert to a JS string
     * if used in a string context.
     */

    /* otherwise, wrap it */
    mo = js_ReflectJObjectToJSObject(cx, ho);
    if (!mo) {
        JS_ReportError(cx, "can't convert Java object to JavaScript object");
	return JS_FALSE;
    }
    *vp = OBJECT_TO_JSVAL(mo);
    return JS_TRUE;
}


#define REFLECTION_IN_PROGRESS ((jref)1)

PR_IMPLEMENT(HObject *)
js_ReflectJSObjectToJObject(JSContext *cx, JSObject *mo)
{
    ExecEnv *ee;
    JRIEnv *env;
    jref jso;
    PRHashEntry *he;

    ee = jsj_GetCurrentEE(cx);
    if (!ee) return 0;

    env = (JRIEnv *) ee;

    /* see if it's already there */
    PR_EnterMonitor(jsReflectionsMonitor);

    while ((jso = PR_HashTableLookup(jsReflections, mo)) != 0) {
	if (jso != REFLECTION_IN_PROGRESS)
	    break;
	PR_Wait(jsReflectionsMonitor, WAIT_FOREVER);
    }

    if (jso) {
	PR_ExitMonitor(jsReflectionsMonitor);
	return (HObject *) jso /*FIXME*/;
    }

    /*
     * Release the monitor temporarily while we call the java constructor,
     * which could contain arbitrary scariness.  Put a placeholder in the
     * hash table so anyone racing to reflect mo waits for us to finish.
     */
    PR_HashTableAdd(jsReflections, mo, REFLECTION_IN_PROGRESS);
    PR_ExitMonitor(jsReflectionsMonitor);

    /* not found, reflect it */
    jso = (jref) /*FIXME*/
        js_ConstructJavaPrivileged(cx, 0, JSObjectClassBlock, "()");

    /* FIXME check for exceptions */
    if (!jso) {
	return 0;
    }

    /* need to check again since we released the monitor */
    PR_EnterMonitor(jsReflectionsMonitor);

#ifdef DEBUG
{
    jref tmp = PR_HashTableLookup(jsReflections, mo);

    PR_ASSERT(!tmp || tmp == REFLECTION_IN_PROGRESS);
    if (tmp && tmp != REFLECTION_IN_PROGRESS) {
	PR_ExitMonitor(jsReflectionsMonitor);

	/* let the one we constructed die in gc and use the one we found */
	return (HObject *) tmp;
    }
}
#endif

    JRI_SetFieldInt(env, jso, JSObjectInternalField, (jint) mo);

    /* add it to the table */
    he = PR_HashTableAdd(jsReflections, mo, jso);
    if (he)
	(void) JS_AddRoot(cx, (void *)&he->key);	/* FIXME be errors? */

    /* awaken anyone racing and exit monitor */
    PR_NotifyAll(jsReflectionsMonitor);
    PR_ExitMonitor(jsReflectionsMonitor);

    return (HObject *) jso;/*FIXME*/
}

PR_IMPLEMENT(void)
js_RemoveReflection(JSContext *cx, JSObject *mo)
{
    PRHashEntry *he, **hep;

    /* remove it from the reflection table */
    PR_LOG(MojaSrc, debug, ("removing JS object 0x%x from table", mo));
    PR_EnterMonitor(jsReflectionsMonitor);
    hep = PR_HashTableRawLookup(jsReflections, java_hashHandle(mo), mo);
    he = *hep;

    if (he) {
	/* FIXME inline JS_RemoveRoot -- this will go away with GC unification */
	JS_LOCK_RUNTIME(finalizeRuntime);
	(void) PR_HashTableRemove(finalizeRuntime->gcRootsHash, (void *)&he->key);
	JS_UNLOCK_RUNTIME(finalizeRuntime);
    }
    PR_HashTableRawRemove(jsReflections, hep, he);
    PR_ExitMonitor(jsReflectionsMonitor);
}



static JSBool
js_convertJSValueToJValue(JSContext *cx, jsval v,
			  OBJECT *addr, char *sig, ClassClass *fromclass,
			  JSBool checkOnly, char **sigRestPtr,
			  int *cost)
{
    Java8 tdub;
    char *p = sig;

    switch (*p) {
    case SIGNATURE_BOOLEAN:
	if (!JSVAL_IS_BOOLEAN(v)) {
	    /* FIXME we could convert other things to boolean too,
	     * but until cost checking is done this will cause us
	     * to choose the wrong method if there is method overloading */
#ifndef USE_COSTS
	    return JS_FALSE;
#else
	    if (!JS_ConvertValue(cx, v, JSTYPE_BOOLEAN, &v))
		return JS_FALSE;
	    (*cost)++;
#endif
	}
	if (!checkOnly)
	    *(long*)addr = (JSVAL_TO_BOOLEAN(v) == JS_TRUE);
	break;
    case SIGNATURE_SHORT:
    case SIGNATURE_BYTE:
    case SIGNATURE_CHAR:
    case SIGNATURE_INT:
	/* FIXME should really do a range check... */
	if (!JSVAL_IS_NUMBER(v)) {
#ifndef USE_COSTS
	    return JS_FALSE;
#else
	    if (!JS_ConvertValue(cx, v, JSTYPE_NUMBER, &v))
		return JS_FALSE;
	    (*cost)++;
#endif
	}
	if (!checkOnly) {
	    if (JSVAL_IS_INT(v))
		*(long*)addr = (long) JSVAL_TO_INT(v);
	    else
		*(long*)addr = (long) *JSVAL_TO_DOUBLE(v);
	}
	break;

    case SIGNATURE_LONG:
	if (!JSVAL_IS_NUMBER(v)) {
#ifndef USE_COSTS
	    return JS_FALSE;
#else
	    if (!JS_ConvertValue(cx, v, JSTYPE_NUMBER, &v))
		return JS_FALSE;
	    (*cost)++;
#endif
	}
	if (!checkOnly) {
	    PRInt64 ll;
	    if (JSVAL_IS_INT(v)) {
		long l = (long) JSVAL_TO_INT(v);
		LL_I2L(ll, l);
	    } else {
		double d = (double) *JSVAL_TO_DOUBLE(v);
		LL_D2L(ll, d);
	    }
	    SET_INT64(tdub, addr, ll);
	}
	break;

    case SIGNATURE_FLOAT:
	if (!JSVAL_IS_NUMBER(v)) {
#ifndef USE_COSTS
	    return JS_FALSE;
#else
	    if (!JS_ConvertValue(cx, v, JSTYPE_NUMBER, &v))
		return JS_FALSE;
	    (*cost)++;
#endif
	}
	if (!checkOnly) {
	    if (JSVAL_IS_INT(v))
		*(float*)addr = (float) JSVAL_TO_INT(v);
	    else
		*(float*)addr = (float) *JSVAL_TO_DOUBLE(v);
	}
	break;

    case SIGNATURE_DOUBLE:
	if (!JSVAL_IS_NUMBER(v)) {
#ifndef USE_COSTS
	    return JS_FALSE;
#else
	    if (!JS_ConvertValue(cx, v, JSTYPE_NUMBER, &v))
		return JS_FALSE;
	    (*cost)++;
#endif
	}
	if (!checkOnly) {
	    double d;
	    if (JSVAL_IS_INT(v))
		d = (double) JSVAL_TO_INT(v);
	    else
		d = (double) *JSVAL_TO_DOUBLE(v);
	    SET_DOUBLE(tdub, addr, d);
	}
	break;

    case SIGNATURE_CLASS:
	/* FIXME cost? */
	if (!js_convertJSValueToJObject((HObject **)/*FIXME*/addr,
					cx, v, p, fromclass,
					checkOnly, cost)) {
	    return JS_FALSE;
	}
	while (*p != SIGNATURE_ENDCLASS) p++;
	break;

    case SIGNATURE_ARRAY:
	/* FIXME cost? */
	if (!js_convertJSValueToJArray((HObject **)/*FIXME*/addr,
				       cx, v, p, fromclass,
				       checkOnly, cost)) {
	    return JS_FALSE;
	}
	while (*p == SIGNATURE_ARRAY) p++; /* skip array beginning */
	/* skip the element type */
	if (*p == SIGNATURE_CLASS) {
	    while (*p != SIGNATURE_ENDCLASS) p++;
	}
	break;

    default:
        PR_LOG(MojaSrc, warn, ("unknown value signature '%s'", p));
	return JS_FALSE;
    }

    if (sigRestPtr)
	*sigRestPtr = p+1;
    return JS_TRUE;
}


/*
 *  this code mimics the method overloading resolution
 *  done in java, for use in JS->java calls
 */


/* push a jsval array onto the java stack for use by
 *  the given method.
 * sigRest gets a pointer to the remainder of the signature (the
 *  rest of the arguments in the list).
 * if checkOnly is true, don't actually convert, just check that it's ok.
 */
static JSBool
js_convertJSToJArgs(JSContext *cx, stack_item *optop,
		    struct methodblock *mb, int argc, jsval *argv,
		    JSBool checkOnly, char **sigRestPtr,
		    int *cost)
{
    struct fieldblock *fb = &mb->fb;
    char *sig = fieldsig(fb);
    jsval *vp = argv;
    int argsleft = argc;
    char *p;
    void *addr;

    *cost = 0;

    for (p = sig + 1; *p != SIGNATURE_ENDFUNC; vp++, argsleft--) {
	if (argsleft == 0)		/* not enough arguments passed */
	    return JS_FALSE;
	if (checkOnly)
	    addr = 0;
	else switch (*p) {
	case SIGNATURE_BOOLEAN:
	case SIGNATURE_SHORT:
	case SIGNATURE_BYTE:
	case SIGNATURE_CHAR:
	case SIGNATURE_INT:
	    addr = &(optop++)->i;
	    break;
	case SIGNATURE_LONG:
	    addr = optop;
	    optop += 2;
	    break;
	case SIGNATURE_FLOAT:
	    addr = &(optop++)->f;
	    break;
	case SIGNATURE_DOUBLE:
	    addr = optop;
	    optop += 2;
	    break;
	case SIGNATURE_CLASS:
	case SIGNATURE_ARRAY:
	    addr = &(optop++)->h;
	    break;
	default:
	    PR_LOG(MojaSrc, warn,
                   ("Invalid method signature '%s' for method '%s'\n",
		    fieldsig(fb), fieldname(fb)));
	    return JS_FALSE;
	    break;
	}
	/* this bumps p to the next argument */
	if (!js_convertJSValueToJValue(cx, *vp, addr, p, fieldclass(&mb->fb),
				       checkOnly, &p, cost)) {
	    return JS_FALSE;
	}
    }
    if (argsleft > 0) {
	/* too many arguments */
	return JS_FALSE;
    }

    if (sigRestPtr)
	*sigRestPtr = p+1 /* go past the SIGNATURE_ENDFUNC */;
    return JS_TRUE;
}

/* java array elements are packed with smaller sizes */
static JSBool
js_convertJSValueToJElement(JSContext *cx, jsval v,
			    char *addr, char *sig, ClassClass *fromclass,
			    char **sigRestPtr)
{
    long tmp[2];
    int cost = 0;

    if (!js_convertJSValueToJValue(cx, v, (OBJECT *)&tmp,
				   sig, fromclass,
				   JS_FALSE, sigRestPtr, &cost))
       return JS_FALSE;

    switch (sig[0]) {
    case SIGNATURE_BOOLEAN:
	*(char*)addr = (char)(*(long*)&tmp);
	break;
    case SIGNATURE_BYTE:
	*(char*)addr = (char)(*(long*)&tmp);
	break;
    case SIGNATURE_CHAR:
	*(unicode*)addr = (unicode)(*(long*)&tmp);
	break;
    case SIGNATURE_SHORT:
	*(signed short*)addr = (signed short)(*(long*)&tmp);
	break;
    case SIGNATURE_INT:
	*(PRInt32*)addr = *(long*)&tmp;
	break;
    case SIGNATURE_LONG:
	*(PRInt64*)addr = *(PRInt64*)&tmp;
	break;
    case SIGNATURE_FLOAT:
	*(float*)addr = *(float*)&tmp;
	break;
    case SIGNATURE_DOUBLE:
	*(double*)addr = *(double*)&tmp;
	break;
    case SIGNATURE_CLASS:
    case SIGNATURE_ARRAY:
	*(HObject**)addr = *(HObject**)&tmp;
	break;
    default:
        PR_LOG(MojaSrc, warn, ("unknown value signature '%s'\n", sig[0]));
	return JS_FALSE;
    }
    return JS_TRUE;
}


static OBJECT *
getJavaFieldAddress(HObject *ho, struct fieldblock *fb)
{
    OBJECT *addr = 0;			/* address of the java value */

    if (fb->access & ACC_PUBLIC) {
	if (fb->access & ACC_STATIC) {
	    char *sig = fieldsig(fb);

	    if (sig[0] == SIGNATURE_LONG || sig[0] == SIGNATURE_DOUBLE)
		addr = (long *)twoword_static_address(fb);
	    else
		addr = (long *)normal_static_address(fb);
	} else {
	    addr = &obj_getoffset(ho, fb->u.offset);
	}
    }
    return addr;
}

static JSBool
js_convertJSValueToJField(JSContext *cx, jsval v, HObject *ho,
			  struct fieldblock *fb)
{
    OBJECT *addr = getJavaFieldAddress(ho, fb);
    int cost = 0;

    if (!addr) {
        JS_ReportError(cx, "can't access field %s", fieldname(fb));
	return JS_FALSE;
    }
    return js_convertJSValueToJValue(cx, v, addr,
				     fieldsig(fb), fieldclass(fb),
				     JS_FALSE, 0, &cost);
}

/*
 * Returns -1 if the given method is not applicable to the arguments,
 * or a cost if it is.
 * if argc == -1, match iff the name matches and don't check the
 *  signature.	this always returns cost 1.
 */
static int
methodIsApplicable(JSContext *cx,
		   struct methodblock *mb, JSBool isStatic,
		   const char *name, int argc, jsval *argv)
{
    struct fieldblock *fb = &mb->fb;
    int cost = 0;

    /* name and access must match */
    if (!CHECK_STATIC(isStatic, fb) || strcmp(fieldname(fb), name))
	return -1;

    if (argc == -1)
	return 1;

    if (!js_convertJSToJArgs(cx, 0, mb, argc, argv,
			     JS_TRUE /* checkOnly */,
			     0 /* &sigRest */, &cost)) {
	return -1;
    }

    return cost;
}

/* FIXME this routine doesn't work yet - its purpose is to choose
 * the best method when java methods are overloaded, or to detect
 * ambiguous method calls */
static JSBool
methodIsMoreSpecific(JSContext *cx,
		     struct methodblock *mb1, struct methodblock *mb2)
{
    char *sig1 = fieldsig(&mb1->fb) + 1;        /* skip '(' */
    char *sig2 = fieldsig(&mb2->fb) + 1;

    /* FIXME fill this in! */
    return JS_TRUE;

    /* go through the args */
    while (*sig1 != SIGNATURE_ENDFUNC && *sig2 != SIGNATURE_ENDFUNC) {
	if (*sig1 == SIGNATURE_CLASS) {
	    if (*sig2 == SIGNATURE_CLASS) {
		ClassClass *cb1 =
		  getSignatureClass(cx, sig1, fieldclass(&mb1->fb));
		ClassClass *cb2 =
		  getSignatureClass(cx, sig2, fieldclass(&mb2->fb));

		if (! js_isSubclassOf(cx, cb1, cb2))
		    return JS_FALSE;

		/* next argument */
		while (*sig1++ != SIGNATURE_ENDCLASS);
		while (*sig2++ != SIGNATURE_ENDCLASS);
	    }
	}
    }
    if (*sig1 != *sig2) {
	return JS_FALSE;
	/* arg number mismatch */
    }
    return JS_TRUE;
}

/* based on sun.tools.java.Environment */
/**
 * Return true if an implicit cast from this type to
 * the given type is allowed.
 */

static JSBool
js_convertJValueToJSValue(JSContext *cx, OBJECT *addr, char *sig, jsval *vp,
			  JSType desired)
{
    Java8 tmp;

    switch (sig[0]) {
    case SIGNATURE_VOID:
	*vp = JSVAL_VOID;
	break;
    case SIGNATURE_BYTE:
    case SIGNATURE_CHAR:
    case SIGNATURE_SHORT:
	*vp = INT_TO_JSVAL((jsint) *addr);
	break;
    case SIGNATURE_INT:
	{
	    PRInt32 val = (PRInt32) *addr;
	    if (INT_FITS_IN_JSVAL(val)) {
		*vp = INT_TO_JSVAL((jsint) *addr);
	    } else {
		if (!JS_NewDoubleValue(cx, val, vp))
		    return JS_FALSE;
	    }
	}
	break;
    case SIGNATURE_BOOLEAN:
	*vp = BOOLEAN_TO_JSVAL((JSBool) *addr);
	break;
    case SIGNATURE_LONG:
	{
	    PRInt64 ll;
	    double d;

	    ll = GET_INT64(tmp, addr);
	    LL_L2D(d, ll);
	    if (!JS_NewDoubleValue(cx, d, vp))
		return JS_FALSE;
	}
	break;
    case SIGNATURE_FLOAT:
	if (!JS_NewDoubleValue(cx, *(float *)addr, vp))
	    return JS_FALSE;
	break;
    case SIGNATURE_DOUBLE:
	if (!JS_NewDoubleValue(cx, GET_DOUBLE(tmp, addr), vp))
	    return JS_FALSE;
	break;
    case SIGNATURE_CLASS:
    case SIGNATURE_ARRAY:
	return js_convertJObjectToJSValue(cx, vp, *(HObject **) addr);
    default:
        JS_ReportError(cx, "unknown Java signature character '%c'", sig[0]);
	return JS_FALSE;
    }

    if (desired != JSTYPE_VOID)
	return JS_ConvertValue(cx, *vp, desired, vp);
    return JS_TRUE;
}

/* java array elements are packed with smaller sizes */
static JSBool
js_convertJElementToJSValue(JSContext *cx, char *addr, char *sig, jsval *vp,
			    JSType desired)
{
    switch (sig[0]) {
    case SIGNATURE_BYTE:
	*vp = INT_TO_JSVAL(*(char*)addr);
	break;
    case SIGNATURE_CHAR:
	*vp = INT_TO_JSVAL(*(unicode*)addr);
	break;
    case SIGNATURE_SHORT:
	*vp = INT_TO_JSVAL(*(signed short*)addr);
	break;
    case SIGNATURE_INT:
	{
	    PRInt32 val = *(PRInt32*)addr;
	    if (INT_FITS_IN_JSVAL(val)) {
		*vp = INT_TO_JSVAL((jsint)val);
	    } else {
		if (!JS_NewDoubleValue(cx, val, vp))
		    return JS_FALSE;
	    }
	}
	break;
    case SIGNATURE_BOOLEAN:
	*vp = BOOLEAN_TO_JSVAL((JSBool) *(PRInt32*)addr);
	break;
    default:
	return js_convertJValueToJSValue(cx, (OBJECT *) addr, sig, vp, desired);
    }

    if (desired != JSTYPE_VOID)
	return JS_ConvertValue(cx, *vp, desired, vp);
    return JS_TRUE;
}

static JSBool
js_convertJFieldToJSValue(JSContext *cx, HObject *ho, struct fieldblock *fb,
			  jsval *vp, JSType desired)
{
    OBJECT *addr = getJavaFieldAddress(ho, fb);
    char *sig = fieldsig(fb);

    if (!addr) {
        JS_ReportError(cx, "can't access Java field %s", fieldname(fb));
	return JS_FALSE;
    }
    return js_convertJValueToJSValue(cx, addr, sig, vp, desired);
}

static JSBool
js_convertJObjectToJSString(JSContext *cx, HObject *ho, JSBool isClass,
			    jsval *vp)
{
    JSString *str;
    char *cstr;

    if (isClass) {
        cstr = PR_smprintf("[JavaClass %s]", cbName((ClassClass*)ho));
    } else {
	HString *hstr;
	ExecEnv *ee;

	if (!ho)
	    return JS_FALSE;

	if (obj_classblock(ho) == StringClassBlock) {
            /* it's a string already */
	    hstr = (HString*) ho;
	} else {
	    /* call toString() to convert to a string */
	    if (!js_ExecuteJavaMethod(cx, &hstr, sizeof(hstr), ho,
                                   "toString", "()Ljava/lang/String;",
				   0, JS_FALSE)) {
		return JS_FALSE;
	    }
	}

	/* FIXME js_ExecuteJavaMethod does this too */
	ee = jsj_GetCurrentEE(cx);
	if (!ee)
	    return JS_FALSE;

	/* convert the java string to a JS string */
	cstr = (char *)
	    JRI_GetStringPlatformChars((JRIEnv *) ee,
				       (struct java_lang_String *) hstr,
				       (const jbyte *) cx->charSetName,
				       (jint) cx->charSetNameLength);
	if (cstr)
	    cstr = strdup(cstr);
    }
    if (!cstr) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    str = JS_NewString(cx, cstr, strlen(cstr));
    if (!str) {
	free(cstr);
	return JS_FALSE;
    }
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
js_convertJObjectToJSNumber(JSContext *cx, HObject *ho, JSBool isClass,
			    jsval *vp)
{
    long foo[2], swap;	/* FIXME JRI and JDK disagree */
    JRI_JDK_Java8 tmp;
    double d;

    if (isClass || !ho) {
        JS_ReportError(cx, "can't convert Java object to number");
	return JS_FALSE;
    }

    if (!js_ExecuteJavaMethod(cx, foo, sizeof(foo), ho,
                                 "doubleValue", "()D",
				 0, JS_FALSE))
	return JS_FALSE;

    swap = foo[0];
    foo[0] = foo[1];
    foo[1] = swap;
    d = JRI_GET_DOUBLE(tmp, &foo[0]);
    return JS_NewDoubleValue(cx, d, vp);
}

static JSBool
js_convertJObjectToJSBoolean(JSContext *cx, HObject *ho, JSBool isClass,
			     jsval *vp)
{
    long b;

    if (isClass) {
        b = JS_TRUE;
	goto ok;
    }

    if (!ho) {
        b = JS_FALSE;
	goto ok;
    }

    if (!js_ExecuteJavaMethod(cx, &b, sizeof(b), ho,
                              "booleanValue", "()Z",
			      0, JS_FALSE)) {
        /* failed, probably missing the booleanValue() method */
        /* by default we convert to true */
        b = JS_TRUE;
    }
 ok:
    *vp = BOOLEAN_TO_JSVAL(b);
    return JS_TRUE;
}

static JSBool
java_returnAsJSValue(JSContext *cx, jsval *vp, ExecEnv *ee, char *sig)
{
    OBJECT *addr;

    if ((sig[0] == SIGNATURE_DOUBLE || sig[0] == SIGNATURE_LONG)) {
	addr = (OBJECT*) &ee->current_frame->optop[-2];
    } else {
	addr = (OBJECT *) &ee->current_frame->optop[-1];
    }
    return js_convertJValueToJSValue(cx, addr, sig, vp, JSTYPE_VOID);
}

static char*
js_createArgTypeString(JSContext* cx, int argc, jsval* argv)
{
    int i;
    int size = 0;
    char *p;
    char** strs;
    char *str;

    if (argc < 1) {
        str = strdup("");
	return str;
    }

    strs = (char**) malloc(argc * sizeof(char*));
    if (!strs)
	return 0;

    for (i = 0; i < argc; i++) {
	JSType t = JS_TypeOfValue(cx, argv[i]);
	switch (t) {
	  case JSTYPE_VOID:
            strs[i] = "void";
	    break;
	  case JSTYPE_OBJECT:
	    if (JSVAL_IS_NULL(argv[i]))
		strs[i] = "null";
	    else
		strs[i] = JS_GetClass(JSVAL_TO_OBJECT(argv[i]))->name;
	    break;
	  case JSTYPE_FUNCTION:
            strs[i] = "function";
	    break;
	  case JSTYPE_STRING:
            strs[i] = "string";
	    break;
	  case JSTYPE_NUMBER:
            strs[i] = "number";
	    break;
	  case JSTYPE_BOOLEAN:
            strs[i] = "boolean";
	    break;
	  default:
            PR_ASSERT(!"bad arg type");
	}
        size += strlen(strs[i]) + 2; /* +2 for ", " */
    }

    str = (char*) malloc(size - 1); /* -2 for ", " & +1 for '\0' */
    if (!str)
	goto out;

    strcpy(str, strs[0]);
    p = str + strlen(strs[0]);
    for (i = 1; i < argc; i++) {
        *p++ = ',';
        *p++ = ' ';
	strcpy(p, strs[i]);
	p += strlen(strs[i]);
    }
    *p = '\0';

  out:
    free(strs);
    return str;
}

/*
 * find the best method to call for a name and a bunch of JS
 * arguments.  try each possible method, then find the most
 * specific of the applicable methods.  "most specific" is
 * determined without reference to the JS arguments: it is
 * the same as "most specific" in the java sense of the word.
 * FIXME this could cause trouble since more methods will be applicable
 *  to a JS call than for a similar java call!
 * pass argc == -1 to match any method with the correct name
 */
static struct methodblock *
matchMethod(JSContext *cx, ClassClass *clazz, JSBool isStatic,
	    const char *name, int argc, jsval *argv, JSBool reportErrors)
{
    int mindex;
    int *isApplicable = 0;		/* array holding costs */
    struct methodblock *bestmb = 0;
    JSBool isOverloaded = JS_FALSE;
    ClassClass *cb = clazz;

    while (cb) {
	/*
	 * Find all applicable methods.  keep track of which ones are
	 * applicable using the isApplicable array, and the best one
	 * found so far in bestmb.  set isOverloaded if there is more
	 * than one applicable method.
	 */
	isApplicable = JS_malloc(cx, cbMethodsCount(cb) * sizeof(int));
	if (!isApplicable)
	    return 0;
	for (mindex = cbMethodsCount(cb); mindex--;) {
	    struct methodblock *mb = cbMethods(cb) + mindex;
	    struct fieldblock *fb = &mb->fb;

	    isApplicable[mindex] = methodIsApplicable(cx, mb, isStatic,
						      name, argc, argv);
	    if (isApplicable[mindex] == -1)
		continue;

            PR_LOG(MojaSrc, debug, ("found applicable method %s with sig %s",
				 fieldname(fb), fieldsig(fb)));

	    if (!bestmb) {		/* first one found */
		bestmb = mb;
		continue;
	    }

	    isOverloaded = JS_TRUE;;

	    if (methodIsMoreSpecific(cx, mb, bestmb)) {
		bestmb = mb;
	    }
	}

        /* if we've found something applicable in the current class,
	 * no need to go any further */
	/* this is the only exit from the loop in which isApplicable
	 * is live */
	if (bestmb)
	    break;

	/* otherwise, check the parent */
	if (cbSuperclass(cb))
	    cb = cbSuperclass(cb);
	else
	    cb = 0;
	JS_free(cx, isApplicable);
	isApplicable = 0;
    }

    /* second pass: check that bestmb is more specific than all other
     * applicable methods */
/* FIXME if mb is equally specific, this is an ambiguous lookup.
   Hopefully we can disambiguate by the cost */

/*
    if (isOverloaded)
	for (mindex = cb->methods_count; mindex--;) {
	    struct methodblock *mb = cbMethods(cb) + mindex;
	    struct fieldblock *fb = &mb->fb;

	    if (!isApplicable[mindex] || mb == bestmb)
		continue;

	}
*/

    if (isApplicable)
	JS_free(cx, isApplicable);

    if (bestmb && (bestmb->fb.access & ACC_PUBLIC))
	return bestmb;
    else if (reportErrors) {
	char buf[512];
	char *argstr = js_createArgTypeString(cx, argc, argv);
        char *methodtype = "method";

        if (0 == strcmp(name, "<init>")) {
            methodtype = "constructor";
	}

	if (!bestmb) {
	    if (argstr) {
		PR_snprintf(buf, sizeof(buf),
                            "no Java %s %s.%s matching JavaScript arguments (%s)",
			    methodtype, cbName(clazz), name, argstr);
		free(argstr);
	    } else {
		PR_snprintf(buf, sizeof(buf),
                            "no Java %s %s.%s matching JavaScript arguments",
			    methodtype, cbName(clazz), name);
	    }
	} else {
	    if (argstr) {
		PR_snprintf(buf, sizeof(buf),
                            "Java %s %s.%s is not public (%s)",
			    methodtype, cbName(clazz), name, argstr);
		free(argstr);
	    } else {
		PR_snprintf(buf, sizeof(buf),
                            "Java %s %s.%s is not public",
			    methodtype, cbName(clazz), name);
	    }
	}
	JS_ReportError(cx, buf);
    }
    return NULL;
}

/* this is the callback to execute java bytecodes in the
 * appropriate java env */
typedef struct {
    JRIEnv  *env;
    char    *pc;
    JSBool  ok;
} js_ExecuteJava_data;

static void
js_ExecuteJava_stub(void *d)
{
    js_ExecuteJava_data *data = d;
    PR_EXTERN(bool_t) ExecuteJava(unsigned char  *, ExecEnv *ee);

    data->ok = (JSBool) ExecuteJava((unsigned char*) data->pc, /*FIXME*/(ExecEnv*)data->env);
}

/* this is a copy of do_execute_java_method_vararg modified for
 *  a jsval* argument list instead of a va_list
 */
static JSBool
do_js_execute_java_method(JSContext *cx, void *obj,
			     struct methodblock *mb, JSBool isStaticCall,
			     int argc, jsval *argv,
			     jsval *rval)
{
    ExecEnv *ee = jsj_GetCurrentEE(cx);
    char *method_name;
    char *method_signature;
    JavaFrame *current_frame, *previous_frame;
    JavaStack *current_stack;
    char *sigRest;
    int cost = 0;
    stack_item *firstarg;
    JSBool success = JS_TRUE;
    JSJClassData *classData;

    unsigned char pc[6];
    cp_item_type  constant_pool[10];
    unsigned char cpt[10];
    JSBool ok;


    if (!ee) return JS_FALSE;

    /* push the safety frame before the call frame */
    if (!js_pushSafeFrame(cx, ee, &classData))
	return JS_FALSE;

    method_name = fieldname(&mb->fb);
    method_signature = fieldsig(&mb->fb);

    previous_frame = ee->current_frame;
    if (previous_frame == 0) {
	/* bottommost frame on this Exec Env. */
	current_stack = ee->initial_stack;
	current_frame = (JavaFrame *)(current_stack->data); /* no vars */
    } else {
	int args_size = mb->args_size;
	current_stack = previous_frame->javastack; /* assume same stack */
	if (previous_frame->current_method) {
	    int size = previous_frame->current_method->maxstack;
	    current_frame = (JavaFrame *)(&previous_frame->ostack[size]);
	} else {
            /* The only frames that don't have a mb are pseudo frames like
             * this one and they don't really touch their stack. */
	    current_frame = (JavaFrame *)(previous_frame->optop + 3);
	}
	if (current_frame->ostack + args_size > current_stack->end_data) {
            /* Ooops.  The current stack isn't big enough.  */
	    if (current_stack->next != 0) {
		current_stack = current_stack->next;
	    } else {
		current_stack = CreateNewJavaStack(ee, current_stack);
		if (current_stack == 0) {
		    JS_ReportOutOfMemory(cx);
		    success = JS_FALSE;
		    goto done;
		}
	    }
	    /* no vars */
	    current_frame = (JavaFrame *)(current_stack->data);
	}
    }
    current_frame->prev = previous_frame;
    current_frame->javastack = current_stack;
    current_frame->optop = current_frame->ostack;
    current_frame->vars = 0;	/* better not reference any! */
    current_frame->constant_pool = 0; /* Until we set it up */
    current_frame->monitor = 0; /* not monitoring anything */
    current_frame->annotation = 0;
    current_frame->current_method = 0;

    /*
     * Allocate space for all the operands before they are actually
     * converted, because conversion may need to use this stack.
     */
    firstarg = current_frame->optop;
    current_frame->optop += mb->args_size;

    ee->current_frame = current_frame;

    /* Push the target object, if not a static call. */
    if (!isStaticCall)
	(firstarg++)->p = obj;

    /* Now convert the args into the space on the stack. */
    if (!js_convertJSToJArgs(cx, firstarg, mb, argc, argv,
			     JS_FALSE /* actually convert */,
			     &sigRest, &cost)) {
        /* the method shouldn't have matched if this was going to happen! */
        JS_ReportError(cx, "internal error: argument conversion failed");
	success = JS_FALSE;
	goto done;
    }

    /* build the bytecodes and constant table for the call */
    constant_pool[CONSTANT_POOL_TYPE_TABLE_INDEX].p = cpt;
    cpt[0] = CONSTANT_POOL_ENTRY_RESOLVED;

    pc[0] = isStaticCall ? opc_invokestatic_quick
	: opc_invokenonvirtual_quick;
    pc[1] = 0; pc[2] = 1;	/* constant pool entry #1 */
    pc[3] = opc_return;

    constant_pool[1].p = mb;
    cpt[1] = CONSTANT_POOL_ENTRY_RESOLVED | CONSTANT_Methodref;

    current_frame->constant_pool = constant_pool;

    /* Run the byte codes in java-land catch any exceptions. */
    ee->exceptionKind = EXCKIND_NONE;

    {
	js_ExecuteJava_data data;
	data.pc = (char*) pc;
	js_CallJava(cx, js_ExecuteJava_stub, &data,
		       JS_FALSE /* we pushed the safety frame already */);
	ok = data.ok;
    }

    if (ok) {
	JSBool ret;
        PR_LOG(MojaSrc, debug, ("method call succeeded\n"));
	ret = java_returnAsJSValue(cx, rval, ee, sigRest);
	if (!ret) {
	    JS_ReportError(cx,
                           "can't convert Java return value with signature %s",
			   sigRest);
	    success = JS_FALSE;
	}
    } else {
	success = JS_FALSE;
    }

  done:
    /* Our caller can look at ee->exceptionKind and ee->exception. */
    ee->current_frame = previous_frame;
    /* pop the safety frame */
    js_popSafeFrame(cx, ee, classData);

    /* FIXMEold cx->javaEnv = saved; */

    return success;
}

static JSBool
js_javaMethodWrapper(JSContext *cx, JSObject *obj,
		     PRUintn argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    const char *name;
    JSJava *java;
    ClassClass *cb;
    struct methodblock *realmb;
    JSBool success;

    PR_ASSERT(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION);
    if (!JS_InstanceOf(cx, obj, &java_class, argv))
	return JS_FALSE;

    fun = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[-2]));
    name = JS_GetFunctionName(fun);

    java = JS_GetPrivate(cx, obj);
    cb = java->cb;

    PR_LOG(MojaSrc, debug, ("entered methodwrap, fun=0x%x, name=\"%s\"(0x%x)",
			 fun, name, name));

    /* match argc,argv against the signatures of the java methods */
    realmb = matchMethod(cx, cb, java->type==JAVA_CLASS, name, argc, argv,
			 JS_TRUE /* reportErrors */);

    if (!realmb) {
	return JS_FALSE;
    }

    PR_LOG(MojaSrc, debug, ("calling %sjava method %s with signature %s",
                         realmb->fb.access & ACC_STATIC ? "static " : "",
			 realmb->fb.name, realmb->fb.signature));

    success =
      do_js_execute_java_method(cx,
				realmb->fb.access & ACC_STATIC ? 0 : java->handle,
				realmb,
				realmb->fb.access & ACC_STATIC /* isStaticCall */,
				argc, argv, rval);

    return success;
}


static JSBool
js_javaConstructorWrapper(JSContext *cx, JSObject *obj,
			  PRUintn argc, jsval *argv,
			  jsval *rval)
{
    JSObject *funobj;
    ClassClass *cb;
    struct methodblock *realmb;
    HObject *ho;
    JSBool success;
    jsval tmp;
    ExecEnv *ee;

    PR_LOG(MojaSrc, debug, ("entered java constructor wrapper\n"));

    PR_ASSERT(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION);
    funobj = JSVAL_TO_OBJECT(argv[-2]);
    if (!JS_GetProperty(cx, funobj, "#javaClass", &tmp))
	return JS_FALSE;
    cb = JSVAL_TO_PRIVATE(tmp);

    if (!JS_InstanceOf(cx, obj, &java_class, 0)
	|| JS_GetPrivate(cx, obj)) {
	/* not a fresh object! */
        /* 3.0 allowed you to call a java constructor without using
         * "new" so we do the same.  setting obj to null causes
         * ReflectJava to create a new js object rather than using
         * the "this" argument of the constructor */
        obj = NULL;
    }

    /* FIXME these are copied from interpreter.c without much understanding */
    if (cbAccess(cb) & (ACC_INTERFACE | ACC_ABSTRACT)) {
        JS_ReportError(cx, "can't instantiate Java class");
	return JS_FALSE;
    }
    if (!VerifyClassAccess(0, cb, FALSE)) {
        JS_ReportError(cx, "can't access Java class");
	return JS_FALSE;
    }

    /* match argc,argv against the signatures of the java constructors */
    realmb = matchMethod(cx, cb, JS_FALSE, "<init>", argc, argv,
			 JS_TRUE /* reportErrors */);
    if (!realmb) {
	return JS_FALSE;
    }

    if (!VerifyFieldAccess(0, fieldclass(&realmb->fb),
			   realmb->fb.access, FALSE)) {
        JS_ReportError(cx, "illegal access to Java constructor");
	return JS_FALSE;
    }

    PR_LOG(MojaSrc, debug, ("calling java constructor with signature %s",
	    realmb->fb.signature));

    /*
     * Because newobject can fail, and call SignalError, which calls
     * FindClassFromClass, etc.
     */
    /* Allocate the object */
    ee = jsj_GetCurrentEE(cx);
    if (!ee)
	return JS_FALSE;
    if ((ho = newobject(cb, 0, ee)) == 0) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }

    success = do_js_execute_java_method(cx, ho, realmb,
					JS_FALSE /* isStaticCall */,
					argc, argv,
					&tmp  /* gets set to void */);

    /* FIXME the interpreter.c code calls cbDecRef(cb) at the end,
     * why?  should we? */

    if (success) {
	JSObject *jso = js_ReflectJava(cx, JAVA_OBJECT, ho, cb, 0, obj);
	if (jso) {
	    *rval = OBJECT_TO_JSVAL(jso);
	    return JS_TRUE;
	}
    }
    return JS_FALSE;
}

/*   *	 *   *	 *   *	 *   *	 *   *	 *   *	 *   *	 *   *	 *   *	 *
 *
 *   JS/java reflection tables
 *
 *   *	 *   *	 *   *	 *   *	 *   *	 *   *	 *   *	 *   *	 *   *	 */

typedef struct ScanArgs {
    GCInfo    *gcInfo;
} ScanArgs;

PR_STATIC_CALLBACK (int)
scanJSJavaReflectionEntry(PRHashEntry *he, int i, void *arg)
{
    ScanArgs *args = arg;
    JSObject *jso = he->value;

    /* scan the handle */
    if (1 /* jso != JSO_REFLECTION_IN_PROGRESS */) {
	JSJava *java = JSVAL_TO_PRIVATE(OBJ_GET_SLOT(jso, JSSLOT_PRIVATE));

	args->gcInfo->livePointer((void *)java->handle);
    }

    return HT_ENUMERATE_NEXT;
}

PR_STATIC_CALLBACK (void)
scanJSJavaReflections(void *runtime)
{
    ScanArgs args;

/*    PR_PROCESS_ROOT_LOG(("Scanning JS reflections of java objects")); */

    args.gcInfo = PR_GetGCInfo();

    /* FIXME this is kind of scary long-term access inside the
     * monitor - is there any alternative? */
#ifndef NSPR20
    PR_EnterMonitor(javaReflectionsMonitor);
#endif
    if (javaReflections) {
	PR_HashTableEnumerateEntries(javaReflections,
				     scanJSJavaReflectionEntry,
				     &args);
    }
#ifndef NSPR20
    PR_ExitMonitor(javaReflectionsMonitor);
#endif
}

#ifdef NSPR20
void PR_CALLBACK PrepareJSLocksForGC(GCLockHookArg arg1)
{
    PR_ASSERT(arg1 == PR_GCBEGIN || arg1 == PR_GCEND);

    if (arg1 == PR_GCBEGIN)
        PR_EnterMonitor(javaReflectionsMonitor);
    else
        PR_ExitMonitor(javaReflectionsMonitor);
}
#endif

static JSObject *
js_ReflectJava(JSContext *cx, JSJavaType type, HObject *handle,
	       ClassClass *cb, char *sig, JSObject *useMe)
{
    JSObject *mo;
    JSJava *java;
    void *key;
    PRHashEntry *he;

    PR_EnterMonitor(javaReflectionsMonitor);
    if (!javaReflections) goto fail;

    /* see if it's already been reflected */
    switch (type) {
      case JAVA_CLASS:
	key = (void *) cbName(cb);
	break;
      case JAVA_OBJECT:
	key = (void *) handle;
	break;
      case JAVA_ARRAY:
	key = (void *) handle;
	break;
      default:
	break;
    }

    mo = PR_HashTableLookup(javaReflections, key);
    if (mo)
	goto succeed;

    /* if we got this far, it isn't in the hash table.
     * if useMe is non-null, we were called via "new" and should
     * use the object that was already constructed if possible */
    if (useMe) {
	/* make sure we have the right js class and no private
	 * data yet.  if not, ignore the provided object */
	if (JS_GetPrivate(cx, useMe))
	    useMe = NULL;
	else {
	    JSClass *clasp = JS_GetClass(useMe);
	    switch (type) {
	      case JAVA_CLASS:
	      case JAVA_OBJECT:
		if (clasp != &java_class)
		    useMe = NULL;
		break;
	      case JAVA_ARRAY:
		if (clasp != &javaarray_class)
		    useMe = NULL;
		break;
	    }
	}
    }
    mo = useMe;

    /* build the private data for the js reflection */
    java = JS_malloc(cx, sizeof(JSJava));
    if (!java)
	goto fail;
    java->type = type;
    java->handle = handle;
    java->cb = cb;
    java->signature = sig;

    switch (type) {
      case JAVA_CLASS:
      case JAVA_OBJECT:
	if (!mo)
	    mo = JS_NewObject(cx, &java_class, 0, 0);
	if (!mo || ! JS_SetPrivate(cx, mo, java))
	    goto fail_free_java;
	break;
      case JAVA_ARRAY:
	if (!mo)
	    mo = JS_NewObject(cx, &javaarray_class, 0, 0);
	if (!mo || ! JS_SetPrivate(cx, mo, java))
	    goto fail_free_java;
	if (! JS_DefineProperty(cx, mo, "length",
				   INT_TO_JSVAL((jsint)obj_length(handle)),
				   0, 0, JSPROP_READONLY))
	    goto fail;
	break;
      default:
	break;
    }

    /* add it to the table */
    he = PR_HashTableAdd(javaReflections, key, mo);
    if (!he) {
	JS_ReportOutOfMemory(cx);
	mo = 0;
    }

  succeed:
    PR_ExitMonitor(javaReflectionsMonitor);
    return mo;

  fail_free_java:
    JS_free(cx, java);
  fail:
    PR_ExitMonitor(javaReflectionsMonitor);
    return 0;
}

/*
 * Get the element type for a java array.  *classp will be set to null
 * if it's not of object type.
 */
static JSBool
js_JArrayElementType(HObject *handle, char **sig, ClassClass **classp)
{
    /* figure out the signature from the type */
    unsigned long elementtype = obj_flags(handle);
    char *elementsig;

    *sig = 0;
    *classp = 0;

    switch (elementtype) {
      case T_CLASS:
	*classp = (ClassClass*)
          unhand((HArrayOfObject *)handle)->body[class_offset(handle)];
	elementsig = SIGNATURE_CLASS_STRING;
	break;
      case T_BOOLEAN:
	elementsig = SIGNATURE_BOOLEAN_STRING;
	break;
      case T_CHAR:
	elementsig = SIGNATURE_CHAR_STRING;
	break;
      case T_FLOAT:
	elementsig = SIGNATURE_FLOAT_STRING;
	break;
      case T_DOUBLE:
	elementsig = SIGNATURE_DOUBLE_STRING;
	break;
      case T_BYTE:
	elementsig = SIGNATURE_BYTE_STRING;
	break;
      case T_SHORT:
	elementsig = SIGNATURE_SHORT_STRING;
	break;
      case T_INT:
	elementsig = SIGNATURE_INT_STRING;
	break;
      case T_LONG:
	elementsig = SIGNATURE_LONG_STRING;
	break;
      default:
	PR_ASSERT(0);
	return JS_FALSE;
    }

    *sig = elementsig;
    return JS_TRUE;
}

PR_IMPLEMENT(JSObject *)
js_ReflectJObjectToJSObject(JSContext *cx, HObject *handle)
{
    JSObject *mo = 0;
    ExecEnv *ee;

    /* force initialization */
    ee = jsj_GetCurrentEE(cx);
    if (!ee) return 0;

    if (handle) {
	if (obj_flags(handle)) {
	    char *elementSig;
	    ClassClass *elementClazz;

	    if (!js_JArrayElementType(handle, &elementSig, &elementClazz)) {
		/* FIXMEbe report an error! */
		return 0;
	    }

	    mo = js_ReflectJava(cx, JAVA_ARRAY, handle,
				elementClazz, elementSig, 0);
            PR_LOG(MojaSrc, debug, ("reflected array[%s] 0x%x as JSObject* 0x%x",
				 elementSig, handle, mo));

	} else {
	    mo = js_ReflectJava(cx, JAVA_OBJECT, handle,
				obj_classblock(handle), 0, 0);
            PR_LOG(MojaSrc, debug, ("reflected HObject* 0x%x as JSObject* 0x%x",
				 handle, mo));
	}
    }

    return mo;
}

JSObject *
js_ReflectJClassToJSObject(JSContext *cx, ClassClass *cb)
{
    ExecEnv *ee;
    JSObject *mo;

    /* force initialization */
    ee = jsj_GetCurrentEE(cx);
    if (!ee) return 0;

    mo = js_ReflectJava(cx, JAVA_CLASS, (HObject *) cbHandle(cb), cb, 0, 0);

    PR_LOG(MojaSrc, debug, ("reflected ClassClass* 0x%x as JSObject* 0x%x",
			 cb, mo));

    return mo;
}

/*	*	*	*	*	*	*	*	*	*/

/*
 * JSJavaSlot is a java slot which will be resolved as a method
 * or a field depending on context.
 */
struct JSJavaSlot {
    JSObject		*obj;		/* the object or class reflection */
    jsval		value;		/* the field value when created */
    JSString		*name;		/* name of the field or method */
    struct fieldblock	*fb;		/* fieldblock if there is a field */
};

/* none of these should ever be called, since javaslot_convert will
 * first turn the slot into the underlying object if there is one */
PR_STATIC_CALLBACK(JSBool)
javaslot_getProperty(JSContext *cx, JSObject *obj, jsval slot,
		      jsval *vp)
{
    /* evil so that the assign hack doesn't kill us */
    if (slot == STRING_TO_JSVAL(ATOM_TO_STRING(cx->runtime->atomState.assignAtom))) {
	*vp = JSVAL_VOID;
	return JS_TRUE;
    }

    JS_ReportError(cx, "Java slots have no properties");
    return JS_FALSE;
}

PR_STATIC_CALLBACK(JSBool)
javaslot_setProperty(JSContext *cx, JSObject *obj, jsval slot,
		 jsval *vp)
{
    JS_ReportError(cx, "Java slots have no properties");
    return JS_FALSE;
}

PR_STATIC_CALLBACK(JSBool)
javaslot_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
javaslot_finalize(JSContext *cx, JSObject *obj)
{
    JSJavaSlot *slot = JS_GetPrivate(cx, obj);

    /* the value may be holding a reference to something... */
    JS_RemoveRoot(cx, &slot->value);

    /* drop the object of which this is a slot */
    JS_RemoveRoot(cx, &slot->obj);

    /* drop the name */
    JS_UnlockGCThing(cx, slot->name);

    JS_free(cx, slot);

    PR_LOG(MojaSrc, debug, ("finalizing JSJavaSlot 0x%x", obj));
}

PR_STATIC_CALLBACK(JSBool)
javaslot_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JSJavaSlot *slot = JS_GetPrivate(cx, obj);
    const char *name;
    JSFunction *fun;
    JSString *str;

    name = JS_GetStringBytes(slot->name);
    switch (type) {
      case JSTYPE_FUNCTION:
	fun = JS_NewFunction(cx, js_javaMethodWrapper, 0, JSFUN_BOUND_METHOD,
			     slot->obj, name);
	if (!fun)
	    return JS_FALSE;

	PR_LOG(MojaSrc, debug,
               ("converted slot to function 0x%x with name %s\n",
		fun, name));

	*vp = OBJECT_TO_JSVAL(JS_GetFunctionObject(fun));
	return JS_TRUE;

      case JSTYPE_OBJECT:
        PR_LOG(MojaSrc, debug, ("converting java slot 0x%x to object", obj));
	/* FALL THROUGH */
      case JSTYPE_NUMBER:
      case JSTYPE_BOOLEAN:
      case JSTYPE_STRING:
	if (slot->fb) {
	    return JS_ConvertValue(cx, slot->value, type, vp);
	}
	if (type == JSTYPE_STRING) {
	    JSJava *java = JS_GetPrivate(cx, slot->obj);
	    char *cstr, *cp;

	    PR_ASSERT(java->type == JAVA_OBJECT || java->type == JAVA_CLASS);
            cstr = PR_smprintf("[JavaMethod %s.%s]", cbName(java->cb), name);
	    if (!cstr) {
		JS_ReportOutOfMemory(cx);
		return JS_FALSE;
	    }
            for (cp = cstr; *cp != '\0'; cp++)
                if (*cp == '/')
                    *cp = '.';
	    str = JS_NewString(cx, cstr, strlen(cstr));
	    if (!str) {
                free(cstr);
		return JS_FALSE;
	    }
	    *vp = STRING_TO_JSVAL(str);
	    return JS_TRUE;
	}
	if (type != JSTYPE_OBJECT) {
            JS_ReportError(cx, "no field with name \"%s\"", name);
	    return JS_FALSE;
	}
	*vp = OBJECT_TO_JSVAL(obj);
	break;
      default:
	break;
    }
    return JS_TRUE;
}

static JSClass javaslot_class = {
    "JavaSlot",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    javaslot_getProperty, javaslot_setProperty, JS_EnumerateStub,
    javaslot_resolve, javaslot_convert, javaslot_finalize
};

static JSBool
js_reflectJavaSlot(JSContext *cx, JSObject *obj, JSString *str,
		   jsval *vp)
{
    JSJava *java = JS_GetPrivate(cx, obj);
    ClassClass *cb = java->cb;
    struct fieldblock *fb;
    struct methodblock *mb;
    JSJavaSlot *slot;
    JSObject *mo;
    const char *name = JS_GetStringBytes(str);

    *vp = JSVAL_VOID;

    /* PR_ASSERT(obj->clazz == &java_class); */

    /* if there's a field get its value at reflection time */
    fb = java_lookup_field(cx, cb, java->type == JAVA_CLASS, name);
    if (fb) {
	if (!js_convertJFieldToJSValue(cx, java->handle, fb,
				       vp, JSTYPE_VOID)) {
	    /*
             * If this happens, the field had a value that couldn't
	     * be represented in JS.
	     */
            PR_LOG(MojaSrc, error, ("looking up initial field value failed!"));

	    /* FIXME should really set a flag that will cause an error
	     * only if the slot is accessed as a field.  for now we
             * make it look like there wasn't any field by that name,
	     * which is less informative */
	    fb = 0;
	}
    }

#ifndef REFLECT_ALL_SLOTS_LAZILY
    /* match the name against the signatures of the java methods */
    mb = matchMethod(cx, cb, java->type==JAVA_CLASS, name, -1, 0,
		     JS_FALSE /* reportErrors */);

    if (!fb) {
	JSFunction *fun;

	if (!mb) {
	    /* nothing by that name, report an error */
            JS_ReportError(cx, "Java object has no field or method named %s",
			   name);
	    return JS_FALSE;
	}

        /* if we get here, there's a method but no field by this name */
	fun = JS_NewFunction(cx, js_javaMethodWrapper, 0,
			     JSFUN_BOUND_METHOD, obj, name);
	if (!fun)
	    return JS_FALSE;

	PR_LOG(MojaSrc, debug,
               ("eagerly converted slot to function 0x%x with name %s(0x%x)\n",
		fun, name, name));

	*vp = OBJECT_TO_JSVAL(JS_GetFunctionObject(fun));
	return JS_TRUE;
    } else if (!mb) {
        /* there's a field but no method by this name */
	/* looking up the field already set *vp */
	return JS_TRUE;
    }

    /* if we get here there are both fields and methods by this name,
     * so we create a slot object to delay the binding */
#endif

    slot = (JSJavaSlot *) JS_malloc(cx, sizeof(JSJavaSlot));
    if (!slot) {
	return JS_FALSE;
    }

    /* corresponding removes and unlocks are in javaslot_finalize */
    slot->obj = obj;
    if (!JS_AddRoot(cx, &slot->obj)) {
	JS_free(cx, slot);
	return JS_FALSE;
    }
    slot->value = *vp;
    if (!JS_AddRoot(cx, &slot->value)) {
	JS_RemoveRoot(cx, &slot->obj);
	JS_free(cx, slot);
	return JS_FALSE;
    }

    /* FIXME check return value? */
    JS_LockGCThing(cx, str);
    slot->name = str;
    slot->fb = fb;

    mo = JS_NewObject(cx, &javaslot_class, 0, 0);
    JS_SetPrivate(cx, mo, slot);

    PR_LOG(MojaSrc, debug, ("reflected slot %s of 0x%x as 0x%x",
	    name, java->handle, mo));
    if (!mo) {
	JS_RemoveRoot(cx, &slot->obj);
	JS_RemoveRoot(cx, &slot->value);
	JS_UnlockGCThing(cx, slot->name);
	JS_free(cx, slot);
	return JS_FALSE;
    }
    *vp = OBJECT_TO_JSVAL(mo);
    return JS_TRUE;
}

/***********************************************************************/

/* a saved JS error state */
typedef struct SavedJSError SavedJSError;
struct SavedJSError {
    char *message;
    JSErrorReport report;
    SavedJSError *next;
};

/*
 * capture a JS error that occurred in JS code called by java.
 * makes a copy of the JS error data and hangs it off the JS
 * environment.  when the JS code returns, this is checked and
 * used to generate a JSException.  if the JSException is uncaught
 * and makes it up to another layer of JS, the error will be
 * reinstated with JS_ReportError
 */
PR_IMPLEMENT(void)
js_JavaErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    /* save the error state */
    SavedJSError *newerr;
    newerr = malloc(sizeof(SavedJSError));
    if (!newerr) {
	/* FIXME not much we can do here, abort? */
	return;
    }
    newerr->message = 0;
    if (message) {
	newerr->message = strdup(message);
	if (!newerr->message) {
	    /* FIXME not much we can do here, abort? */
	    free(newerr);
	    return;
	}
    }
    newerr->report.filename = 0;
    newerr->report.lineno = 0;
    newerr->report.linebuf = 0;
    newerr->report.tokenptr = 0;

    if (report) {
	if (report->filename) {
	    newerr->report.filename = strdup(report->filename);
	    if (!newerr->report.filename) {
		/* FIXME not much we can do here, abort? */
		free(newerr->message);
		free(newerr);
		return;
	    }
	}
	newerr->report.lineno = report->lineno;

	if (report->linebuf) {
	    newerr->report.linebuf = strdup(report->linebuf);
	    if (!newerr->report.linebuf) {
		/* FIXME not much we can do here, abort? */
		free((void*)newerr->report.filename);
		free(newerr->message);
		free(newerr);
		return;
	    }
	    newerr->report.tokenptr = newerr->report.linebuf +
				      (report->tokenptr - report->linebuf);
	}
    }

    /* push this error */
    newerr->next = cx->savedErrors;
    cx->savedErrors = newerr;
}

static SavedJSError *
js_js_FreeError(SavedJSError *err)
{
    SavedJSError *next = err->next;

    free(err->message);
    free((char*)err->report.filename);/*FIXME*/
    free((char*)err->report.linebuf);
    free(err);

    return next;
}

PR_IMPLEMENT(void)
jsj_ClearSavedErrors(JSContext *cx)
{
    while (cx->savedErrors)
        cx->savedErrors = js_js_FreeError(cx->savedErrors);
}

/* this is called upon returning from JS to java.  one possibility
 * is that the JS error was actually triggered by java at some point -
 * if so we throw the original java exception.	otherwise, each JS
 * error will have pushed something on JSContext->savedErrors, so
 * we convert them all to a string and throw a JSException with that
 * info.
 */
PR_IMPLEMENT(void)
js_JSErrorToJException(JSContext *cx, ExecEnv *ee)
{
    SavedJSError *err = 0;

    if (!cx->savedErrors) {
	exceptionClear(ee);
	PR_LOG(MojaSrc, debug,
               ("j-m succeeded with no exception cx=0x%x ee=0x%x", cx, ee));
	return;
    }

    /*
     * If there's a pending exception in the java env, assume it
     * needs to be propagated (since JS couldn't have caught it
     * and done something with it).
     */
    if (exceptionOccurred(ee)) {
	PR_LOG(MojaSrc, debug,
               ("j-m propagated exception through JS cx=0x%x ee=0x%x",
		cx, ee));
	return; 	/* propagating is easy! */
    }

    /* otherwise, throw a JSException */
    /* get the message from the deepest saved JS error */
    err = cx->savedErrors;
    if (err) {
	while (err->next)
	    err = err->next;
    }

    /* propagate any pending JS errors upward with a java exception */
    {
	JRIEnv *env = (JRIEnv*) ee;
	struct java_lang_String* message =
	  JRI_NewStringUTF(env, err->message,
			   strlen(err->message));
	struct java_lang_String* filename = err->report.filename
	  ? JRI_NewStringUTF(env, err->report.filename,
			      strlen(err->report.filename))
	  : NULL;
	int lineno = err->report.lineno;
	struct java_lang_String* source = err->report.linebuf
	  ? JRI_NewStringUTF(env, err->report.linebuf,
			      strlen(err->report.linebuf))
	  : NULL;
	int index = err->report.linebuf
	  ? err->report.tokenptr - err->report.linebuf
	  : 0;
	jref exc = (jref)
	 execute_java_constructor((ExecEnv *)env,
				  NULL, JSExceptionClassBlock,
                                  "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;I)",
				  message, filename, (int32_t)lineno, source, (int32_t)index);

	exceptionThrow(ee, (HObject *)exc);
	PR_LOG(MojaSrc, debug,
               ("j-m raised JSException \"%s\" cx=0x%x ee=0x%x",
		err->message, cx, ee));
    }
}

static JSBool
js_isJSException(ExecEnv *ee, HObject *exc)
{
    return strcmp(cbName(obj_array_classblock(exc)),
                  "netscape/javascript/JSException")
      ? JS_TRUE : JS_FALSE;
}

/*
 * This is called after returning from java to JS.  if the exception
 * is actually a JSException, we pull the original JS error state out
 * of the JSContext and use that.  Otherwise we turn the JSException
 * into a string and pass it up as a JS error
 */
static JSBool
js_JExceptionToJSError(JSContext *cx, ExecEnv *ee)
{
    SavedJSError *err = 0;
    char *message;
    JSBool success;
    JHandle *exc;

    /*
     * Get rid of any JS errors so far, but save the deepest one
     * in case this was a JSException and we re-report it.
     */
    /* FIXME the deepest one is the most interesting? */
    err = cx->savedErrors;
    if (err) {
	while (err->next)
	    err = js_js_FreeError(err);
    }

    /* if no exception reached us, continue on our merry way */
    if (!exceptionOccurred(ee)) {
	PR_LOG(MojaSrc, debug,
               ("m-j succeeded, no exceptions cx=0x%x ee=0x%x",
		cx, ee));
	success = JS_TRUE;
	goto done;
    }

    /* if we got this far there was an error for sure */
    success = JS_FALSE;

    switch (ee->exceptionKind) {
      case EXCKIND_THROW:
        /* save the exception while we poke around in java */
        exc = ee->exception.exc;
        exceptionClear(ee);

	if (err && js_isJSException(ee, exc)) {
	    js_ReportErrorAgain(cx, err->message, &err->report);
	    PR_LOG(MojaSrc, debug,
                   ("m-j re-reported error \"%s\" cx=0x%x ee=0x%x",
		    err->message, cx, ee));
	}
	/* otherwise, describe the exception to a string */
	else if (js_isSubclassOf(cx,
				 obj_array_classblock(exc),
				 ThrowableClassBlock)) {
	    HString *hdetail =
	      unhand((Hjava_lang_Throwable *)exc)->detailMessage;
	    ClassClass *cb = obj_array_classblock(exc);

	    message = (char *)
		JRI_GetStringPlatformChars((JRIEnv *) ee,
					   (struct java_lang_String *) hdetail,
					   (const jbyte *) cx->charSetName,
					   (jint) cx->charSetNameLength);
	    PR_LOG(MojaSrc, debug,
                   ("m-j converted exception %s, \"%s\" to error cx=0x%x ee=0x%x",
		    cbName(cb), message, cx, ee));
	    /* pass the string to JS_ReportError */
            JS_ReportError(cx, "uncaught Java exception %s (\"%s\")",
			   cbName(cb), message);
	}

        /* it's not a Throwable, somebody in java-land is being lame */
	else {
	    ClassClass *cb = obj_array_classblock(exc);
            JS_ReportError(cx, "uncaught Java exception of class %s",
			      cbName(cb));
	    PR_LOG(MojaSrc, debug,
                   ("m-j converted exception %s to error cx=0x%x ee=0x%x",
		    cbName(cb), cx, ee));
	}
	break;
      case EXCKIND_STKOVRFLW:
        JS_ReportError(cx, "Java stack overflow, pc=0x%x", ee->exception.addr);
	break;
      default:
	JS_ReportError(cx,
                       "internal error: Java exception of unknown type %d",
		       ee->exceptionKind);
	break;
    }

  done:
    if (err) {
	js_js_FreeError(err);
	cx->savedErrors = 0;
    }
    PR_LOG(MojaSrc, debug, ("m-j cleared JS errors cx=0x%x", cx));
    return success;
}

/***********************************************************************/

static JSBool
js_FindSystemClass(JSContext *cx, char *name, ClassClass **clazz)
{
    ExecEnv *ee;

    ee = jsj_GetCurrentEE(cx);

    if (!ee) return JS_FALSE;

    *clazz = FindClassFromClass(ee, name, TRUE, NULL);

    return js_JExceptionToJSError(cx, ee);
}

/*
 * All js_CallJava calls must use a data pointer that starts like
 * this one:
 */
typedef struct {
    JRIEnv *env;
} js_CallJava_data;

static JSBool
js_CallJava(JSContext *cx, JSJCallback doit, void *d, JSBool pushSafeFrame)
{
    js_CallJava_data *data = d;
    JSBool success = JS_TRUE;
    ExecEnv *ee;
    JSJClassData *classData;

    ee = jsj_GetCurrentEE(cx);

    if (!ee) return JS_FALSE;

    data->env = (JRIEnv *) ee;

    /* security: push the safety frame onto the java stack */
    if (pushSafeFrame)
	if (!js_pushSafeFrame(cx, ee, &classData)) {
	    return JS_FALSE;
	}

    PR_LOG_BEGIN(MojaSrc, debug, ("entering java ee=0x%x cx=0x%x", ee, cx));
    /* FIXME this should be restructured: there could be a prolog and
     * epilog to set up and tear down the JS->java call stuff */
    doit(data);

    PR_LOG_END(MojaSrc, debug, ("left java ee=0x%x cx=0x%x", ee, cx));

    /* it's only safe to call this on the mozilla thread */
    success = js_JExceptionToJSError(cx, ee);

    /* pop the safety frame */
    if (pushSafeFrame)
	js_popSafeFrame(cx, ee, classData);

    return success;
}


/***********************************************************************/

typedef struct {
    JRIEnv *env;
    HObject *self;
    char *name;
    char *sig;
    struct methodblock *mb;
    JSBool isStaticCall;
    va_list args;
    long *raddr;
    size_t rsize;
} js_ExecuteJavaMethod_data;

static void
js_ExecuteJavaMethod_stub(void *d)
{
    js_ExecuteJavaMethod_data *data = (js_ExecuteJavaMethod_data *) d;

    data->raddr[0] =
	do_execute_java_method_vararg(/*FIXME*/(ExecEnv*)data->env,
				      data->self,
				      data->name, data->sig,
				      data->mb, (bool_t) data->isStaticCall,
				      data->args,
				      (data->rsize > sizeof(long))
					  ? &data->raddr[1] : NULL,
				      FALSE);
}

static JSBool
js_ExecuteJavaMethod(JSContext *cx, void *raddr, size_t rsize,
			HObject *ho, char *name, char *sig,
			struct methodblock *mb, JSBool isStaticCall, ...)
{
    js_ExecuteJavaMethod_data data;
    JSBool success;
    va_list args;

#if defined(XP_MAC) && !defined(NSPR20)
 /* Metrowerks va_start() doesn't handle one-byte parameters properly. FIX this when va_start() works again. */
	args = &isStaticCall+1;
#else
    va_start(args, isStaticCall);
#endif

    data.self = ho;
    data.name = name;
    data.sig = sig;
    data.mb = mb;
    data.isStaticCall = isStaticCall;
    VARARGS_ASSIGN(data.args, args);
    data.raddr = raddr;
    data.rsize = rsize;

    success = js_CallJava(cx, js_ExecuteJavaMethod_stub, &data, JS_TRUE);
    va_end(args);

    return success;
}

/***********************************************************************/

typedef struct {
    JRIEnv *env;
    JSContext *cx;
    char *name;
    ClassClass *fromclass;
    ClassClass *ret;
    char *errstr;
} js_FindJavaClass_data;

static void
js_FindJavaClass_stub(void *d)
{
    js_FindJavaClass_data *data = d;
    ExecEnv *ee = /*FIXME*/(ExecEnv*)data->env;

    exceptionClear(ee);

    /* FIXME need to push a stack frame with the classloader
     * of data->fromclass or the security check on opening
     * the url for the class will fail */

    data->ret = FindClassFromClass(ee, data->name, TRUE, data->fromclass);

    /* we clear the exception state, because when JS
     * fails to find a class it assumes it's a package instead
     * of an error. */
    /* FIXME can we report an error if the problem is accessing
     * bogus-codebase? */

    if (exceptionOccurred(ee)) {
	ClassClass *cb = obj_array_classblock(ee->exception.exc);

#ifdef DEBUG
	char *message;
        /* FIXME this could fail: we don't check if it's Throwable,
	 * but i assume that FindClass is well-behaved */
	HString *hdetail =
	  unhand((Hjava_lang_Throwable *)
		 ee->exception.exc)->detailMessage;

	message = (char *)
	    JRI_GetStringPlatformChars((JRIEnv *) ee,
				       (struct java_lang_String *) hdetail,
				       (const jbyte *) data->cx->charSetName,
				       (jint) data->cx->charSetNameLength);

	PR_LOG(MojaSrc, debug,
               ("exception in is_subclass_of %s (\"%s\")",
		cbName(cb), message));
#endif

#ifdef DEBUG_JSJ
	/* take a look at the exception to see if we can narrow
	 * down the kinds of failures that cause a package to
	 * be created? */
	exceptionDescribe(ee);
#endif
        /* FIXME other exceptions don't matter? narrow this down... */
	exceptionClear(ee);
    }
}

/*
 * This can call arbitrary java code in class initialization.
 * pass 0 for the "from" argument for system classes, but if
 * you want to check the applet first pass its class.
 */
static ClassClass *
js_FindJavaClass(JSContext *cx, char *name, ClassClass *from)
{
    js_FindJavaClass_data data;

    data.cx = cx;
    data.name = name;
    data.fromclass = from;
    data.errstr = 0;
    if (!js_CallJava(cx, js_FindJavaClass_stub, &data, JS_TRUE))
	return 0;
    if (data.errstr) {
        JS_ReportError(cx, "%s", data. errstr);
	free(data.errstr);
	/* FIXME need to propagate error condition differently */
	return 0;
    }
    return data.ret;
}

/***********************************************************************/


typedef struct {
    JRIEnv *env;
    JSBool privileged;
    char *name;
    ClassClass *cb;
    char *sig;
    va_list args;
    HObject *ret;
} js_ConstructJava_data;

static void
js_ConstructJava_stub(void *d)
{
    js_ConstructJava_data *data = d;
    ExecEnv *ee = /*FIXME*/(ExecEnv*)data->env;

    if (data->privileged) {
	/* FIXME extremely lame - there should be a security
	 * flag to execute_java_constructor_vararg instead.
	 * the effect of this is that the JSObject constructor may
         * get called on the wrong thread, but this probably won't
	 * do any damage.  JRI will fix this, right? */
	ee = PRIVILEGED_EE;
    }
    data->ret =
      execute_java_constructor_vararg(ee,
				      data->name, data->cb,
				      data->sig, data->args);
}

/* this can call arbitrary java code in class initialization */
static HObject *
js_ConstructJava(JSContext *cx, char *name, ClassClass *cb,
		    char *sig, ...)
{
    js_ConstructJava_data data;
    va_list args;
    JSBool success;

    va_start(args, sig);
    data.name = name;
    data.privileged = JS_FALSE;
    data.cb = cb;
    data.sig = sig;
    VARARGS_ASSIGN(data.args, args);
    data.env = JRI_GetCurrentEnv();

    success = js_CallJava(cx, js_ConstructJava_stub, &data, JS_TRUE);
    va_end(args);

    if (success) return data.ret;
    else return 0;
}

/* for private constructors, i.e. JSObject */
static HObject *
js_ConstructJavaPrivileged(JSContext *cx, char *name, ClassClass *cb,
			      char *sig, ...)
{
    js_ConstructJava_data data;
    va_list args;
    JSBool success;

    va_start(args, sig);
    data.privileged = JS_TRUE;
    data.name = name;
    data.cb = cb;
    data.sig = sig;
    VARARGS_ASSIGN(data.args, args);
    data.env = JRI_GetCurrentEnv();

    success = js_CallJava(cx, js_ConstructJava_stub, &data, JS_TRUE);
    va_end(args);

    if (success) return data.ret;
    else return 0;
}


/***********************************************************************/

static JSBool jsj_enabled = JS_TRUE;

/*
 * most of the initialization is done lazily by this function,
 * which is only called through jsj_GetCurrentEE().  it is
 * only called once - this is enforced in jsj_GetCurrentEE().
 */
static void
jsj_FinishInit(JSContext *cx, JRIEnv *env)
{
    char *name;
    ExecEnv *ee;

    PR_LOG(MojaSrc, debug, ("jsj_FinishInit()\n"));

    /* initialize the reflection tables */
    javaReflections =
      PR_NewHashTable(256, (PRHashFunction) java_hashHandle,
		      (PRHashComparator) java_pointerEq,
		      (PRHashComparator) java_pointerEq, 0, 0);
#ifdef NSPR20
    PR_RegisterGCLockHook((GCLockHookFunc*) PrepareJSLocksForGC, 0);
#endif
    PR_RegisterRootFinder(scanJSJavaReflections,
                          "scan JS reflections of java objects",
			  JS_GetRuntime(cx));
    if (javaReflectionsMonitor == NULL)
	javaReflectionsMonitor = PR_NewNamedMonitor("javaReflections");
    jsReflections =
      PR_NewHashTable(256, (PRHashFunction) java_hashHandle,
		      (PRHashComparator) java_pointerEq,
		      (PRHashComparator) java_pointerEq, 0, 0);
    if (jsReflectionsMonitor == NULL)
	jsReflectionsMonitor = PR_NewNamedMonitor("jsReflections");

    ee = (ExecEnv *) env;

    exceptionClear(ee);

    name = "java/lang/Object";
    if (! (ObjectClassBlock = FindClassFromClass(ee, name, TRUE, 0)))
	goto badclass;
    MakeClassSticky(ObjectClassBlock);

    name = "netscape/javascript/JSObject";
    if (! (JSObjectClassBlock = FindClassFromClass(ee, name, TRUE, 0)))
	goto badclass;
    MakeClassSticky(JSObjectClassBlock);
    JSObjectInternalField = JRI_GetFieldID(env,
					   (struct java_lang_Class*)cbHandle(JSObjectClassBlock),
                                           "internal", "I");

    name = "netscape/javascript/JSException";
    if (! (JSExceptionClassBlock = FindClassFromClass(ee, name, TRUE, 0)))
	goto badclass;
    MakeClassSticky(JSExceptionClassBlock);

    name = "java/lang/String";
    if (! (StringClassBlock = FindClassFromClass(ee, name, TRUE, 0)))
	goto badclass;
    MakeClassSticky(StringClassBlock);

    name = "java/lang/Boolean";
    if (! (BooleanClassBlock = FindClassFromClass(ee, name, TRUE, 0)))
	goto badclass;
    MakeClassSticky(BooleanClassBlock);

    name = "java/lang/Double";
    if (! (DoubleClassBlock = FindClassFromClass(ee, name, TRUE, 0)))
	goto badclass;
    MakeClassSticky(DoubleClassBlock);

    name = "java/lang/Throwable";
    if (! (ThrowableClassBlock = FindClassFromClass(ee, name, TRUE, 0)))
	goto badclass;
    MakeClassSticky(ThrowableClassBlock);

    return;

  badclass:
    PR_LOG(MojaSrc, error, ("couldn't find class \"%s\"\n", name));
    JS_ReportError(cx, "Unable to initialize LiveConnect: missing \"%s\"",
		   name);
    jsj_enabled = JS_FALSE;
    return;
}

PR_STATIC_CALLBACK(int)
jsj_TrashJSReflectionEntry(PRHashEntry *he, int i, void *arg)
{
    JRIEnv *env = arg;
    jref jso = (jref) he->value;

    JRI_SetFieldInt(env, jso, JSObjectInternalField, (jint) 0);
    /* FIXME inline JS_RemoveRoot -- this will go away with GC unification */
    JS_LOCK_RUNTIME(finalizeRuntime);
    (void) PR_HashTableRemove(finalizeRuntime->gcRootsHash, (void *)&he->key);
    JS_UNLOCK_RUNTIME(finalizeRuntime);

    return HT_ENUMERATE_REMOVE;
}

PR_EXTERN(void)
JSJ_Finish(void)
{
    JRIEnv *env = 0;

    /* PR_ASSERT(we_have_libmocha_lock) */

    jsj_enabled = JS_FALSE;

    if (!javaReflectionsMonitor)
	return;
    PR_EnterMonitor(javaReflectionsMonitor);
    if (javaReflections) {
	PR_HashTableDestroy(javaReflections);
	javaReflections = NULL;
    }
    PR_ExitMonitor(javaReflectionsMonitor);

    env = JRI_GetCurrentEnv();

    if (!env) {
#ifdef DEBUG_JSJ
        PR_ASSERT(env);
#else
        return;
#endif
    }

    /* FIXME assert that lm_lock is held */
    PR_EnterMonitor(jsReflectionsMonitor);
    if (jsReflections) {
        PR_HashTableEnumerateEntries(jsReflections,
                                     jsj_TrashJSReflectionEntry,
                                     (void*) env);
	PR_HashTableDestroy(jsReflections);
	jsReflections = NULL;
    }
    PR_ExitMonitor(jsReflectionsMonitor);
}

/*
 * Get the java class associated with an instance, useful for access
 * to static fields and methods of applets.
 */
static JSBool
js_getJavaClass(JSContext *cx, JSObject *obj, PRUintn argc, jsval *argv,
		jsval *rval)
{
    JSObject *mo;
    JSObject *moclass;
    JSJava *java;

    /* FIXME this could accept strings as well i suppose */
    if (argc != 1 ||
	!JSVAL_IS_OBJECT(argv[0]) ||
	!(mo = JSVAL_TO_OBJECT(argv[0])) ||
	!JS_InstanceOf(cx, mo, &java_class, 0) ||
	(java = (JSJava *) JS_GetPrivate(cx, mo))->type != JAVA_OBJECT) {
        JS_ReportError(cx, "getClass expects a Java object argument");
	return JS_FALSE;
    }

    if (!(moclass = js_ReflectJClassToJSObject(cx, java->cb))) {
        JS_ReportError(cx, "getClass can't find Java class reflection");
	return JS_FALSE;
    }

    *rval = OBJECT_TO_JSVAL(moclass);
    return JS_TRUE;
}

static JSObject *
js_DefineJavaPackage(JSContext *cx, JSObject *obj, char *jsname, char *package)
{
    JSJavaPackage *pack;
    JSObject *pobj;

    pack = JS_malloc(cx, sizeof(JSJavaPackage));
    if (!pack)
	return 0;

    if (package) {
	pack->name = JS_strdup(cx, package);
	if (!pack->name) {
	    JS_free(cx, pack);
	    return 0;
	}
    } else {
	pack->name = 0;
    }

    /* FIXME can we make the package read-only? */
    pobj = JS_DefineObject(cx, obj, jsname, &javapackage_class, 0, 0);
    if (pobj && !JS_SetPrivate(cx, pobj, pack))
	(void) JS_DeleteProperty(cx, obj, jsname);
    return pobj;
}

/* hook from js_DestroyContext */
static void
jsj_DestroyJSContextHook(JSContext *cx)
{
    /* FIXME do anything with the env? */
    /* FIXMEold cx->javaEnv = 0; */
}

/* FIXME make sure this is called from jsscript.c:js_DestroyScript !*/
static void
jsj_DestroyScriptHook(JSContext *cx, JSScript *script)
{
    JSJClassData *data = script->javaData;

    if (!data)
	return;

    jsj_DestroyClassData(cx, data);

    script->javaData = 0;
}

static void
jsj_DestroyFrameHook(JSContext *cx, JSStackFrame *frame) {
    if (frame->annotation) {
        JRIEnv *env = (JRIEnv *) jsj_GetCurrentEE(cx);
        if (!env) return;

        JRI_DisposeGlobalRef(env, frame->annotation);
    }
}

/* we need to hook into the js interpreter in a few special places */
JSInterpreterHooks js_Hooks = {
    jsj_DestroyJSContextHook,
    jsj_DestroyScriptHook,
    jsj_DestroyFrameHook
};

extern void _java_javascript_init(void);

/*
 * Initialize JS<->Java glue
 */
PR_IMPLEMENT(JSBool)
JSJ_Init(JSJCallbacks *callbacks)
{
    static JSBool initialized = JS_FALSE;

    /* JSJ_Init may be called twice: it is called with default
     * hooks when the netscape.javascript.JSObject class is
     * initialized, but it also may be called by the client
     * or server.  in this case, the first to call it gets
     * to set the hooks */
    if (initialized)
        return JS_FALSE;
    initialized = JS_TRUE;

    _java_javascript_init();	/* stupid linker tricks */

    js_SetInterpreterHooks(&js_Hooks);

    if (!callbacks)
	return JS_TRUE;

#ifdef NSPR20
    if (MojaSrc == NULL)
        MojaSrc = PR_NewLogModule("MojaSrc");
#endif

    jsj_callbacks = callbacks;

    return JS_TRUE;
}

/*
 * Initialize a javascript context and toplevel object for use with JSJ
 */
PR_IMPLEMENT(JSBool)
JSJ_InitContext(JSContext *cx, JSObject *obj)
{
    JSObject *mo;

    /* grab the main runtime from the context if we haven't yet */
    if (!finalizeRuntime)
        finalizeRuntime = JS_GetRuntime(cx);

    /* define the top of the java package namespace as "Packages" */
    mo = js_DefineJavaPackage(cx, obj, "Packages", 0);

    /* some convenience packages */
    /* FIXME these should be properties of the top-level package
     * too.  as it is there will be two different objects for
     * "java" and "Packages.java" which is unfortunate but mostly
     * invisible */

    /* FIXMEbe Use new JS_AliasProperty API call.
     * have to lookup Packages.java first... */

    js_DefineJavaPackage(cx, obj, "java", "java");
    js_DefineJavaPackage(cx, obj, "sun", "sun");
    js_DefineJavaPackage(cx, obj, "netscape", "netscape");

    JS_DefineFunction(cx, obj, "getClass", js_getJavaClass, 0, JSPROP_READONLY);
    /* create the prototype object for java objects and classes */
    mo = JS_DefineObject(cx, obj, "#javaPrototype", &java_class, 0,
			   JSPROP_READONLY | JSPROP_PERMANENT);
    /* FIXME set up the private data? */

    /* any initialization that depends on java running is done in
     * js_FinishInitJava */

    return JS_TRUE;
}

#ifdef NEVER
/* This is really a big comment describing the JavaScript class file,
   never intended to be compiled. */
/* FIXME should this be in a package? */

/**
 * Internal class for secure JavaScript->Java calls.
 * We load this class with a crippled ClassLoader, and make sure
 * that one of the methods from this class is on the Java stack
 * when JavaScript calls into Java.
 * The call to System.err.println is necessary because otherwise
 * sj doesn't account for the arguments in mb->maxstack, and
 * the jsContext argument gets wiped out when the next stack
 * frame is constructed.
 */

package netscape.javascript;

class JavaScript {
    /* can't be constructed */
    private JavaScript() {};

    /**
     * this is the method that will be on the Java stack when
     * JavaScript calls Java.  it is never actually called,
     * but we put a reference to it on the stack as if it had
     * been.  the mochaContext argument is a native pointer to
     * the current JavaScript context so that we can figure
     * that out from Java.
     */
    static void callJava(int jsContext) {
	/* this call is here to make sure the compiler
	 * allocates enough stack for us - previously we
	 * were getting mb->maxstack == 0, which would
	 * cause the jsContext argument to be overwritten */
	System.err.println(jsContext);
    };
}

#endif /* NEVER */

static unsigned char JavaScript_class_bytes[] = {
 "\312\376\272\276\000\003\000-\000\035\007\000\026\007\000\030\007\000"
 "\020\007\000\033\012\000\003\000\012\012\000\002\000\010\011\000\004\000"
 "\011\014\000\031\000\034\014\000\015\000\032\014\000\013\000\014\001\000"
 "\007println\001\000\004(I)V\001\000\003err\001\000\015ConstantValue\001"
 "\000\010callJava\001\000\023java/io/PrintStream\001\000\012Exceptions"
 "\001\000\017LineNumberTable\001\000\012SourceFile\001\000\016LocalVar"
 "iables\001\000\004Code\001\000\036netscape/javascript/JavaScript\001\000"
 "\017JavaScript.java\001\000\020java/lang/Object\001\000\006<init>\001"
 "\000\025Ljava/io/PrintStream;\001\000\020java/lang/System\001\000\003"
 "()V\000\000\000\001\000\002\000\000\000\000\000\002\000\002\000\031\000"
 "\034\000\001\000\025\000\000\000\035\000\001\000\001\000\000\000\005*"
 "\267\000\006\261\000\000\000\001\000\022\000\000\000\006\000\001\000\000"
 "\000\020\000\010\000\017\000\014\000\001\000\025\000\000\000$\000\002"
 "\000\001\000\000\000\010\262\000\007\032\266\000\005\261\000\000\000\001"
 "\000\022\000\000\000\012\000\002\000\000\000\037\000\007\000\032\000\001"
 "\000\023\000\000\000\002\000\027"
};

/* now something to make a classloader and get a JavaScript object
 * from it. */

/* FIXME garbage collection of these classes?  how should they be
 * protected? */

#include "java_lang_ClassLoader.h"

#define USELESS_CODEBASE_URL "http://javascript-of-unknown-origin.netscape.com/"

static void
jsj_DestroyClassData(JSContext *cx, JSJClassData *data)
{
    ExecEnv *ee;
    JRIEnv *env;
    HObject *loader;

    /* FIXME locking! */
    if (--data->nrefs > 0)
	return;

    ee = jsj_GetCurrentEE(cx);
    if (!ee) {
        /* FIXME memory leak - if we can't find a java execution
         * env we can't free the classloader and class */
        PR_ASSERT(!"can't find java env to free JSScript.javaData");
	return;
    }

    env = (JRIEnv *) ee;

    if (data->clazz) {
	JRI_DisposeGlobalRef(env, data->clazz);
	data->clazz = NULL;
    }

    if (data->loader) {
	loader = JRI_GetGlobalRef(env, data->loader);
	/* comment out call to releaseClassLoader because it is obsolete.
	execute_java_dynamic_method(ee, loader,
                                    "releaseClassLoader", "()V");
	if (exceptionOccurred(ee)) {
	*/
                /* FIXME memory leak - if we can't find a java execution
                 * env we can't free the classloader and class */
    /*
            PR_ASSERT(!"failed releasing javascript classloader");
	    goto done;
	}
	*/
	JRI_DisposeGlobalRef(env, data->loader);
	data->loader = NULL;
    }

    data->mb = NULL;

  done:
    free(data);
}

/* security: this method returns a methodblock which can be
 * used for secure JS->java calls.  the methodblock is
 * guaranteed to have an associated AppletClassLoader which will
 * allow the SecurityManager to determine what permissions to
 * give to the Java code being called.
 */
static JSJClassData *
jsj_MakeClassData(JSContext *cx)
{
    int i;
    ExecEnv *ee;
    JRIEnv *env;
    JSScript *script;
    JSStackFrame *fp;
    PRInt32 length;
    HArrayOfByte *bytes;
    const char *origin_url;
    struct Hjava_lang_ClassLoader *loader;
    ClassClass *clazz;
    struct methodblock *mb;
    JSJClassData *data;

    ee = jsj_GetCurrentEE(cx);
    if (!ee) return 0;

    /* see if this script already has a classloader */
    script = NULL;
    for (fp = cx->fp; fp; fp = fp->down) {
	script = fp->script;
	if (script)
	    break;
    }
    if (script &&
	(data = (JSJClassData *) script->javaData)) {
	/* FIXME locking! */
	data->nrefs++;
	return data;
    }

    data = (JSJClassData *) malloc(sizeof(JSJClassData));
    if (!data) {
	JS_ReportOutOfMemory(cx);
	return 0;
    }
    data->nrefs = 1;
    data->loader = NULL;
    data->clazz = NULL;
    data->mb = NULL;

    if (script && script->filename
        && strcmp("[unknown origin]", script->filename)) {
	origin_url = script->filename;
    } else {
	origin_url = USELESS_CODEBASE_URL;
    }

    loader = jsj_callbacks->jsClassLoader(cx, origin_url);

    if (exceptionOccurred(ee) || !loader) {
        JS_ReportError(cx, "couldn't create ClassLoader for JavaScript");
	free(data);
	return 0;
    }

    env = (JRIEnv *) ee;
    data->loader = JRI_NewGlobalRef(env, loader);

    /* FIXME should save and re-use this array in a jglobal */
    /* make the array of bytes for class JavaScript */
    length = sizeof(JavaScript_class_bytes);
    bytes = (HArrayOfByte *) ArrayAlloc(T_BYTE, length);
    if (!bytes) {
	jsj_DestroyClassData(cx, data);
	JS_ReportError(cx,
                       "couldn't allocate bytes for JavaScript.class");
	return 0;
    }
    memmove(unhand(bytes)->body, JavaScript_class_bytes, (size_t)length);

    /* FIXME lock to avoid race condition between this and defineClass */
    clazz = FindLoadedClass("netscape/javascript/JavaScript", loader);

    if (!clazz) {
	/* make class JavaScript */
	clazz = (ClassClass*)
	    do_execute_java_method(ee, loader,
                                   "defineClass", "(Ljava/lang/String;[BII)Ljava/lang/Class;",
				   0, FALSE, NULL /* no required name */, bytes, 0L, length);

	if (exceptionOccurred(ee) || !clazz) {
	    jsj_DestroyClassData(cx, data);
            JS_ReportError(cx, "couldn't load class JavaScript");
	    return 0;
	}

        (void)
            do_execute_java_method(ee, loader,
                                   "setPrincipalAry",
                                   "(Ljava/lang/Class;Ljava/lang/String;)Z",
                                   0, FALSE,
                                   clazz, NULL /* don't lookup */);

	if (exceptionOccurred(ee) || !clazz) {
	    jsj_DestroyClassData(cx, data);
            JS_ReportError(cx, "couldn't set principal for class JavaScript");
	    return 0;
	}
    }

    data->clazz = JRI_NewGlobalRef(env, cbHandle(clazz));

    /* FIXME set up the signatures on the class here */

    /* find the static method callJava() for the class */
    for (i = 0; i < (int)cbMethodsCount(clazz); i++) {
	mb = cbMethods(clazz) + i;
        if (!strcmp(fieldname(&mb->fb), "callJava")
            && !strcmp(fieldsig(&mb->fb), "(I)V")) {
	    /* found it... */
	    data->mb = mb;
	    if (script) {
		/* FIXME locking! */
		data->nrefs++;
		script->javaData = data;
	    }
	    return data;
	}
    }

    jsj_DestroyClassData(cx, data);
    JS_ReportError(cx, "can't find method JavaScript.callJava");

    return 0;
}

/*
 * tell whether a partiular methodblock is part of a safety frame
 *
 * the security here depends on the fact that users are forbidden
 * to define packages in netscape.javascript.  the consequence
 * of breaking this security would be that the user could get
 * a java int dereferenced as a pointer, most likely crashing
 * the program.
 *
 * embedding some sort of secret into the JavaScript class
 *  (pointer to itself?) would also secure this.
 */
PR_IMPLEMENT(JSBool)
JSJ_IsSafeMethod(struct methodblock *mb)
{
    ClassClass *cb;

    if (!mb)
	return JS_FALSE;

    cb = fieldclass(&mb->fb);

    if (!cb ||
        strcmp(cbName(cb), "netscape/javascript/JavaScript"))
	return JS_FALSE;

    return JS_TRUE;
}

/*
 * push a frame onto the java stack which does nothing except
 * provide a classloader for the security manager
 */
static JSBool
js_pushSafeFrame(JSContext *cx, ExecEnv *ee, JSJClassData **classData)
{
    JavaFrame *current_frame, *previous_frame;
    JavaStack *current_stack;
    struct methodblock *mb;

    if (!jsj_callbacks->jsClassLoader) {
        *classData = NULL;
        return JS_TRUE;
    }

    if (!(*classData = jsj_MakeClassData(cx)))
	return JS_FALSE;
    mb = (*classData)->mb;

    previous_frame = ee->current_frame;
    if (previous_frame == 0) {
	/* bottommost frame on this Exec Env. */
	current_stack = ee->initial_stack;
	current_frame = (JavaFrame *)(current_stack->data); /* no vars */
    } else {
	int args_size = mb->args_size;
	current_stack = previous_frame->javastack; /* assume same stack */
	if (previous_frame->current_method) {
	    int size = previous_frame->current_method->maxstack;
	    current_frame = (JavaFrame *)(&previous_frame->ostack[size]);
	} else {
            /* The only frames that don't have a mb are pseudo frames like
             * this one and they don't really touch their stack. */
	    current_frame = (JavaFrame *)(previous_frame->optop + 3);
	}
	if (current_frame->ostack + args_size > current_stack->end_data) {
            /* Ooops.  The current stack isn't big enough.  */
	    if (current_stack->next != 0) {
		current_stack = current_stack->next;
	    } else {
		current_stack = CreateNewJavaStack(ee, current_stack);
		if (current_stack == 0) {
		    JS_ReportOutOfMemory(cx);
		    return JS_FALSE;
		}
	    }
	    /* no vars */
	    current_frame = (JavaFrame *)(current_stack->data);
	}
    }
    current_frame->prev = previous_frame;
    current_frame->javastack = current_stack;
    current_frame->optop = current_frame->ostack;
    current_frame->vars = 0;	/* better not reference any! */
    current_frame->monitor = 0; /* not monitoring anything */
    current_frame->annotation = 0;

    /* make this be a method with the JS classloader */
    current_frame->current_method = mb;

    /* FIXME push the current mochaContext as an integer */
    current_frame->optop[0].i = (int32_t) cx;
    current_frame->optop += current_frame->current_method->args_size;
    current_frame->constant_pool = 0;

    ee->current_frame = current_frame;

    return JS_TRUE;
}

/*
 * pop the safety frame from the java stack
 */
static void
js_popSafeFrame(JSContext *cx, ExecEnv *ee, JSJClassData *classData)
{
    if (!classData)
        return;

    ee->current_frame = ee->current_frame->prev;
    jsj_DestroyClassData(cx, classData);
}

/*
 * look up the stack for the most recent safety frame and extract
 * the JSContext from it.  return NULL if no such frame was found.
 */
PR_IMPLEMENT(void)
JSJ_FindCurrentJSContext(JRIEnv *env, JSContext **cxp, void **loaderp)
{
    ExecEnv *ee = (ExecEnv *) env;
    JavaFrame *frame, frame_buf;
    ClassClass	*cb;

#define NEXT_FRAME(frame) \
    (((frame)->current_method && (frame)->current_method->fb.access & ACC_MACHINE_COMPILED) ? \
     CompiledFramePrev(frame, &frame_buf) \
     : frame->prev)

    *cxp = 0;
    *loaderp = 0;

    /* search the stack upward */
    for (frame = ee->current_frame; frame; frame = NEXT_FRAME(frame)) {
	struct methodblock *mb = frame->current_method;
	if (mb) {
	    cb = fieldclass(&frame->current_method->fb);
	    *loaderp = cbLoader(cb);
	    if (*loaderp) {
		if (JSJ_IsSafeMethod(mb)) {
			/* extract the mochacontext here */
		    *cxp = (JSContext *)frame->ostack[0].i;
		} else
		    *cxp = 0;
		return;
	    }
	}
    }
    return;
}

PR_IMPLEMENT(JSBool)
JSJ_IsCalledFromJava(JSContext *cx)
{
    ExecEnv *ee;

    ee = (ExecEnv *) JRI_GetCurrentEnv();
    if (ee == NULL)
        return JS_FALSE;

    return ee->current_frame != NULL;
}

/* extract the javascript object from a netscape.javascript.JSObject */
PR_IMPLEMENT(JSObject *)
JSJ_ExtractInternalJSObject(JRIEnv *env, HObject* jso)
{
    PR_ASSERT(obj_array_classblock((HObject*)jso) == JSObjectClassBlock);
    
    return (JSObject *) JRI_GetFieldInt(env, (netscape_javascript_JSObject*)jso,
                                        JSObjectInternalField);
}

PR_IMPLEMENT(JSPrincipals *)
js_GetJSPrincipalsFromJavaCaller(JSContext *cx, int callerDepth)
{
    return jsj_callbacks->getJSPrincipalsFromJavaCaller(cx, callerDepth);
}


#endif /* defined(JAVA) */
