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

#include "prtypes.h"
#include "prprintf.h"
#include "prosdep.h"

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

JSObject *
define_JavaPackage(JSContext *cx, JSObject *parent_obj,
                   const char *obj_name, const char *path, int flags)
{
    JSObject *package_obj;
    JavaPackage_Private *package;

    package_obj = JS_DefineObject(cx, parent_obj, obj_name, &JavaPackage_class, 0,
                                  JSPROP_PERMANENT | JSPROP_READONLY);
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
static JSBool
JavaPackage_setProperty(JSContext *cx, JSObject *obj, jsval slot, jsval *vp)
{
    JavaPackage_Private *package = JS_GetPrivate(cx, obj);
    if (!package) {
        JS_ReportError(cx, "illegal attempt to add property to "
                           "JavaPackage prototype object");
        return JS_FALSE;
    }
    JS_ReportError(cx, "You may not add properties to %s, "
                       "as it is not a JavaScript object", package->path);
    return JS_FALSE;
}

static JSBool quiet_resolve_failure;

/*
 * Resolve a component name to be either the name of a class or a package.
 */
static JSBool
JavaPackage_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JavaPackage_Private *package;
    JSBool ok = JS_TRUE;
    jclass jclazz;
    char *subPath, *newPath;
    const char *path;
    JNIEnv *jEnv;
    
    package = (JavaPackage_Private *)JS_GetPrivate(cx, obj);
    if (!package) {
        fprintf(stderr, "JavaPackage_resolve: no private data!\n");
        return JS_FALSE;
    }
    path = package->path;
    subPath = JS_GetStringBytes(JSVAL_TO_STRING(id));

    newPath = PR_smprintf("%s%s%s", path, (path[0] ? "/" : ""), subPath);
    if (!newPath) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    jsj_MapJSContextToJSJThread(cx, &jEnv);
    if (!jEnv)
        return JS_FALSE;

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
        if (!newClass) {
            ok = JS_FALSE;
            goto out;
        }
    } else {

        /*
         * If there's no class of the given name, then we must be referring to
         * a package.  However, don't allow bogus sub-packages of pre-defined
         * system packages to be created.
         */
        if (JS_InstanceOf(cx, obj, &JavaPackage_class, NULL)) {
            JavaPackage_Private *package;

            package = JS_GetPrivate(cx, obj);
            if (package->flags & PKG_SYSTEM) {
                char *msg, *cp;

                /* Painful hack for pre_define_java_packages() */
                if (quiet_resolve_failure)
                    return JS_FALSE;

                msg = PR_smprintf("No Java system package with name \"%s\" was identified "
                                  "at initialization time and no Java class "
                                  "with that name exists either", newPath);

                /* Check for OOM */
                if (msg) {
                    /* Convert package of form "java/lang" to "java.lang" */
                    for (cp = msg; *cp != '\0'; cp++)
                        if (*cp == '/')
                            *cp = '.';
                    JS_ReportError(cx, msg);
                    free((char*)msg);
                }
                               
                return JS_FALSE;
            }
        }
        if (!define_JavaPackage(cx, obj, subPath, newPath, 0)) {
            ok = JS_FALSE;
            goto out;
        }
        
#ifdef DEBUG
        printf("JavaPackage \'%s/%s\' created\n", subPath, newPath);
#endif

    }
    
out:
    free(newPath);
    return ok;
}

static JSBool
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
    case JSTYPE_STRING:
        /* Convert '/' to '.' so that it looks like Java language syntax. */
        if (!package->path)
            break;
        name = PR_smprintf("[JavaPackage %s]", package->path);
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
static void
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
    JavaPackage_convert, JavaPackage_finalize
};

JavaPackageDef
standard_java_packages[] = {
    {"java",                NULL,   PKG_SYSTEM},
    {"java.awt",            NULL,   PKG_SYSTEM},
    {"java.awt.event",      NULL,   PKG_SYSTEM},
    {"java.awt.image",      NULL,   PKG_SYSTEM},
    {"java.awt.peer",       NULL,   PKG_SYSTEM},
    {"java.beans",          NULL,   PKG_SYSTEM},
    {"java.io",             NULL,   PKG_SYSTEM},
    {"java.lang",           NULL,   PKG_SYSTEM},
    {"java.lang.reflect",   NULL,   PKG_SYSTEM},
    {"java.math",           NULL,   PKG_SYSTEM},
    {"java.net",            NULL,   PKG_SYSTEM},
    {"java.text",           NULL,   PKG_SYSTEM},
    {"java.util",           NULL,   PKG_SYSTEM},
    {"java.util.zip",       NULL,   PKG_SYSTEM},
    {"netscape",            NULL,   PKG_SYSTEM},
    {"netscape.javascript", NULL,   PKG_SYSTEM},
    {"sun",                 NULL,   PKG_USER},
    {"Packages",            "",     PKG_USER},
    0
};

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
        package_name = path = NULL;

        parent_obj = global_obj;
        package_name = strdup(package_def->name);
        if (!package_name)
            goto out_of_memory;

        /* Walk the chain of JavaPackage objects to get to the parent of the
           rightmost sub-package in the fully-qualified package name. */
        for (simple_name = strtok(package_name, "."); 1; simple_name = strtok(NULL, ".")) {
            jsval v;

            if (!simple_name) {
                JS_ReportError(cx, "Package %s defined twice ?", package_name);
                goto error;
            }

            /* Check to see if the sub-package already exists */
            quiet_resolve_failure = JS_TRUE;
            package_exists = JS_LookupProperty(cx, parent_obj, simple_name, &v) && JSVAL_IS_OBJECT(v);
            quiet_resolve_failure = JS_FALSE;

            if (package_exists) {
                    parent_obj = JSVAL_TO_OBJECT(v);
                    continue;
            } else {
                /* New package objects should only be created at the terminal
                   sub-package in a fully-qualified package-name */
                if (strtok(NULL, ".")) {
                    JS_ReportError(cx, "Illegal predefined package definition for %s",
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
                parent_obj = define_JavaPackage(cx, parent_obj, simple_name, path, flags);
                if (!parent_obj)
                    goto error;
     
                free(path);
                break;
            }
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

/*
 * One-time initialization for the JavaPackage class.  (This is not
 * run once per thread, rather it's run once for a given JSContext.)
 */
JSBool
jsj_init_JavaPackage(JSContext *cx, JSObject *global_obj,
                     JavaPackageDef *additional_predefined_packages) {

    /* Define JavaPackage class */
    if (!JS_InitClass(cx, global_obj, 0, &JavaPackage_class, 0, 0, 0, 0, 0, 0))
        return JS_FALSE;

    /* Add top-level packages, e.g. : java, netscape, sun */
    if (!pre_define_java_packages(cx, global_obj, standard_java_packages))
        return JS_FALSE;
    if (!pre_define_java_packages(cx, global_obj, additional_predefined_packages))
        return JS_FALSE;
    
    return JS_TRUE;
}
