/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/* This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the native code implementation of the JavaPackage class.
 *
 * A JavaPackage is JavaScript's representation of a Java package.  The
 * JavaPackage object contains only a string, which is the path to the package,
 * e.g. "java/lang".  The JS properties of a JavaPackage are either nested packages
 * or a JavaClass object, which represents the path to a Java class.
 *
 * Note that there is no equivalent to a JavaPackage object in Java.  Example:
 * Although there are instances of java.lang.String and there are static methods
 * of java.lang.String that can be invoked, there's no such thing as a java.lang
 * object in Java that exists at run time.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "jsj_private.h"        /* LiveConnect internals */
#include "jsjava.h"


JSClass JavaPackage_class;      /* Forward declaration */

/*
 * The native part of a JavaPackage object.  It gets stored in the object's
 * private slot.
 */
typedef struct {
    const char * path;          /* e.g. "java/lang" or NULL if top level package */
    int flags;                  /* e.g. PKG_SYSTEM, PKG_CLASS */
} JavaPackage_Private;

static JSObject *
define_JavaPackage(JSContext *cx, JSObject *parent_obj,
                   const char *obj_name, const char *path, int flags, int access)
{
    JSObject *package_obj;
    JavaPackage_Private *package;

    package_obj = JS_DefineObject(cx, parent_obj, obj_name, &JavaPackage_class, 0, JSPROP_PERMANENT | access);
    
    if (!package_obj)
        return NULL;
    
    /* Attach private, native data to the JS object */
    package = (JavaPackage_Private *)JS_malloc(cx, sizeof(JavaPackage_Private));
    JS_SetPrivate(cx, package_obj, (void *)package);
    if (path)
        package->path = JS_strdup(cx, path);
    else
        package->path = "";

    package->flags = flags;

    /* Check for OOM */
    if (!package->path) {
        JS_DeleteProperty(cx, parent_obj, obj_name);
        JS_free(cx, package);
        return NULL;
    }

    return package_obj;
}

/* JavaPackage uses standard JS getProperty */

/*
 * Don't allow user-defined properties to be set on Java package objects, e.g.
 * it is illegal to write "java.lang.myProperty = 4".  We probably could relax
 * this restriction, but it's potentially confusing and not clearly useful.
 */
JS_STATIC_DLL_CALLBACK(JSBool)
JavaPackage_setProperty(JSContext *cx, JSObject *obj, jsval slot, jsval *vp)
{
    JavaPackage_Private *package = JS_GetPrivate(cx, obj);
    if (!package) {
        JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                JSJMSG_BAD_ADD_TO_PACKAGE);
        return JS_FALSE;
    }
    JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                JSJMSG_DONT_ADD_TO_PACKAGE);
    return JS_FALSE;
}

static JSBool quiet_resolve_failure;

/*
 * Resolve a component name to be either the name of a class or a package.
 */
JS_STATIC_DLL_CALLBACK(JSBool)
JavaPackage_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JavaPackage_Private *package;
    JSBool ok = JS_TRUE;
    jclass jclazz;
    char *subPath, *newPath;
    const char *path;
    JNIEnv *jEnv;
    JSJavaThreadState *jsj_env;

    /* Painful hack for pre_define_java_packages() */
    if (quiet_resolve_failure)
        return JS_FALSE;
                
    package = (JavaPackage_Private *)JS_GetPrivate(cx, obj);
    if (!package)
        return JS_TRUE;

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;
    subPath = JS_GetStringBytes(JSVAL_TO_STRING(id));

    /*
     * There will be an attempt to invoke the toString() method when producing
     * the string representation of a JavaPackage.  When this occurs, avoid
     * creating a bogus toString package.  (This means that no one can ever
     * create a package with the simple name "toString", but we'll live with
     * that limitation.)
     */
    if (!strcmp(subPath, "toString"))
        return JS_FALSE;

    path = package->path;
    newPath = JS_smprintf("%s%s%s", path, (path[0] ? "/" : ""), subPath);
    if (!newPath) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    jsj_env = jsj_EnterJava(cx, &jEnv);
    if (!jEnv) {
        ok = JS_FALSE;
        goto out;
    }

    /*
        Unfortunately, Java provides no way to find out whether a particular
        name is a package or not.  The only way to tell is to try to load the
        name as a class file and, if that fails, assume it's a package.  This
        makes things work as expected for the most part, but it has three
        noticeable problems that keep coming up:

        - You can refer to a package like java.lang.i.buried.paul without
          generating a complaint.  Of course, you'll never be able to refer to
          any classes through it.

        - An annoying consequence of the above is that misspelling a class name
          results in a cryptic error about packages.

        - In a browser context, i.e. where applets are involved, figuring out
          whether something is a class may require looking for it over the net
          using the current classloader.  This means that the first time you
          refer to java.lang.System in a js context, there will be an attempt
          to search for [[DOCBASE]]/java.class on the server.
    
        A solution is to explicitly tell jsjava the names of all the (local)
        packages on the CLASSPATH.  (Not implemented yet.)

    */
    jclazz = (*jEnv)->FindClass(jEnv, newPath);
    if (jclazz) {
        JSObject *newClass;

        newClass = jsj_define_JavaClass(cx, jEnv, obj, subPath, jclazz);
        (*jEnv)->DeleteLocalRef(jEnv, jclazz);
        if (!newClass) {
            ok = JS_FALSE;
            goto out;
        }
    } else {

        /* We assume that any failed attempt to load a class is because it
           doesn't exist.  If we wanted to do a better job, we would check
           the exception type and make sure that it's NoClassDefFoundError */
        (*jEnv)->ExceptionClear(jEnv);

        /*
         * If there's no class of the given name, then we must be referring to
         * a package.  However, don't allow bogus sub-packages of pre-defined
         * system packages to be created.
         */
        if (JS_InstanceOf(cx, obj, &JavaPackage_class, NULL)) {
            JavaPackage_Private *current_package;

            current_package = JS_GetPrivate(cx, obj);
            if (current_package->flags & PKG_SYSTEM) {
                char *msg, *cp;
                msg = JS_strdup(cx, newPath);

                /* Check for OOM */
                if (msg) {
                    /* Convert package of form "java/lang" to "java.lang" */
                    for (cp = msg; *cp != '\0'; cp++)
                        if (*cp == '/')
                            *cp = '.';
                    JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                                JSJMSG_MISSING_PACKAGE, msg);
                    free((char*)msg);
                }
                               
                ok = JS_FALSE;
                goto out;
            }
        }

        if (!define_JavaPackage(cx, obj, subPath, newPath, 0, JSPROP_READONLY)) {
            ok = JS_FALSE;
            goto out;
        }
        
