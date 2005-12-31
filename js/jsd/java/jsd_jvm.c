/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*                        
* Code to start a Java VM (*some* code from the JRE)
*/                        

#include "jsdj.h"

/***************************************************************************/
#ifdef JSD_STANDALONE_JAVA_VM
#include "jre.h"

static char* more_classpath[] =
{
    {"..\\..\\jsdj\\dist\\classes"},
    {"..\\..\\jsdj\\dist\\classes\\ifc11.jar"},

/*
*     {"..\\..\\..\\jsdj\\dist\\classes"},
*     {"..\\..\\..\\jsdj\\dist\\classes\\ifc12.jar"},
*/

/*
*     {"..\\..\\samples\\jslogger"},
*     {"classes"},
*     {"ifc12.jar"},
*     {"jsd10.jar"},
*     {"jsdeb15.jar"}
*/
};
#define MORE_CLASSPATH_COUNT (sizeof(more_classpath)/sizeof(more_classpath[0]))

/*
* static char  main_class[]     = "callnative";
* static char  main_class[]     = "simpleIFC";
* static char* params[]         = {"16 Dec 1997"};
* #define PARAM_COUNT (sizeof(params)/sizeof(params[0]))
*/

/*
* static char  main_class[]     = "netscape/jslogger/JSLogger";
* static char  main_class[]     = "LaunchJSDebugger";
*/
static char  main_class[]     = "com/netscape/jsdebugging/ifcui/launcher/local/LaunchJSDebugger";
static char* params[]         = {NULL};
#define PARAM_COUNT           0

/* Globals */
static char **props;		/* User-defined properties */
static int numProps, maxProps;	/* Current, max number of properties */

static void *handle;
static JavaVM *jvm;
static JNIEnv *env;

/* Check for null value and return */
#define NULL_CHECK(e) if ((e) == 0) return 0

/*
 * Adds a user-defined system property definition.
 */
void AddProperty(char *def)
{
    if (numProps >= maxProps) {
        if (props == 0) {
            maxProps = 4;
            props = JRE_Malloc(maxProps * sizeof(char **));
        } else {
            char **tmp;
            maxProps *= 2;
            tmp = JRE_Malloc(maxProps * sizeof(char **));
            memcpy(tmp, props, numProps * sizeof(char **));
            free(props);
            props = tmp;
        }
    }
    props[numProps++] = def;
}

/*
 * Deletes a property definition by name.
 */
void DeleteProperty(const char *name)
{
    int i;

    for (i = 0; i < numProps; ) {
        char *def = props[i];
        char *c = strchr(def, '=');
        int n;
        if (c != 0) {
            n = c - def;
        } else {
            n = strlen(def);
        }
        if (strncmp(name, def, n) == 0) {
            if (i < --numProps) {
                memmove(&props[i], &props[i+1], (numProps-i) * sizeof(char **));
            }
        } else {
            i++;
        }
    }
}

/*
 * Creates an array of Java string objects from the specified array of C
 * strings. Returns 0 if the array could not be created.
 */
jarray NewStringArray(JNIEnv *env, char **cpp, int count)
{
    jclass cls;
    jarray ary;
    int i;

    NULL_CHECK(cls = (*env)->FindClass(env, "java/lang/String"));
    NULL_CHECK(ary = (*env)->NewObjectArray(env, count, cls, 0));
    for (i = 0; i < count; i++) {
        jstring str = (*env)->NewStringUTF(env, *cpp++);
        NULL_CHECK(str);
        (*env)->SetObjectArrayElement(env, ary, i, str);
        (*env)->DeleteLocalRef(env, str);
    }
    return ary;
}

/***************************************************************************/

