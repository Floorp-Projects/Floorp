/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
 
/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * Publicly exported functions for JavaScript <==> Java communication.
 *
 */
 
#ifndef _JSJAVA_H
#define _JSJAVA_H

#ifndef prtypes_h___
#include "prtypes.h"
#endif

PR_BEGIN_EXTERN_C

#include "jni.h"             /* Java Native Interface */
#include "jsapi.h"           /* JavaScript engine API */

/*
 * A JSJavaVM structure is a wrapper around a JavaVM which incorporates
 * additional LiveConnect state.
 */
typedef struct JSJavaVM JSJavaVM;

/* LiveConnect and Java state, one per thread */
typedef struct JSJavaThreadState JSJavaThreadState;

/*
 * This callback table provides hooks to external functions that implement
 * functionality specific to the embedding.  For example, these callbacks are
 * necessary in multi-threaded environments or to implement a security
 * policy.
 */
typedef struct JSJCallbacks {

    /* This callback is invoked when there is no JavaScript execution
       environment (JSContext) associated with the current Java thread and
       a call is made from Java into JavaScript.  (A JSContext is associated
       with a Java thread by calling the JSJ_SetJSContextForJavaThread()
       function.)  This callback is only invoked when Java spontaneously calls
       into JavaScript, i.e. it is not called when JS calls into Java which
       calls back into JS.
       
       This callback can be used to create a JSContext lazily, or obtain
       one from a pool of available JSContexts.  The implementation of this
       callback can call JSJ_SetJSContextForJavaThread() to avoid any further
       callbacks of this type for this Java thread. */
    JSContext *	        (*map_jsj_thread_to_js_context)(JNIEnv *jEnv,
                                                        char **errp);

    /* This callback is invoked whenever a call is made into Java from
       JavaScript.  It's responsible for mapping from a JavaScript execution
       environment (JSContext) to a Java thread.  (A JavaContext can only
       be associated with one Java thread at a time.) */
    JSJavaThreadState * (*map_js_context_to_jsj_thread)(JSContext *cx,
                                                        char **errp);
                                                        
    /* This callback implements netscape.javascript.JSObject.getWindow(),
       a method named for its behavior in the browser environment, where it
       returns the JS "Window" object corresponding to the HTML window that an
       applet is embedded within.  More generally, it's a way for Java to get
       hold of a JS object that has not been explicitly passed to it. */
    JSObject *	        (*map_java_object_to_js_object)(JNIEnv *jEnv, jobject hint,
                                                        char **errp);
        
    /* An interim callback function until the LiveConnect security story is
       straightened out.  This function pointer can be set to NULL. */
    JSPrincipals *      (*get_JSPrincipals_from_java_caller)(JNIEnv *jEnv);
    
    /* The following two callbacks sandwich any JS evaluation performed
       from Java.   They may be used to implement concurrency constraints, e.g.
       by suspending the current thread until some condition is met.  In the
       browser embedding, these are used to maintain the run-to-completion
       semantics of JavaScript.  It is acceptable for either function pointer
       to be NULL. */
    JSBool	        (*enter_js_from_java)(char **errp);
    void	        (*exit_js)(void);

    /* Most LiveConnect errors are signaled by calling JS_ReportError(), but in
       some circumstances, the target JSContext for such errors is not
       determinable, e.g. during initialization.  In such cases any error
       messages are routed to this function.  If the function pointer is set to
       NULL, error messages are sent to stderr. */
    void                (*error_print)(const char *error_msg);

    JavaVM *            (*get_java_vm)(char **errp);

    /* Reserved for future use */
    void *              reserved[10];
} JSJCallbacks;

/*===========================================================================*/

/* A flag that denotes that a Java package has no sub-packages other than those
   explicitly pre-defined at the time of initialization.  An access
   to a simple name within such a package, therefore, must either correspond to
   one of these explicitly pre-defined sub-packages or to a class within this
   package.  It is reasonable for LiveConnect to signal an error if a simple
   name does not comply with these criteria. */
#define PKG_SYSTEM      1

/* A flag that denotes that a Java package which might contain sub-packages
   that are not pre-defined at initialization time, because the sub-packages
   may not be the same in all installations.  Therefore, an access to a simple
   name within such a a package which does not correspond to either a
   pre-defined sub-package or to a class, must be assummed to refer to an
   unknown sub-package.  This behavior may cause bogus JavaPackage objects to be
   created if a package name is misspelled, e.g. sun.oi.net. */
#define PKG_USER        2

/* A Java package defined at initialization time. */
typedef struct JavaPackageDef {
    const char *        name;   /* e.g. "java.lang" */
    const char *        path;   /* e.g. "java/lang", or NULL for default */
    int                 flags;  /* PKG_USER, PKG_SYSTEM, etc. */
} JavaPackageDef;

/*===========================================================================*/