#ifdef DEBUG
        /* printf("JavaPackage \'%s\' created\n", newPath); */
#endif

    }
    
out:
    free(newPath);
    jsj_ExitJava(jsj_env);
    return ok;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaPackage_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JSString *str;
    char *name, *cp;

    JavaPackage_Private *package = JS_GetPrivate(cx, obj);
    if (!package) {
        fprintf(stderr, "JavaPackage_resolve: no private data!\n");
        return JS_FALSE;
    }

    switch (type) {

    /* Pretty-printing of JavaPackage */
    case JSTYPE_VOID:   /* Default value */
    case JSTYPE_NUMBER:
    case JSTYPE_STRING:
        /* Convert '/' to '.' so that it looks like Java language syntax. */
        if (!package->path)
            break;
        name = JS_smprintf("[JavaPackage %s]", package->path);
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
            /* It's not necessary to call JS_ReportOutOfMemory(), as
               JS_NewString() will do so on failure. */
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

/*
 * Free the private native data associated with the JavaPackage object.
 */
JS_STATIC_DLL_CALLBACK(void)
JavaPackage_finalize(JSContext *cx, JSObject *obj)
{
    JavaPackage_Private *package = JS_GetPrivate(cx, obj);
    if (!package)
        return;

    if (package->path)
        JS_free(cx, (char *)package->path);
    JS_free(cx, package);
}

/*
 * The definition of the JavaPackage class
 */
JSClass JavaPackage_class = {
    "JavaPackage", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JavaPackage_setProperty,
    JS_EnumerateStub, JavaPackage_resolve,
    JavaPackage_convert, JavaPackage_finalize,

    /* Optionally non-null members start here. */
    NULL,                       /* getObjectOps */
    NULL,                       /* checkAccess */
    NULL,                       /* call */
    NULL,                       /* construct */
    NULL,                       /* xdrObject */
    NULL,                       /* hasInstance */
    NULL,                       /* mark */
    0,                          /* spare */
};

JavaPackageDef
standard_java_packages[] = {
    {"java",                NULL,   PKG_USER,   0},
    {"java.applet",         NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.awt",            NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.awt.datatransfer",                       
                            NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.awt.event",      NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.awt.image",      NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.awt.peer",       NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.beans",          NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.io",             NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.lang",           NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.lang.reflect",   NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.math",           NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.net",            NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.rmi",            NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.rmi.dgc",        NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.rmi.user",       NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.rmi.registry",   NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.rmi.server",     NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.security",       NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.security.acl",   NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.security.interfaces",
                            NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.sql",            NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.text",           NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.text.resources", NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"java.util",           NULL,   PKG_USER,   JSPROP_READONLY},
    {"java.util.zip",       NULL,   PKG_SYSTEM, JSPROP_READONLY},

    {"netscape",            NULL,   PKG_USER,   0},
    {"netscape.applet",     NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.application",NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.debug",      NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.javascript", NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.ldap",       NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.misc",       NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.net",        NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.plugin",     NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.util",       NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.secfile",    NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.security",   NULL,   PKG_SYSTEM, JSPROP_READONLY},
    {"netscape.WAI",        NULL,   PKG_SYSTEM, JSPROP_READONLY},

    {"sun",                 NULL,   PKG_USER,   0},
    {"Packages",            "",     PKG_USER,   JSPROP_READONLY},

    {NULL,                  NULL,   0,          0}
};

/*
 * On systems which provide strtok_r we'll use that function to avoid
 * problems with non-thread-safety.
 */
#if HAVE_STRTOK_R
# define STRTOK_1ST(str, seps, res) strtok_r (str, seps, &res)
# define STRTOK_OTHER(seps, res) strtok_r (res, seps, &res)
#else
# define STRTOK_1ST(str, seps, res) strtok (str, seps)
# define STRTOK_OTHER(seps, res) strtok (NULL, seps)
#endif

/*
 * Pre-define a hierarchy of JavaPackage objects.
 * Pre-defining a Java package at initialization time is not necessary, but
 * it will make package lookup faster and, more importantly, will avoid
 * unnecessary network accesses if classes are being loaded over the network.
 */
static JSBool
pre_define_java_packages(JSContext *cx, JSObject *global_obj,
                         JavaPackageDef *predefined_packages)
{
    JSBool package_exists;
    JSObject *parent_obj;
    JavaPackageDef *package_def;
    char *simple_name, *cp, *package_name, *path;
    int flags;

    if (!predefined_packages)
        return JS_TRUE;

    /* Iterate over all pre-defined Java packages */
    for (package_def = predefined_packages; package_def->name; package_def++) {
#if HAVE_STRTOK_R
	char *nextstr;
#endif
        package_name = path = NULL;

        parent_obj = global_obj;
        package_name = strdup(package_def->name);
        if (!package_name)
            goto out_of_memory;

        /* Walk the chain of JavaPackage objects to get to the parent of the
           rightmost sub-package in the fully-qualified package name. */
        for (simple_name = STRTOK_1ST(package_name, ".", nextstr); simple_name /*1*/; simple_name = STRTOK_OTHER(".", nextstr)) {
            jsval v;

            if (!simple_name) {
                JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL, 
                                        JSJMSG_DOUBLE_SHIPPING, package_name);
                goto error;
            }

            /* Check to see if the sub-package already exists */
            quiet_resolve_failure = JS_TRUE;
            package_exists = JS_LookupProperty(cx, parent_obj, simple_name, &v) && JSVAL_IS_OBJECT(v);
            quiet_resolve_failure = JS_FALSE;

            if (package_exists) {
                parent_obj = JSVAL_TO_OBJECT(v);
                continue;
            }

            /* New package objects should only be created at the terminal
               sub-package in a fully-qualified package-name */
            if (STRTOK_OTHER(".", nextstr)) {
                JS_ReportErrorNumber(cx, jsj_GetErrorMessage, NULL,
                                JSJMSG_BAD_PACKAGE_PREDEF,
                               package_def->name);
                goto error;
            }
            
            if (package_def->path) {
                path = strdup(package_def->path);
                if (!path)
                    goto out_of_memory;
            } else {
                /*
                 * The default path is specified, so create it from the
                 * fully-qualified package name.
                 */
                path = strdup(package_def->name);
                if (!path)
                    goto out_of_memory;
                /* Transform package name, e.g. "java.lang" ==> "java/lang" */
                for (cp = path; *cp != '\0'; cp++) {
                    if (*cp == '.')
                         *cp = '/';
                }
            }
            flags = package_def->flags;
            parent_obj = define_JavaPackage(cx, parent_obj, simple_name, path, flags,
                                            package_def->access);
            if (!parent_obj)
                goto error;
 
            free(path);
            break;
        }
        free(package_name);
    }
    return JS_TRUE;

out_of_memory:
    JS_ReportOutOfMemory(cx);

error:
    JS_FREE_IF(cx, package_name);
    JS_FREE_IF(cx, path);
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
JavaPackage_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    if (!JS_InstanceOf(cx, obj, &JavaPackage_class, argv))
        return JS_FALSE;
    return JavaPackage_convert(cx, obj, JSTYPE_STRING, rval);
}

static JSFunctionSpec JavaPackage_methods[] = {
    {"toString",   JavaPackage_toString,        0,      0,      0},
    {0, 0, 0, 0, 0},
};

/*
 * One-time initialization for the JavaPackage class.  (This is not
 * run once per thread, rather it's run once for a given JSContext.)
 */
JSBool
jsj_init_JavaPackage(JSContext *cx, JSObject *global_obj,
                     JavaPackageDef *additional_predefined_packages) {

    /* Define JavaPackage class */
    if (!JS_InitClass(cx, global_obj, 0, &JavaPackage_class,
                      0, 0, 0, JavaPackage_methods, 0, 0))
        return JS_FALSE;

    /* Add top-level packages, e.g. : java, netscape, sun */
    if (!pre_define_java_packages(cx, global_obj, standard_java_packages))
        return JS_FALSE;
    if (!pre_define_java_packages(cx, global_obj, additional_predefined_packages))
        return JS_FALSE;
    
    return JS_TRUE;
}