static JNIEnv*
_CreateJavaVM(void)
{
    JNIEnv* env = NULL;
    JDK1_1InitArgs vmargs;
    JRESettings set;

    printf("Starting Java...\n");

    if(JRE_GetCurrentSettings(&set) != 0)
    {
        if(JRE_GetDefaultSettings(&set) != 0)
        {
            fprintf(stderr, "Could not locate Java runtime\n");
            return NULL;
        }
    }

    /* Load runtime library */
    handle = JRE_LoadLibrary(set.runtimeLib);
    if (handle == 0) {
        fprintf(stderr, "Could not load runtime library: %s\n",
                set.runtimeLib);
        return NULL;
    }

    /* Add pre-defined system properties */
    if (set.javaHome != 0) {
        char *def = JRE_Malloc(strlen(set.javaHome) + 16);
        sprintf(def, "java.home=%s", set.javaHome);
        AddProperty(def);
    }

    if (set.compiler != 0) {
        char *def = JRE_Malloc(strlen(set.compiler) + 16);
        sprintf(def, "java.compiler=%s", set.compiler);
        AddProperty(def);
    }

    /*
     * The following is used to specify that we require at least
     * JNI version 1.1. Currently, this field is not checked but
     * will be starting with JDK/JRE 1.2. The value returned after
     * calling JNI_GetDefaultJavaVMInitArgs() is the actual JNI version
     * supported, and is always higher that the requested version.
     */
    vmargs.version = 0x00010001;

    if (JRE_GetDefaultJavaVMInitArgs(handle, &vmargs) != 0) {
        fprintf(stderr, "Could not initialize Java VM\n");
        return NULL;
    }

    /* Tack on our classpath */
    if(MORE_CLASSPATH_COUNT)
    {
        int i;
        int size = strlen(set.classPath) + 1;
        char sep[2];

        sep[0] = PATH_SEPARATOR;
        sep[1] = 0;

        for(i = 0; i < MORE_CLASSPATH_COUNT; i++)
            size += strlen(more_classpath[i]) + 1;

        vmargs.classpath = malloc(size);
        if(vmargs.classpath == 0)
        {
            fprintf(stderr, "malloc error\n");
            return NULL;
        }

        strcpy(vmargs.classpath, set.classPath);
        for(i = 0; i < MORE_CLASSPATH_COUNT; i++)
        {
            strcat(vmargs.classpath, sep);
            strcat(vmargs.classpath, more_classpath[i]);
        }
    }
    else
    {
        vmargs.classpath = set.classPath;
    }

/*
*     fprintf(stderr, "classpath: %s\n", vmargs.classpath);
*/

    /* Set user-defined system properties for Java VM */
    if (props != 0) {
        if (numProps == maxProps) {
            char **tmp = JRE_Malloc((numProps + 1) * sizeof(char **));
            memcpy(tmp, props, numProps * sizeof(char **));
            free(props);
            props = tmp;
        }
        props[numProps] = 0;
        vmargs.properties = props;
    }


    /* verbose? */
/*
*     vmargs.verbose = JNI_TRUE;
*/

    /* Load and initialize Java VM */
    if (JRE_CreateJavaVM(handle, &jvm, &env, &vmargs) != 0) {
        fprintf(stderr, "Could not create Java VM\n");
        return NULL;
    }

    /* Free properties */
    if (props != 0) {
        free(props);
    }
    return env;
}

static JSBool
_StartDebuggerFE(JNIEnv* env)
{
    jclass clazz;
    jmethodID mid;
    jarray args;

    /* Find class */
    clazz = (*env)->FindClass(env, main_class);
    if (clazz == 0) {
        fprintf(stderr, "Class not found: %s\n", main_class);
        return JS_FALSE;
    }

    /* Find main method of class */
    mid = (*env)->GetStaticMethodID(env, clazz, "main",
    				    "([Ljava/lang/String;)V");
    if (mid == 0) {
        fprintf(stderr, "In class %s: public static void main(String args[])"
                        " is not defined\n");
        return JS_FALSE;
    }

    /* Invoke main method */
    args = NewStringArray(env, params, PARAM_COUNT);

    if (args == 0) {
        JRE_FatalError(env, "Couldn't build argument list for main\n");
    }
    (*env)->CallStaticVoidMethod(env, clazz, mid, args);
    if ((*env)->ExceptionOccurred(env)) {
        (*env)->ExceptionDescribe(env);
    }

    return JS_TRUE;
}

JNIEnv*
jsdj_CreateJavaVMAndStartDebugger(JSDJContext* jsdjc)
{
    JNIEnv* env = NULL;

    env = _CreateJavaVM();
    if( ! env )
        return NULL;

    jsdj_SetJNIEnvForCurrentThread(jsdjc, env);
    if( !  jsdj_RegisterNatives(jsdjc) )
        return NULL;
    if( ! _StartDebuggerFE(env) )
        return NULL;

    return env;
}


#endif /* JSD_STANDALONE_JAVA_VM */
/***************************************************************************/