/* The following two convenience functions present a complete, but simplified
   LiveConnect API which is designed to handle the special case of a single 
   Java-VM, with single-threaded operation, and the use of only one JSContext.
   The full API is in the section below. */

/* Initialize the provided JSContext by setting up the JS classes necessary for
   reflection and by defining JavaPackage objects for the default Java packages
   as properties of global_obj.  If java_vm is NULL, a new Java VM is
   created, using the provided classpath in addition to any default classpath.
   The classpath argument is ignored, however, if java_vm is non-NULL. */
PR_IMPLEMENT(JSBool)
JSJ_SimpleInit(JSContext *cx, JSObject *global_obj,
               JavaVM *java_vm, const char *classpath);

/* Free up all resources.  Destroy the Java VM if it was created by LiveConnect */
PR_IMPLEMENT(void)
JSJ_SimpleShutdown();

/*===========================================================================*/

/* The "full" LiveConnect API, required when more than one thread, Java VM, or
   JSContext is involved.  Initialization pseudocode might go roughly like
   this:

   JSJ_Init()   // Setup callbacks
   for each JavaVM {
      JSJ_ConnectToJavaVM(...)
   }
   for each JSContext {
      JSJ_InitJSContext(...)
   }
   for each JS evaluation {
      run JavaScript code in the JSContext;
   }
 */

/* Called once for all instances of LiveConnect to set up callbacks */
PR_IMPLEMENT(void)
JSJ_Init(JSJCallbacks *callbacks);

/* Called once per Java VM, this function initializes the classes, fields, and
   methods required for Java reflection.  If java_vm is NULL, a new Java VM is
   created, using the provided classpath in addition to any default classpath.
   The classpath argument is ignored, however, if java_vm is non-NULL. */
PR_IMPLEMENT(JSJavaVM *)
JSJ_ConnectToJavaVM(JavaVM *java_vm, const char *classpath);

/* Initialize the provided JSContext by setting up the JS classes necessary for
   reflection and by defining JavaPackage objects for the default Java packages
   as properties of global_obj.  Additional packages may be pre-defined by
   setting the predefined_packages argument.  (Pre-defining a Java package at
   initialization time is not necessary, but it will make package lookup faster
   and, more importantly, will avoid unnecessary network accesses if classes
   are being loaded over the network.) */
PR_IMPLEMENT(JSBool)
JSJ_InitJSContext(JSContext *cx, JSObject *global_obj,
                  JavaPackageDef *predefined_packages);
   
/* This function returns a structure that encapsulates the Java and JavaScript
   execution environment for the current native thread.  It is intended to
   be called from the embedder's implementation of JSJCallback's
   map_js_context_to_jsj_thread() function.  The thread_name argument is only
   used for debugging purposes and can be set to NULL.  The Java JNI
   environment associated with this thread is returned through the java_envp
   argument if java_envp is non-NULL.  */
PR_IMPLEMENT(JSJavaThreadState *)
JSJ_AttachCurrentThreadToJava(JSJavaVM *jsjava_vm, const char *thread_name,
                              JNIEnv **java_envp);

/* Destructor routine for per-thread JSJavaThreadState structure */
PR_IMPLEMENT(JSBool)
JSJ_DetachCurrentThreadFromJava(JSJavaThreadState *jsj_env);

/* This function is used to specify a particular JSContext as *the* JavaScript
   execution environment to be used when LiveConnect is accessed from the given
   Java thread, i.e. when one of the methods of netscape.javascript.JSObject
   has been called.  There can only be one such JS context for any given Java
   thread at a time.  To multiplex JSContexts among a single thread, this
   function could be called before Java is invoked on that thread.)  The return
   value is the previous JSContext associated with the given Java thread.

   If this function has not been called for a thread and a crossing is made
   into JavaScript from Java, the map_jsj_thread_to_js_context() callback will
   be invoked to determine the JSContext for the thread.  The purpose of the 
   function is to improve performance by avoiding the expense of the callback.
*/
PR_IMPLEMENT(JSContext *)
JSJ_SetDefaultJSContextForJavaThread(JSContext *cx, JSJavaThreadState *jsj_env);

/* This routine severs the connection to a Java VM, freeing all related resources.
   It shouldn't be called until the global scope has been cleared in all related
   JSContexts (so that all LiveConnect objects are finalized) and a JavaScript
   GC is performed.  Otherwise, accessed to free'ed memory could result. */
PR_IMPLEMENT(void)
JSJ_DisconnectFromJavaVM(JSJavaVM *);

/*
 * Reflect a Java object into a JS value.  The source object, java_obj, must
 * be of type java.lang.Object or a subclass and may, therefore, be an array.
 */
PR_IMPLEMENT(JSBool)
JSJ_ConvertJavaObjectToJSValue(JSContext *cx, jobject java_obj, jsval *vp);

PR_END_EXTERN_C
#endif  /* _JSJAVA_H */
