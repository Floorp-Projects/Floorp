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
 * Portions created by the Initial Developer are Copyright (C) 1997
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
* JNI natives to reflect JSD into Java 
*/                                     

#include "jsdj.h"
/* this can be included for type checking, but otherwise is not needed */
/* #include "_jni/jsdjnih.h" */

/***************************************************************************/

#define ASSERT_RETURN_VOID(x)   \
JS_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        JS_ASSERT(0);           \
        return;                 \
    }                           \
JS_END_MACRO

#define ASSERT_RETURN_VALUE(x,v)\
JS_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        JS_ASSERT(0);           \
        return v;               \
    }                           \
JS_END_MACRO

#define CHECK_RETURN_VOID(x)    \
JS_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        return;                 \
    }                           \
JS_END_MACRO

#define CHECK_RETURN_VALUE(x,v) \
JS_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        return v;               \
    }                           \
JS_END_MACRO

#define ASSERT_GOTO(x,w)        \
JS_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        JS_ASSERT(0);           \
        goto w;                 \
    }                           \
JS_END_MACRO

#define CHECK_GOTO(x,w)         \
JS_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        goto w;                 \
    }                           \
JS_END_MACRO

#ifdef DEBUG
#define ASSERT_CLEAR_EXCEPTION(e)   \
JS_BEGIN_MACRO                      \
    if((*e)->ExceptionOccurred(e))  \
    {                               \
        (*e)->ExceptionDescribe(e); \
        JS_ASSERT(0);               \
    }                               \
    (*e)->ExceptionClear(e);        \
JS_END_MACRO
#else  /* ! DEBUG */
#define ASSERT_CLEAR_EXCEPTION(e) (*e)->ExceptionClear(e)
#endif /* DEBUG */

#define CHECK_CLEAR_EXCEPTION(e) (*e)->ExceptionClear(e)

/***************************************************************************/

#ifdef DEBUG
void JSDJ_ASSERT_VALID_CONTEXT(JSDJContext* jsdjc)
{
    JS_ASSERT(jsdjc);
}
#endif /* DEBUG */

static JSHashNumber
_hash_root(const void *key)
{
    return ((JSHashNumber) key) >> 2; /* help lame MSVC1.5 on Win16 */
}

static JSBool       g_singleContextMode = JS_FALSE;
static JSHashTable* g_contextTable = NULL;
static void*        g_lock = NULL;
static int          g_contextCount = 0;
static JSDJContext* g_newestContext = NULL;


#define LOCK()                          \
JS_BEGIN_MACRO                          \
    if(!g_singleContextMode) {          \
        if(!g_lock)                     \
            g_lock = JSD_CreateLock();  \
        JS_ASSERT(g_lock);              \
        JSD_Lock(g_lock);               \
    }                                   \
JS_END_MACRO

#define UNLOCK()                        \
JS_BEGIN_MACRO                          \
    if(!g_singleContextMode) {          \
        JS_ASSERT(g_lock);              \
        JSD_Unlock(g_lock);             \
    }                                   \
JS_END_MACRO

/***************************************************************************/

JSDJContext*
jsdj_SimpleInitForSingleContextMode(JSDContext* jsdc,
                                    JSDJ_GetJNIEnvProc getEnvProc, void* user)
{
    JSDJContext* jsdjc;

    if(!jsdj_SetSingleContextMode())
        return NULL;

    if(!(jsdjc = jsdj_CreateContext()))
        return NULL;

    if(getEnvProc)
    {
        JSDJ_UserCallbacks callbacks;
        callbacks.size      = sizeof(JSDJ_UserCallbacks);
        callbacks.getJNIEnv = getEnvProc;
        jsdj_SetUserCallbacks(jsdjc, &callbacks, user);
    }

    if(jsdc)
        jsdj_SetJSDContext(jsdjc, jsdc);

    if(!jsdj_RegisterNatives(jsdjc))
    {
        jsdj_DestroyContext(jsdjc);
        jsdjc = NULL;
    }

    return jsdjc;
}

JSBool
jsdj_SetSingleContextMode()
{
    /* this can not be called after any contexts have been created */
    ASSERT_RETURN_VALUE(0 == g_contextCount, JS_FALSE);
    g_singleContextMode = JS_TRUE;
    return JS_TRUE;
}

JSDJContext*
jsdj_CreateContext()
{
    JSDJContext* jsdjc;

    ASSERT_RETURN_VALUE(!(g_singleContextMode && g_contextCount), NULL);

    jsdjc = (JSDJContext*) calloc(1, sizeof(JSDJContext));
    if(!g_singleContextMode)
    {
        jsdjc->envTable = JS_NewHashTable(256, _hash_root,
                                          JS_CompareValues, JS_CompareValues,
                                          NULL, NULL);
        if( ! jsdjc->envTable)
            return NULL;
    }
    LOCK();
    g_contextCount++;
    g_newestContext = jsdjc;
    UNLOCK();

    return jsdjc;
}

void
jsdj_DestroyContext(JSDJContext* jsdjc)
{
    LOCK();
    g_contextCount--;
    if(g_newestContext == jsdjc)
        g_newestContext = NULL;
    UNLOCK();

    if(jsdjc->envTable)
        free(jsdjc->envTable);
    free(jsdjc);
}

void
jsdj_SetUserCallbacks(JSDJContext* jsdjc, JSDJ_UserCallbacks* callbacks,
                      void* user)
{
    memset(&jsdjc->callbacks, 0, sizeof(JSDJ_UserCallbacks));
    if(callbacks)
        memcpy(&jsdjc->callbacks, callbacks,
               JS_MIN(sizeof(JSDJ_UserCallbacks), callbacks->size));
}

void
jsdj_SetJNIEnvForCurrentThread(JSDJContext* jsdjc, JNIEnv* env)
{
    if(g_singleContextMode && jsdjc->callbacks.getJNIEnv)
        return;

    LOCK();
    if(!g_contextTable)
        g_contextTable = JS_NewHashTable(256, _hash_root,
                                         JS_CompareValues, JS_CompareValues,
                                         NULL, NULL);
    if(g_contextTable)
        JS_HashTableAdd(g_contextTable, env, jsdjc);

    JS_HashTableAdd(jsdjc->envTable, JSD_CurrentThread(), (void*)env);
    UNLOCK();
}

JNIEnv*
jsdj_GetJNIEnvForCurrentThread(JSDJContext* jsdjc)
{
    JNIEnv* env;

    if(jsdjc->callbacks.getJNIEnv)
        return (*jsdjc->callbacks.getJNIEnv)(jsdjc,jsdjc->user);

    LOCK();
    env = JS_HashTableLookup(jsdjc->envTable, JSD_CurrentThread());
    UNLOCK();
    return env;
}

void
jsdj_SetJSDContext(JSDJContext* jsdjc, JSDContext* jsdc)
{
    JS_ASSERT(!jsdjc->ownJSDC);
    jsdjc->jsdc = jsdc;
}

JSDContext*
jsdj_GetJSDContext(JSDJContext* jsdjc)
{
    return jsdjc->jsdc;
}

static JSDJContext*
_getJSDJContext(JNIEnv *env)
{
    JSDJContext* jsdjc;

    if(g_singleContextMode)
        return g_newestContext;

    LOCK();
    jsdjc = JS_HashTableLookup(g_contextTable, env);
    /* XXX HACKAGE! */
    if(!jsdjc && g_contextCount == 1 && g_newestContext != NULL)
    {
        jsdjc = g_newestContext;
        jsdj_SetJNIEnvForCurrentThread(jsdjc, env);
    }
    UNLOCK();
    return jsdjc;
}

static JSContext* _getDefaultJSContext(JNIEnv *env)
{
    JSDJContext* jsdjc;
    ASSERT_RETURN_VALUE(jsdjc = _getJSDJContext(env), NULL);
    ASSERT_RETURN_VALUE(jsdjc->jsdc, NULL);
    return JSD_GetDefaultJSContext(jsdjc->jsdc);
}

/***************************************************************************/

static JSDValue* _getNativeJSDValue(JNIEnv *env, jobject self)
{
    jclass clazz;
    jfieldID fid;

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "_nativeJSVal", "I");
    ASSERT_RETURN_VALUE(fid, NULL);
    return (JSDValue*) (*env)->GetIntField(env, self, fid);
}

static jobject
_constructValue(JNIEnv *env, JSDContext* jsdc, JSDValue* jsdval)
{
    jclass clazz;
    jmethodID mid;
    jfieldID fid;
    jobject obj;

    clazz = (*env)->FindClass(env, "netscape/jsdebug/Value");
    ASSERT_GOTO(clazz, construct_value_fail);
    fid = (*env)->GetFieldID(env, clazz, "_nativeJSVal", "I");
    ASSERT_GOTO(fid, construct_value_fail);
    mid = (*env)->GetMethodID(env, clazz, "<init>", "()V");
    ASSERT_GOTO(mid, construct_value_fail);
    obj = (*env)->NewObject(env, clazz, mid);
    ASSERT_GOTO(obj, construct_value_fail);
    (*env)->SetIntField(env, obj, fid, (long) jsdval);
    ASSERT_CLEAR_EXCEPTION(env);
    return obj;
construct_value_fail:
    JSD_DropValue(jsdc, jsdval);
    ASSERT_CLEAR_EXCEPTION(env);
    return NULL;
}

static void
_destroyValue(JNIEnv *env, jobject self)
{
    JSDValue* jsdval;
    JSDJContext* jsdjc;
    ASSERT_RETURN_VOID(jsdval = _getNativeJSDValue(env, self));
    ASSERT_RETURN_VOID(jsdjc = _getJSDJContext(env));
    ASSERT_RETURN_VOID(jsdjc->jsdc);
    JSD_DropValue(jsdjc->jsdc, jsdval);
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

static jobject
_constructInteger(JNIEnv *env, long i)
{
    jclass clazz;
    jmethodID mid;

    clazz = (*env)->FindClass(env, "java/lang/Integer");
    ASSERT_RETURN_VALUE(clazz, NULL);
    mid = (*env)->GetMethodID(env, clazz, "<init>", "(I)V");
    ASSERT_RETURN_VALUE(mid, NULL);
    return (*env)->NewObject(env, clazz, mid, i);
}

static jmethodID
_getObjectMethodID(JNIEnv *env, jobject o, const char* method, const char* sig)
{
    jclass clazz = (*env)->GetObjectClass(env, o);
    ASSERT_RETURN_VALUE(clazz, NULL);
    return (*env)->GetMethodID(env, clazz, method, sig );
}

static jobject
_putHash(JNIEnv *env, jobject tbl, jobject key, jobject ob)
{
    jmethodID mid = _getObjectMethodID(env, tbl, "put",
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    ASSERT_RETURN_VALUE(mid, NULL);
    return (*env)->CallObjectMethod(env, tbl, mid, key, ob);
}

static jobject
_getHash(JNIEnv *env, jobject tbl, jobject key)
{
    jmethodID mid = _getObjectMethodID(env, tbl, "get",
        "(Ljava/lang/Object;)Ljava/lang/Object;");
    ASSERT_RETURN_VALUE(mid, NULL);
    return (*env)->CallObjectMethod(env, tbl, mid, key);
}

static jobject
_removeHash(JNIEnv *env, jobject tbl, jobject key)
{
    jmethodID mid = _getObjectMethodID(env, tbl, "remove",
        "(Ljava/lang/Object;)Ljava/lang/Object;");
    ASSERT_RETURN_VALUE(mid, NULL);
    return (*env)->CallObjectMethod(env, tbl, mid, key);
}

static jobject
_constructJSStackFrameInfo(JNIEnv *env,
                           JSDStackFrameInfo* jsdframe,
                           jobject threadstate)
{
    jclass clazz;
    jmethodID mid;
    jobject frame;

    clazz = (*env)->FindClass(env, "netscape/jsdebug/JSStackFrameInfo");
    ASSERT_RETURN_VALUE(clazz, NULL);
    mid = (*env)->GetMethodID(env, clazz, "<init>",
                              "(Lnetscape/jsdebug/JSThreadState;)V");
    ASSERT_RETURN_VALUE(mid, NULL);
    frame = (*env)->NewObject(env, clazz, mid, threadstate);
    if( frame )
    {
        jfieldID fid;

        fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetIntField(env, frame, fid, (long) jsdframe);

        /* XXX fill in additional fields... */

    }
    return frame;
}


static jobject
_constructJSThreadState(JNIEnv *env,
                        JSDStackFrameInfo* jsdframe,
                        JSDThreadState* jsdstate)
{
    jclass clazz;
    jmethodID mid;
    jobject threadState;

    clazz = (*env)->FindClass(env, "netscape/jsdebug/JSThreadState");
    ASSERT_RETURN_VALUE(clazz, NULL);
    mid = (*env)->GetMethodID(env, clazz, "<init>",
                              "()V");
    ASSERT_RETURN_VALUE(mid, NULL);
    threadState = (*env)->NewObject(env, clazz, mid);
    if( threadState )
    {
        jfieldID fid;

        /* valid */
        fid = (*env)->GetFieldID(env, clazz, "valid", "Z");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetBooleanField(env, threadState, fid, JNI_TRUE);

        /* currentFramePtr */
        fid = (*env)->GetFieldID(env, clazz, "currentFramePtr", "I");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetIntField(env, threadState, fid, (long) jsdframe);

        /* nativeThreadState */
        fid = (*env)->GetFieldID(env, clazz, "nativeThreadState", "I");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetIntField(env, threadState, fid, (long) jsdstate );

        /* continueState */
        fid = (*env)->GetFieldID(env, clazz, "continueState", "I");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetIntField(env, threadState, fid, DEBUG_STATE_RUN );

        /* XXX fill in additional fields... */

    }
    return threadState;
}

static jobject
_constructScript(JNIEnv *env, JSDContext* jsdc, JSDScript* jsdscript)
{
    jclass clazz;
    jmethodID mid;
    jobject script;
    char* url      = (char*)JSD_GetScriptFilename       (jsdc, jsdscript);
    char* function = (char*)JSD_GetScriptFunctionName   (jsdc, jsdscript);
    int   base     =        JSD_GetScriptBaseLineNumber (jsdc, jsdscript);
    int   extent   =        JSD_GetScriptLineExtent     (jsdc, jsdscript);

    if( ! url )
    {
        return NULL;
        /* url = ""; */
    }

    clazz = (*env)->FindClass(env, "netscape/jsdebug/Script");
    ASSERT_RETURN_VALUE(clazz, NULL);
    mid = (*env)->GetMethodID(env, clazz, "<init>", "()V");
    ASSERT_RETURN_VALUE(mid, NULL);
    script = (*env)->NewObject(env, clazz, mid);
    if( script )
    {
        jfieldID fid;

        /* _url */
        fid = (*env)->GetFieldID(env, clazz, "_url", "Ljava/lang/String;");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetObjectField(env, script, fid,
                               (*env)->NewStringUTF(env, url));

        /* _function */
        fid = (*env)->GetFieldID(env, clazz, "_function", "Ljava/lang/String;");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetObjectField(env, script, fid,
                    function ? (*env)->NewStringUTF(env, function) : NULL);

        /* _baseLineNumber */
        fid = (*env)->GetFieldID(env, clazz, "_baseLineNumber", "I");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetIntField(env, script, fid, base);

        /* _lineExtent */
        fid = (*env)->GetFieldID(env, clazz, "_lineExtent", "I");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetIntField(env, script, fid, extent);

        /* _nativePtr */
        fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetIntField(env, script, fid, (long)jsdscript);

        /* XXX fill in additional fields... */
    }
    return script;
}

static jobject
_constructJSPC(JNIEnv *env, jobject script, long pc)
{
    jclass clazz;
    jmethodID mid;
    jobject pcOb;

    clazz = (*env)->FindClass(env, "netscape/jsdebug/JSPC" );
    ASSERT_RETURN_VALUE(clazz, NULL);
    mid = (*env)->GetMethodID(env, clazz, "<init>",
                              "(Lnetscape/jsdebug/Script;I)V");
    ASSERT_RETURN_VALUE(mid, NULL);
    pcOb = (*env)->NewObject(env, clazz, mid, script, pc);
    /* XXX fill in additional fields... */
    return pcOb;
}

static jobject
_constructJSSourceLocation(JNIEnv *env, jobject pc, long line)
{
    jclass clazz;
    jmethodID mid;
    jobject loc;

    clazz = (*env)->FindClass(env, "netscape/jsdebug/JSSourceLocation");
    ASSERT_RETURN_VALUE(clazz, NULL);
    mid = (*env)->GetMethodID(env, clazz, "<init>",
                              "(Lnetscape/jsdebug/JSPC;I)V");
    ASSERT_RETURN_VALUE(mid, NULL);
    loc = (*env)->NewObject(env, clazz, mid, pc, line);
    /* XXX fill in additional fields... */
    return loc;
}

static jobject
_getScriptHook(JNIEnv *env, JSDJContext* jsdjc)
{
    jclass clazz;
    jfieldID fid;

    clazz = (*env)->GetObjectClass(env, jsdjc->controller);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "scriptHook",
                             "Lnetscape/jsdebug/ScriptHook;");
    ASSERT_RETURN_VALUE(fid, NULL);
    return (*env)->GetObjectField(env, jsdjc->controller, fid);
}

static jobject
_getInterruptHook(JNIEnv *env, JSDJContext* jsdjc)
{
    jclass clazz;
    jfieldID fid;

    clazz = (*env)->GetObjectClass(env, jsdjc->controller);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "interruptHook",
                             "Lnetscape/jsdebug/InterruptHook;");
    ASSERT_RETURN_VALUE(fid, NULL);
    return (*env)->GetObjectField(env, jsdjc->controller, fid);
}

static jobject
_getDebugBreakHook(JNIEnv *env, JSDJContext* jsdjc)
{
    jclass clazz;
    jfieldID fid;

    clazz = (*env)->GetObjectClass(env, jsdjc->controller);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "debugBreakHook",
                             "Lnetscape/jsdebug/DebugBreakHook;");
    ASSERT_RETURN_VALUE(fid, NULL);
    return (*env)->GetObjectField(env, jsdjc->controller, fid);
}

static jobject
_getInstructionHook(JNIEnv *env, JSDJContext* jsdjc, jobject pc)
{
    jmethodID mid;

    mid = _getObjectMethodID(env, jsdjc->controller, "getInstructionHook0",
                "(Lnetscape/jsdebug/PC;)Lnetscape/jsdebug/InstructionHook;");
    ASSERT_RETURN_VALUE(mid, NULL);
    return (*env)->CallObjectMethod(env, jsdjc->controller, mid, pc);
}

static jobject
_getErrorReporter(JNIEnv *env, JSDJContext* jsdjc)
{
    jclass clazz;
    jfieldID fid;

    clazz = (*env)->GetObjectClass(env, jsdjc->controller);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "errorReporter",
                             "Lnetscape/jsdebug/JSErrorReporter;");
    ASSERT_RETURN_VALUE(fid, NULL);
    return (*env)->GetObjectField(env, jsdjc->controller, fid);
}

static jobject
_getScriptTable(JNIEnv *env, JSDJContext* jsdjc)
{
    jclass clazz;
    jfieldID fid;

    clazz = (*env)->GetObjectClass(env, jsdjc->controller);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "scriptTable",
                             "Lnetscape/util/Hashtable;");
    ASSERT_RETURN_VALUE(fid, NULL);
    return (*env)->GetObjectField(env, jsdjc->controller, fid);
}

static jobject
_scriptObFromJSDScriptPtr(JNIEnv *env, JSDJContext* jsdjc, JSDScript* jsdscript)
{
    jobject tbl;
    jobject key;

    tbl = _getScriptTable(env, jsdjc);
    ASSERT_RETURN_VALUE(tbl, NULL);
    key = _constructInteger(env,(long)jsdscript);
    ASSERT_RETURN_VALUE(key, NULL);
    return _getHash(env, tbl, key);
}

static JSDThreadState*
_JSDThreadStateFromJSStackFrameInfo(JNIEnv *env, jobject frame)
{
    jclass clazz;
    jfieldID fid;
    jobject threadState;

    clazz = (*env)->GetObjectClass(env, frame);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "threadState",
                             "Lnetscape/jsdebug/ThreadStateBase;");
    ASSERT_RETURN_VALUE(fid, NULL);
    threadState = (*env)->GetObjectField(env, frame, fid);
    ASSERT_RETURN_VALUE(threadState, NULL);

    clazz = (*env)->GetObjectClass(env, threadState);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "nativeThreadState", "I");
    ASSERT_RETURN_VALUE(fid, NULL);
    return (JSDThreadState*) (*env)->GetIntField(env, threadState, fid);
}

static JSDStackFrameInfo*
_JSDStackFrameInfoFromJSStackFrameInfo(JNIEnv *env, jobject frame)
{
    jclass clazz;
    jfieldID fid;

    clazz = (*env)->GetObjectClass(env, frame);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
    ASSERT_RETURN_VALUE(fid, NULL);
    return (JSDStackFrameInfo*) (*env)->GetIntField(env, frame, fid);
}

static char*
_allocCString(JNIEnv *env, jstring str)
{
    jsize len;
    char* buf;

    len = (*env)->GetStringUTFLength(env, str);
    buf = malloc(len+1);
    ASSERT_RETURN_VALUE(buf, NULL);
    buf[0] = 0;
    if(len)
    {
        const jbyte* temp = (*env)->GetStringUTFChars(env, str, NULL);
        if(temp)
        {
            strncpy(buf, temp, len);
            (*env)->ReleaseStringUTFChars(env, str, temp);
            buf[len] = 0;
        }
    }
    return buf;
}


static jstring
_NewStringUTF_No_Z(JNIEnv *env, const char* str, uintN len)
{
    jstring textOb;
    char* temp;

    temp = malloc(len+1);
    ASSERT_RETURN_VALUE(temp, NULL);

    strncpy(temp, str, len);
    temp[len] = 0;
    textOb = (*env)->NewStringUTF(env, temp);
    free(temp);
    return textOb;
}

/***************************************************************************/

void JS_DLL_CALLBACK
_scriptHook(JSDContext* jsdc,
            JSDScript*  jsdscript,
            JSBool      creating,
            void*       callerdata)
{
    JNIEnv* env;
    JSDJContext* jsdjc;
    jobject script;
    jobject tbl;
    jobject key;
    jobject hook;
    jmethodID mid;

    jsdjc = (JSDJContext*) callerdata;
    ASSERT_RETURN_VOID(jsdjc && jsdjc->controller);

    env = jsdj_GetJNIEnvForCurrentThread(jsdjc);
    ASSERT_RETURN_VOID(env);

    ASSERT_CLEAR_EXCEPTION(env);

    tbl = _getScriptTable(env, jsdjc);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(tbl);

    key = _constructInteger(env, (long)jsdscript);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(key);

    hook = _getScriptHook(env, jsdjc);
    CHECK_CLEAR_EXCEPTION(env);

    if( creating )
    {
        script = _constructScript(env, jsdc, jsdscript);
        ASSERT_CLEAR_EXCEPTION(env);
        ASSERT_RETURN_VOID(script);

        /* add it to the hash table */
        _putHash( env, tbl, key, script );
        ASSERT_CLEAR_EXCEPTION(env);
        if( hook )
        {
            /* call the hook */
            mid = _getObjectMethodID(env, hook, "justLoadedScript",
                                     "(Lnetscape/jsdebug/Script;)V" );
            ASSERT_CLEAR_EXCEPTION(env);
            ASSERT_RETURN_VOID(mid);
            (*env)->CallVoidMethod(env, hook, mid, script);
            ASSERT_CLEAR_EXCEPTION(env);
        }
    }
    else
    {
        /* find Java Object for Script */
        script = _getHash(env, tbl, key);
        ASSERT_CLEAR_EXCEPTION(env);
        ASSERT_RETURN_VOID(script);

        /* remove it from the hash table  */
        _removeHash(env, tbl, key);
        ASSERT_CLEAR_EXCEPTION(env);

        if( hook )
        {
            /* call the hook */
            mid = _getObjectMethodID(env, hook, "aboutToUnloadScript",
                                     "(Lnetscape/jsdebug/Script;)V" );
            ASSERT_CLEAR_EXCEPTION(env);
            ASSERT_RETURN_VOID(mid);
            (*env)->CallVoidMethod(env, hook, mid, script);
            ASSERT_CLEAR_EXCEPTION(env);
        }

        /* set the Script as invalid */
        mid = _getObjectMethodID(env, script, "_setInvalid",
                                 "()V" );
        ASSERT_CLEAR_EXCEPTION(env);
        ASSERT_RETURN_VOID(mid);
        (*env)->CallVoidMethod(env, script, mid);
        ASSERT_CLEAR_EXCEPTION(env);
    }
}

/***************************************************************************/
uintN JS_DLL_CALLBACK
_executionHook(JSDContext*     jsdc,
               JSDThreadState* jsdstate,
               uintN           type,
               void*           callerdata,
               jsval*          rval)
{
    JNIEnv* env;
    JSDJContext* jsdjc;
    JSDStackFrameInfo* jsdframe;
    JSDScript*  jsdscript;
    jobject script;
    jobject threadState;
    jobject pcOb;
    jobject tbl;
    jobject key;
    jobject hook;
    jmethodID mid;
    jfieldID fid;
    jclass clazz;
    int pc;

    jsdjc = (JSDJContext*) callerdata;
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->controller, JSD_HOOK_RETURN_HOOK_ERROR);

    env = jsdj_GetJNIEnvForCurrentThread(jsdjc);
    ASSERT_RETURN_VALUE(env, JSD_HOOK_RETURN_HOOK_ERROR);
    ASSERT_CLEAR_EXCEPTION(env);

    /* get the JSDStackFrameInfo */
    jsdframe = JSD_GetStackFrame(jsdc, jsdstate);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdframe, JSD_HOOK_RETURN_HOOK_ERROR);

    /* get the JSDScript */
    jsdscript = JSD_GetScriptForStackFrame(jsdc, jsdstate, jsdframe);
    ASSERT_RETURN_VALUE(jsdscript, JSD_HOOK_RETURN_HOOK_ERROR);

    /* find Java Object for Script */

    tbl = _getScriptTable(env, jsdjc);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(tbl, JSD_HOOK_RETURN_HOOK_ERROR);

    key = _constructInteger(env, (long)jsdscript);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(key, JSD_HOOK_RETURN_HOOK_ERROR);

    script = _getHash(env, tbl, key);
    CHECK_CLEAR_EXCEPTION(env);
    CHECK_RETURN_VALUE(script, JSD_HOOK_RETURN_HOOK_ERROR);

    /* generate a JSPC */
    pc = JSD_GetPCForStackFrame(jsdc, jsdstate, jsdframe);
    pcOb = _constructJSPC(env, script, pc);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(pcOb, JSD_HOOK_RETURN_HOOK_ERROR);

    /* build a JSThreadState */
    threadState = _constructJSThreadState(env, jsdframe, jsdstate);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(threadState, JSD_HOOK_RETURN_HOOK_ERROR);

    /* find and call the appropriate Hook */
    if( JSD_HOOK_INTERRUPTED == type )
    {
        /* clear the JSD level hook (must reset on next sendInterrupt0()*/
        JSD_ClearInterruptHook(jsdc);

        hook = _getInterruptHook(env, jsdjc);
        CHECK_CLEAR_EXCEPTION(env);
        CHECK_RETURN_VALUE(hook, JSD_HOOK_RETURN_HOOK_ERROR);

        /* call the hook */
        mid = _getObjectMethodID(env, hook, "aboutToExecute",
                "(Lnetscape/jsdebug/ThreadStateBase;Lnetscape/jsdebug/PC;)V");
        ASSERT_CLEAR_EXCEPTION(env);
        ASSERT_RETURN_VALUE(mid, JSD_HOOK_RETURN_HOOK_ERROR);
        (*env)->CallVoidMethod(env, hook, mid, threadState, pcOb);
        ASSERT_CLEAR_EXCEPTION(env);
    }
    else if( JSD_HOOK_DEBUG_REQUESTED == type )
    {
        hook = _getDebugBreakHook(env, jsdjc);
        CHECK_CLEAR_EXCEPTION(env);
        CHECK_RETURN_VALUE(hook, JSD_HOOK_RETURN_HOOK_ERROR);

        /* call the hook */
        mid = _getObjectMethodID(env, hook, "aboutToExecute",
                "(Lnetscape/jsdebug/ThreadStateBase;Lnetscape/jsdebug/PC;)V");
        ASSERT_CLEAR_EXCEPTION(env);
        ASSERT_RETURN_VALUE(mid, JSD_HOOK_RETURN_HOOK_ERROR);
        (*env)->CallVoidMethod(env, hook, mid, threadState, pcOb);
        ASSERT_CLEAR_EXCEPTION(env);
    }
    else if( JSD_HOOK_BREAKPOINT == type )
    {
        hook = _getInstructionHook(env, jsdjc, pcOb);
        CHECK_CLEAR_EXCEPTION(env);
        CHECK_RETURN_VALUE(hook, JSD_HOOK_RETURN_HOOK_ERROR);

        /* call the hook */
        mid = _getObjectMethodID(env, hook, "aboutToExecute",
                "(Lnetscape/jsdebug/ThreadStateBase;)V");
        ASSERT_CLEAR_EXCEPTION(env);
        ASSERT_RETURN_VALUE(mid, JSD_HOOK_RETURN_HOOK_ERROR);
        (*env)->CallVoidMethod(env, hook, mid, threadState);
        ASSERT_CLEAR_EXCEPTION(env);
    }

    /* should we abort? */

    clazz = (*env)->GetObjectClass(env, threadState);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, JSD_HOOK_RETURN_CONTINUE);

    fid = (*env)->GetFieldID(env, clazz, "continueState", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, JSD_HOOK_RETURN_CONTINUE);

    if( DEBUG_STATE_THROW == (*env)->GetIntField(env, threadState, fid) )
        return JSD_HOOK_RETURN_ABORT;

    return JSD_HOOK_RETURN_CONTINUE;
}

/***************************************************************************/

uintN JS_DLL_CALLBACK
_errorReporter( JSDContext*     jsdc,
                JSContext*      cx,
                const char*     message,
                JSErrorReport*  report,
                void*           callerdata )
{
    JNIEnv* env;
    JSDJContext* jsdjc;
    jmethodID mid;
    jobject reporter;
    jobject msg         = NULL;
    jobject filename    = NULL;
    int     lineno      = 0;
    jobject linebuf     = NULL;
    int     tokenOffset = 0;
    int     retval;

    jsdjc = (JSDJContext*) callerdata;
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->controller, JSD_ERROR_REPORTER_PASS_ALONG);

    env = jsdj_GetJNIEnvForCurrentThread(jsdjc);
    ASSERT_RETURN_VALUE(env, JSD_ERROR_REPORTER_PASS_ALONG);
    ASSERT_CLEAR_EXCEPTION(env);

    reporter = _getErrorReporter(env, jsdjc);
    CHECK_CLEAR_EXCEPTION(env);
    CHECK_RETURN_VALUE(reporter, JSD_ERROR_REPORTER_PASS_ALONG);

    if( message )
        msg = (*env)->NewStringUTF(env, message);
    if( report && report->filename )
        filename = (*env)->NewStringUTF(env, report->filename);
    if( report && report->linebuf )
        linebuf = (*env)->NewStringUTF(env, report->linebuf);
    if( report )
        lineno = report->lineno;
    if( report && report->linebuf && report->tokenptr )
        tokenOffset = report->tokenptr - report->linebuf;

    /* call the hook */
    mid = _getObjectMethodID(env, reporter, "reportError",
            "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;I)I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(mid, JSD_HOOK_RETURN_HOOK_ERROR);

    retval = (*env)->CallIntMethod(env, reporter, mid,
                                   msg, filename, lineno, linebuf, tokenOffset);
    ASSERT_CLEAR_EXCEPTION(env);
    return retval;
}

/***************************************************************************/
/*
 * Class:     netscape_jsdebug_DebugController
 * Method:    setInstructionHook0
 * Signature: (Lnetscape/jsdebug/PC;)V
 */
JNIEXPORT void JNICALL Java_netscape_jsdebug_DebugController_setInstructionHook0
  (JNIEnv *env, jobject self, jobject pcOb)
{
    jobject script;
    JSDScript* jsdscript;
    uintN pc;
    JSDJContext* jsdjc;
    jmethodID mid;
    jclass clazz;
    jfieldID fid;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VOID(jsdjc && jsdjc->jsdc && jsdjc->controller);

    mid = _getObjectMethodID(env, pcOb, "getScript",
                "()Lnetscape/jsdebug/Script;");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(mid);

    script = (*env)->CallObjectMethod(env, pcOb, mid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(script);

    clazz = (*env)->FindClass(env, "netscape/jsdebug/Script");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(clazz);

    fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(fid);

    jsdscript = (JSDScript*) (*env)->GetIntField(env, script, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(jsdscript);

    mid = _getObjectMethodID(env, pcOb, "getPC", "()I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(mid);

    pc = (uintN) (*env)->CallIntMethod(env, pcOb, mid);
    ASSERT_CLEAR_EXCEPTION(env);

    JSD_SetExecutionHook(jsdjc->jsdc, jsdscript, pc, _executionHook, jsdjc);
}

/*
 * Class:     netscape_jsdebug_DebugController
 * Method:    sendInterrupt0
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_netscape_jsdebug_DebugController_sendInterrupt0
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VOID(jsdjc && jsdjc->jsdc && jsdjc->controller);
    ASSERT_CLEAR_EXCEPTION(env);
    JSD_SetInterruptHook(jsdjc->jsdc, _executionHook, jsdjc);
}

/*
 * Class:     netscape_jsdebug_DebugController
 * Method:    _setController
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_netscape_jsdebug_DebugController__1setController
  (JNIEnv *env, jobject self, jboolean on)
{
    /*
    ** XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
    ** NOTE: in Navigator we turn native debug support on and off in this
    **       function. Here we just connect/disconnect from that support.
    ** XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
    */

    JSDJContext* jsdjc;
    jclass clazz;
    jfieldID fid;

    jsdjc = _getJSDJContext(env);
    ASSERT_GOTO(jsdjc, LABEL_FAILURE);
    ASSERT_CLEAR_EXCEPTION(env);

    if(on)
    {
        if(!jsdjc->jsdc)
        {
            /*
            * This is a hack to support Navigator. JSD_DebuggerOn ASSERTs in
            * JSD that someone has previously called JSD_SetUserCallbacks
            * (which is done in libmocha/lm_jsd). Here we are turning on
            * debug support because a debugger applet has been started.
            */
            ASSERT_GOTO(jsdjc->jsdc = JSD_DebuggerOn(), LABEL_FAILURE);
            jsdjc->ownJSDC = JS_TRUE;
        }
        jsdjc->controller = (*env)->NewGlobalRef(env,self);
        JSD_SetScriptHook(jsdjc->jsdc, _scriptHook, jsdjc);
        JSD_SetErrorReporter(jsdjc->jsdc, _errorReporter, jsdjc);
        JSD_SetDebugBreakHook(jsdjc->jsdc, _executionHook, jsdjc);
    }
    else
    {
        JSD_SetDebugBreakHook(jsdjc->jsdc, NULL, NULL);
        JSD_SetErrorReporter(jsdjc->jsdc, NULL, NULL);
        JSD_SetScriptHook(jsdjc->jsdc, NULL, NULL);
        (*env)->DeleteGlobalRef(env,jsdjc->controller);
        ASSERT_CLEAR_EXCEPTION(env);
        jsdjc->controller = NULL;
        jsdjc = NULL;   /* for setting below... */
        if(jsdjc->ownJSDC)
        {
            JSD_DebuggerOff(jsdjc->jsdc);
            jsdjc->jsdc = NULL;
        }
    }

    /* store the _nativeContext in the controller */
    clazz = (*env)->FindClass(env, "netscape/jsdebug/DebugController");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_GOTO(clazz, LABEL_FAILURE);

    fid = (*env)->GetFieldID(env, clazz, "_nativeContext", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_GOTO(fid, LABEL_FAILURE);
    (*env)->SetIntField(env, self, fid, (long)jsdjc);
    ASSERT_CLEAR_EXCEPTION(env);

    if(jsdjc->callbacks.startStop)
        (*jsdjc->callbacks.startStop)(jsdjc,
                                      on ? JSDJ_START_SUCCESS : JSDJ_STOP,
                                      jsdjc->user);
    return;

LABEL_FAILURE:
    if(jsdjc->callbacks.startStop)
        (*jsdjc->callbacks.startStop)(jsdjc,
                                      on ? JSDJ_START_FAILURE : JSDJ_STOP,
                                      jsdjc->user);
}

/*
 * Class:     netscape_jsdebug_DebugController
 * Method:    executeScriptInStackFrame0
 * Signature: (Lnetscape/jsdebug/JSStackFrameInfo;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_netscape_jsdebug_DebugController_executeScriptInStackFrame0
  (JNIEnv *env, jobject self, jobject frame, jstring text,
   jstring filename, jint lineno)
{
    JSDJContext* jsdjc;
    JSDThreadState* jsdthreadstate;
    JSDStackFrameInfo* jsdframe;
    char* filenameC;
    char* srcC;
    JSString* jsstr;
    jsval rval;
    JSBool success;
    int srclen;
    jstring retval;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, NULL);
    ASSERT_CLEAR_EXCEPTION(env);

    jsdthreadstate = _JSDThreadStateFromJSStackFrameInfo(env, frame);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdthreadstate, NULL);

    jsdframe = _JSDStackFrameInfoFromJSStackFrameInfo(env, frame);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdframe, NULL);

    filenameC = _allocCString(env, filename);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(filenameC, NULL);

    srcC = _allocCString(env, text);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(srcC, NULL);

    srclen = strlen(srcC);

    success = JSD_EvaluateScriptInStackFrame(jsdjc->jsdc,
                                             jsdthreadstate, jsdframe,
                                             srcC, srclen,
                                             filenameC, lineno, &rval);
    free(filenameC);
    free(srcC);

    if( ! success )
        return NULL;

    if( JSVAL_IS_NULL(rval) || JSVAL_IS_VOID(rval) )
        return NULL;

    jsstr = JSD_ValToStringInStackFrame(jsdjc->jsdc, jsdthreadstate,
                                        jsdframe, rval);
    if( ! jsstr )
        return NULL;

    retval = (*env)->NewString(env,JS_GetStringChars(jsstr),
                                 JS_GetStringLength(jsstr));
    ASSERT_CLEAR_EXCEPTION(env);
    return retval;
}

/*
 * Class:     netscape_jsdebug_DebugController
 * Method:    executeScriptInStackFrameValue0
 * Signature: (Lnetscape/jsdebug/JSStackFrameInfo;Ljava/lang/String;Ljava/lang/String;I)Lnetscape/jsdebug/Value;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_DebugController_executeScriptInStackFrameValue0
  (JNIEnv *env, jobject self, jobject frame, jstring text,
   jstring filename, jint lineno)
{
    JSDJContext* jsdjc;
    JSDThreadState* jsdthreadstate;
    JSDStackFrameInfo* jsdframe;
    char* filenameC;
    char* srcC;
    jsval rval;
    JSBool success;
    int srclen;
    JSDValue* jsdval;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, NULL);
    ASSERT_CLEAR_EXCEPTION(env);

    jsdthreadstate = _JSDThreadStateFromJSStackFrameInfo(env, frame);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdthreadstate, NULL);

    jsdframe = _JSDStackFrameInfoFromJSStackFrameInfo(env, frame);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdframe, NULL);

    filenameC = _allocCString(env, filename);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(filenameC, NULL);

    srcC = _allocCString(env, text);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(srcC, NULL);

    srclen = strlen(srcC);

    success = JSD_EvaluateScriptInStackFrame(jsdjc->jsdc,
                                             jsdthreadstate, jsdframe,
                                             srcC, srclen,
                                             filenameC, lineno, &rval);
    free(filenameC);
    free(srcC);

    if( ! success )
        return NULL;

    jsdval = JSD_NewValue(jsdjc->jsdc, rval);
    if( ! jsdval )
        return NULL;

    return _constructValue(env, jsdjc->jsdc, jsdval);
}

/*
 * Class:     netscape_jsdebug_DebugController
 * Method:    getNativeMajorVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_netscape_jsdebug_DebugController_getNativeMajorVersion
  (JNIEnv *env, jclass clazz)
{
    ASSERT_CLEAR_EXCEPTION(env);
    return (jint) JSD_GetMajorVersion();
}

/*
 * Class:     netscape_jsdebug_DebugController
 * Method:    getNativeMinorVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_netscape_jsdebug_DebugController_getNativeMinorVersion
  (JNIEnv *env, jclass clazz)
{
    ASSERT_CLEAR_EXCEPTION(env);
    return (jint) JSD_GetMinorVersion();
}

/***************************************************************************/
/*
 * Class:     netscape_jsdebug_JSPC
 * Method:    getSourceLocation
 * Signature: ()Lnetscape/jsdebug/SourceLocation;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSPC_getSourceLocation
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    JSDScript* jsdscript;
    jobject script;
    jobject newPCOb;
    jclass clazz;
    jfieldID fid;
    int line;
    int newpc;
    int pc;
    jobject retval;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, NULL);
    ASSERT_CLEAR_EXCEPTION(env);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, NULL);

    fid = (*env)->GetFieldID(env, clazz, "script", "Lnetscape/jsdebug/Script;");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);

    script = (*env)->GetObjectField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(script, NULL);

    clazz = (*env)->GetObjectClass(env, script);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, NULL);

    fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);

    jsdscript = (JSDScript*) (*env)->GetIntField(env, script, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdscript, NULL);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, NULL);

    fid = (*env)->GetFieldID(env, clazz, "pc", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);

    pc = (*env)->GetIntField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    line = JSD_GetClosestLine(jsdjc->jsdc, jsdscript, pc);
    newpc = JSD_GetClosestPC(jsdjc->jsdc, jsdscript, line);

    newPCOb = _constructJSPC(env, script, newpc );
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(newPCOb, NULL);

    retval = _constructJSSourceLocation(env, newPCOb, line);
    ASSERT_CLEAR_EXCEPTION(env);
    return retval;
}

/***************************************************************************/
/*
 * Class:     netscape_jsdebug_JSSourceTextProvider
 * Method:    loadSourceTextItem0
 * Signature: (Ljava/lang/String;)Lnetscape/jsdebug/SourceTextItem;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSSourceTextProvider_loadSourceTextItem0
  (JNIEnv *env, jobject self, jstring url)
{
    /* this should attempt to load the source for the indicated URL */
    ASSERT_CLEAR_EXCEPTION(env);
    return NULL;
}

/*
 * Class:     netscape_jsdebug_JSSourceTextProvider
 * Method:    refreshSourceTextVector
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_netscape_jsdebug_JSSourceTextProvider_refreshSourceTextVector
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    jclass clazz;
    jmethodID mid;
    jclass clazzSelf;
    jclass clazzSTI;
    jmethodID midFindItem;
    jmethodID midSTIctor;
    jmethodID midSTIsetText;
    jmethodID midSTIsetStatus;
    jmethodID midSTIsetDirty;
    jmethodID midVectorAddElement;
    jfieldID fid;
    jobject vec;
    jobject itemOb;
    JSDSourceText* iterp = 0;
    JSDSourceText* item;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VOID(jsdjc && jsdjc->jsdc && jsdjc->controller);
    ASSERT_CLEAR_EXCEPTION(env);

    /* gather for later use */
    clazzSelf = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(clazzSelf);

    midFindItem = (*env)->GetMethodID(env, clazzSelf, "findSourceTextItem0",
                      "(Ljava/lang/String;)Lnetscape/jsdebug/SourceTextItem;");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(midFindItem);

    clazzSTI = (*env)->FindClass(env, "netscape/jsdebug/SourceTextItem");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(clazzSTI);

    midSTIctor = (*env)->GetMethodID(env, clazzSTI, "<init>",
                     "(Ljava/lang/String;Ljava/lang/String;I)V");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(midSTIctor);

    midSTIsetText = (*env)->GetMethodID(env, clazzSTI, "setText",
                     "(Ljava/lang/String;)V");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(midSTIsetText);

    midSTIsetStatus = (*env)->GetMethodID(env, clazzSTI, "setStatus", "(I)V");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(midSTIsetStatus);

    midSTIsetDirty = (*env)->GetMethodID(env, clazzSTI, "setDirty", "(Z)V");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(midSTIsetDirty);

    /* create new vector */
    clazz = (*env)->FindClass(env, "netscape/util/Vector");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(clazz);
    mid = (*env)->GetMethodID(env, clazz, "<init>", "()V");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(mid);
    vec = (*env)->NewObject(env, clazz, mid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(vec);

    /* get method id for later */
    midVectorAddElement = (*env)->GetMethodID(env, clazz, "addElement",
                                              "(Ljava/lang/Object;)V");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(midVectorAddElement);


    /* lock the native subsystem */
    JSD_LockSourceTextSubsystem(jsdjc->jsdc);

    /* iterate through the native items */
    while( 0 != (item = JSD_IterateSources(jsdjc->jsdc, &iterp)) )
    {
        const char* url;
        jstring urlOb;
        int status;

        status = JSD_GetSourceStatus(jsdjc->jsdc,item);

        /* try to find Java object */
        url = JSD_GetSourceURL(jsdjc->jsdc, item);
        if( ! url || ! strlen(url) ) /* ignoring those with no url */
            continue;

        urlOb = (*env)->NewStringUTF(env, url);
        ASSERT_CLEAR_EXCEPTION(env);
        if( ! urlOb )
            continue;

        itemOb = (*env)->CallObjectMethod(env, self, midFindItem, urlOb);
        ASSERT_CLEAR_EXCEPTION(env);

        if( ! itemOb )
        {
            /* if not found then generate new item */

            jstring textOb;
            const char* str;
            int len;

            if( ! JSD_GetSourceText(jsdjc->jsdc, item, &str, &len ) )
            {
                str = "";
                len = 0;
            }

            /*
            * XXX this is pretty lame...
            * makeJavaString() used to take a string len and
            * didn't require zero termination.
            * NewStringUTF() ASSUMES zero termination.
            * So, this code makes an otherwise unnecessary copy
            * of the string.
            *
            * XXX the JSD function should (in the future)
            * allow for Unicode source text :)
            */
            textOb = _NewStringUTF_No_Z(env, str, len);
            ASSERT_CLEAR_EXCEPTION(env);
            if(! textOb)
            {
                JS_ASSERT(0);
                continue;
            }
            itemOb = (*env)->NewObject(env, clazzSTI, midSTIctor,
                                       urlOb, textOb, status);
            ASSERT_CLEAR_EXCEPTION(env);
        }
        else if( JSD_IsSourceDirty(jsdjc->jsdc, item) &&
                 JSD_SOURCE_CLEARED != status )
        {
            /* if found and dirty then update */
            jstring textOb;
            const char* str;
            int len;

            if( ! JSD_GetSourceText(jsdjc->jsdc, item, &str, &len ) )
            {
                str = "";
                len = 0;
            }

            /*
            * XXX see "this is pretty lame..." above
            */

            textOb = _NewStringUTF_No_Z(env, str, len);
            ASSERT_CLEAR_EXCEPTION(env);
            if(! textOb)
            {
                JS_ASSERT(0);
                continue;
            }

            (*env)->CallVoidMethod(env, itemOb, midSTIsetText, textOb);
            ASSERT_CLEAR_EXCEPTION(env);
            (*env)->CallVoidMethod(env, itemOb, midSTIsetStatus, status);
            ASSERT_CLEAR_EXCEPTION(env);
            (*env)->CallVoidMethod(env, itemOb, midSTIsetDirty, JNI_TRUE);
            ASSERT_CLEAR_EXCEPTION(env);
        }

        /* we have our copy; clear the native cached text */
        if( JSD_SOURCE_INITED  != status &&
            JSD_SOURCE_PARTIAL != status &&
            JSD_SOURCE_CLEARED != status )
        {
            JSD_ClearSourceText(jsdjc->jsdc, item);
        }

        /* set the item clean */
        JSD_SetSourceDirty(jsdjc->jsdc, item, 0 );

        /* add the item to the vector */
        if( itemOb )
        {
            (*env)->CallVoidMethod(env, vec, midVectorAddElement, itemOb);
            ASSERT_CLEAR_EXCEPTION(env);
        }
    }
    /* unlock the native subsystem */
    JSD_UnlockSourceTextSubsystem(jsdjc->jsdc);

    /* set main vector to our new vector */
    fid = (*env)->GetFieldID(env, clazzSelf, "_sourceTextVector",
                             "Lnetscape/util/Vector;");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VOID(fid);
    (*env)->SetObjectField(env, self, fid, vec);
    ASSERT_CLEAR_EXCEPTION(env);
}

/***************************************************************************/
/*
 * Class:     netscape_jsdebug_JSStackFrameInfo
 * Method:    getCaller0
 * Signature: ()Lnetscape/jsdebug/StackFrameInfo;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSStackFrameInfo_getCaller0
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    jclass clazz;
    jfieldID fid;
    JSDStackFrameInfo* jsdframeCur;
    JSDStackFrameInfo* jsdframeCaller;
    JSDThreadState* jsdthreadstate;
    jobject threadState;
    jobject retval;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, NULL);
    ASSERT_CLEAR_EXCEPTION(env);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, NULL);

    /* get jsdframeCur */
    fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);
    jsdframeCur = (JSDStackFrameInfo*) (*env)->GetIntField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdframeCur, NULL);

    /* get threadState */
    fid = (*env)->GetFieldID(env, clazz, "threadState",
                             "Lnetscape/jsdebug/ThreadStateBase;");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);
    threadState = (*env)->GetObjectField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(threadState, NULL);

    /* get jsdthreadState from threadState */
    clazz = (*env)->GetObjectClass(env, threadState);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, NULL);
    fid = (*env)->GetFieldID(env, clazz, "nativeThreadState", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);
    jsdthreadstate = (JSDThreadState*) (*env)->GetIntField(env, threadState, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdthreadstate, NULL);

    /* get caller frame from JSD */
    jsdframeCaller = JSD_GetCallingStackFrame(jsdjc->jsdc, jsdthreadstate, jsdframeCur);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdframeCaller, NULL);

    retval = _constructJSStackFrameInfo(env, jsdframeCaller, threadState);
    ASSERT_CLEAR_EXCEPTION(env);
    return retval;
}

static JSBool _getJSStackFrameInfo(JNIEnv *env, jobject self,
                                   JSDJContext** pjsdjc,
                                   JSDThreadState** pjsdthreadstate,
                                   JSDStackFrameInfo** pjsdframe)
{
    JSDJContext* jsdjc;
    JSDThreadState* jsdthreadstate;
    JSDStackFrameInfo* jsdframe;
    jclass clazz;
    jfieldID fid;
    jobject threadState;

    JS_ASSERT(pjsdjc && pjsdframe && pjsdthreadstate);
    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, JS_FALSE);
    ASSERT_CLEAR_EXCEPTION(env);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, JS_FALSE);

    /* get jsdframe */
    fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, JS_FALSE);
    jsdframe = (JSDStackFrameInfo*) (*env)->GetIntField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdframe, JS_FALSE);

    /* get threadState */
    fid = (*env)->GetFieldID(env, clazz, "threadState",
                             "Lnetscape/jsdebug/ThreadStateBase;");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, JS_FALSE);
    threadState = (*env)->GetObjectField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(threadState, JS_FALSE);

    /* get jsdthreadState from threadState */
    clazz = (*env)->GetObjectClass(env, threadState);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, JS_FALSE);
    fid = (*env)->GetFieldID(env, clazz, "nativeThreadState", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, JS_FALSE);
    jsdthreadstate = (JSDThreadState*) (*env)->GetIntField(env, threadState, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdthreadstate, JS_FALSE);

    *pjsdjc          = jsdjc;
    *pjsdthreadstate = jsdthreadstate;
    *pjsdframe       = jsdframe;
    return JS_TRUE;
}

/*
 * Class:     netscape_jsdebug_JSStackFrameInfo
 * Method:    getPC
 * Signature: ()Lnetscape/jsdebug/PC;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSStackFrameInfo_getPC
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    JSDStackFrameInfo* jsdframe;
    JSDThreadState* jsdthreadstate;
    JSDScript* jsdscript;
    jobject script;
    int pc;
    jobject retval;

    if( !_getJSStackFrameInfo(env, self, &jsdjc, &jsdthreadstate, &jsdframe))
        return NULL;

    /* get jsdscript from JSD */
    jsdscript = JSD_GetScriptForStackFrame(jsdjc->jsdc, jsdthreadstate, jsdframe);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdscript, NULL);

    script = _scriptObFromJSDScriptPtr(env, jsdjc, jsdscript);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(script, NULL);

    pc = JSD_GetPCForStackFrame(jsdjc->jsdc, jsdthreadstate, jsdframe);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(pc, NULL);

    retval = _constructJSPC(env, script, pc);
    ASSERT_CLEAR_EXCEPTION(env);
    return retval;
}

/*
 * Class:     netscape_jsdebug_JSStackFrameInfo
 * Method:    getCallObject
 * Signature: ()Lnetscape/jsdebug/Value;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSStackFrameInfo_getCallObject
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    JSDStackFrameInfo* jsdframe;
    JSDThreadState* jsdthreadstate;
    JSDValue* jsdval;

    if( !_getJSStackFrameInfo(env, self, &jsdjc, &jsdthreadstate, &jsdframe))
        return NULL;
    jsdval = JSD_GetCallObjectForStackFrame(jsdjc->jsdc, jsdthreadstate, jsdframe);
    if(!jsdval)
        return NULL;
    return _constructValue(env, jsdjc->jsdc, jsdval);
}

/*
 * Class:     netscape_jsdebug_JSStackFrameInfo
 * Method:    getScopeChain
 * Signature: ()Lnetscape/jsdebug/Value;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSStackFrameInfo_getScopeChain
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    JSDStackFrameInfo* jsdframe;
    JSDThreadState* jsdthreadstate;
    JSDValue* jsdval;

    if( !_getJSStackFrameInfo(env, self, &jsdjc, &jsdthreadstate, &jsdframe))
        return NULL;
    jsdval = JSD_GetScopeChainForStackFrame(jsdjc->jsdc, jsdthreadstate, jsdframe);
    if(!jsdval)
        return NULL;
    return _constructValue(env, jsdjc->jsdc, jsdval);
}

/*
 * Class:     netscape_jsdebug_JSStackFrameInfo
 * Method:    getThis
 * Signature: ()Lnetscape/jsdebug/Value;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSStackFrameInfo_getThis
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    JSDStackFrameInfo* jsdframe;
    JSDThreadState* jsdthreadstate;
    JSDValue* jsdval;

    if( !_getJSStackFrameInfo(env, self, &jsdjc, &jsdthreadstate, &jsdframe))
        return NULL;
    jsdval = JSD_GetThisForStackFrame(jsdjc->jsdc, jsdthreadstate, jsdframe);
    if(!jsdval)
        return NULL;
    return _constructValue(env, jsdjc->jsdc, jsdval);
}

/***************************************************************************/
/*
 * Class:     netscape_jsdebug_JSThreadState
 * Method:    countStackFrames
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_netscape_jsdebug_JSThreadState_countStackFrames
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    jclass clazz;
    jfieldID fid;
    JSDThreadState* jsdthreadstate;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, 0);
    ASSERT_CLEAR_EXCEPTION(env);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, 0);

    fid = (*env)->GetFieldID(env, clazz, "nativeThreadState", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, 0);
    jsdthreadstate = (JSDThreadState*) (*env)->GetIntField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdthreadstate, 0);

    return (long) JSD_GetCountOfStackFrames(jsdjc->jsdc, jsdthreadstate);
}

/*
 * Class:     netscape_jsdebug_JSThreadState
 * Method:    getCurrentFrame
 * Signature: ()Lnetscape/jsdebug/StackFrameInfo;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSThreadState_getCurrentFrame
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    jclass clazz;
    jfieldID fid;
    JSDThreadState* jsdthreadstate;
    JSDStackFrameInfo* jsdframe;
    jobject retval;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, NULL);
    ASSERT_CLEAR_EXCEPTION(env);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, NULL);

    fid = (*env)->GetFieldID(env, clazz, "nativeThreadState", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);
    jsdthreadstate = (JSDThreadState*) (*env)->GetIntField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdthreadstate, NULL);

    jsdframe = JSD_GetStackFrame(jsdjc->jsdc, jsdthreadstate);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdframe, NULL);

    retval = _constructJSStackFrameInfo(env, jsdframe, self);
    ASSERT_CLEAR_EXCEPTION(env);
    return retval;
}

/***************************************************************************/
/*
 * Class:     netscape_jsdebug_Script
 * Method:    getClosestPC
 * Signature: (I)Lnetscape/jsdebug/JSPC;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_Script_getClosestPC
  (JNIEnv *env, jobject self, jint lineno)
{
    JSDJContext* jsdjc;
    jclass clazz;
    jfieldID fid;
    JSDScript* jsdscript;
    uintN pc;
    jobject retval;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, NULL);
    ASSERT_CLEAR_EXCEPTION(env);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, NULL);

    fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);
    jsdscript = (JSDScript*) (*env)->GetIntField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdscript, NULL);

    pc = JSD_GetClosestPC(jsdjc->jsdc, jsdscript, lineno);
    if( -1 == pc )
        return NULL;

    retval = _constructJSPC(env, self, pc);
    ASSERT_CLEAR_EXCEPTION(env);
    return retval;
}

/***************************************************************************/
/***************************************************************************/

#undef  IS_VALUE_X
#define IS_VALUE_X(type,e,s)                                            \
JS_BEGIN_MACRO                                                          \
    JSDValue* jsdval;                                                   \
    JSDJContext* jsdjc;                                                 \
    ASSERT_RETURN_VALUE(jsdval = _getNativeJSDValue(e, s), JNI_FALSE);  \
    ASSERT_RETURN_VALUE(jsdjc = _getJSDJContext(e), JNI_FALSE);         \
    ASSERT_RETURN_VALUE(jsdjc->jsdc, JNI_FALSE);                        \
    return JSD_IsValue##type(jsdjc->jsdc, jsdval);                      \
JS_END_MACRO

#undef  GET_VALUE_X
#define GET_VALUE_X(type,e,s,r)                                         \
JS_BEGIN_MACRO                                                          \
    JSDValue* jsdval;                                                   \
    JSDJContext* jsdjc;                                                 \
    ASSERT_RETURN_VALUE(jsdval = _getNativeJSDValue(e, s), JNI_FALSE);  \
    ASSERT_RETURN_VALUE(jsdjc = _getJSDJContext(e), JNI_FALSE);         \
    ASSERT_RETURN_VALUE(jsdjc->jsdc, JNI_FALSE);                        \
    r = JSD_GetValue##type(jsdjc->jsdc, jsdval);                        \
JS_END_MACRO

#undef  GET_VALUE_THING
#define GET_VALUE_THING(thing,e,s)                                          \
JS_BEGIN_MACRO                                                              \
    JSDJContext* jsdjc;                                                     \
    JSDContext* jsdc;                                                       \
    JSDValue* jsdval;                                                       \
    JSDValue* result;                                                       \
    ASSERT_RETURN_VALUE(jsdjc = _getJSDJContext(e), NULL);                  \
    ASSERT_RETURN_VALUE(jsdc = jsdjc->jsdc, NULL);                          \
    CHECK_RETURN_VALUE(jsdval = _getNativeJSDValue(env, s), NULL);          \
    CHECK_RETURN_VALUE(JSD_IsValueObject(jsdc, jsdval), NULL);              \
    CHECK_RETURN_VALUE(result = JSD_GetValue##thing(jsdc, jsdval), NULL);   \
    return _constructValue(env, jsdc, result);                              \
JS_END_MACRO

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isObject
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isObject
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Object, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isNumber
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isNumber
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Number, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isInt
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isInt
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Int, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isDouble
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isDouble
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Double, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isString
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isString
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(String, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isBoolean
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isBoolean
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Boolean, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isNull
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isNull
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Null, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isVoid
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isVoid
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Void, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isPrimitive
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isPrimitive
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Primitive, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isFunction
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isFunction
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Function, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    isNative
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_isNative
  (JNIEnv *env, jobject self)
{
    IS_VALUE_X(Native, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    getBoolean
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value_getBoolean
  (JNIEnv *env, jobject self)
{
    JSBool retval;
    GET_VALUE_X(Boolean, env, self, retval);
    return retval;
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    getInt
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_netscape_jsdebug_Value_getInt
  (JNIEnv *env, jobject self)
{
    int32 retval;
    GET_VALUE_X(Int, env, self, retval);
    return retval;
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    getDouble
 * Signature: ()D
 */
JNIEXPORT jdouble JNICALL Java_netscape_jsdebug_Value_getDouble
  (JNIEnv *env, jobject self)
{
    jsdouble* retval;
    GET_VALUE_X(Double, env, self, retval);
    CHECK_RETURN_VALUE(retval, 0);
    return *retval;
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_netscape_jsdebug_Value_finalize
  (JNIEnv *env, jobject self)
{
    _destroyValue(env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _refresh
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_netscape_jsdebug_Value__1refresh
  (JNIEnv *env, jobject self)
{
    JSDValue* jsdval;
    JSDJContext* jsdjc;
    ASSERT_RETURN_VOID(jsdval = _getNativeJSDValue(env, self));
    ASSERT_RETURN_VOID(jsdjc = _getJSDJContext(env));
    ASSERT_RETURN_VOID(jsdjc->jsdc);
    JSD_RefreshValue(jsdjc->jsdc, jsdval);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _getString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_netscape_jsdebug_Value__1getString
  (JNIEnv *env, jobject self)
{
    jstring jstr;
    JSString* retval;
    GET_VALUE_X(String, env, self, retval);
    CHECK_RETURN_VALUE(retval, NULL);

    jstr = (*env)->NewString(env, JS_GetStringChars(retval),
                                  JS_GetStringLength(retval));
    ASSERT_CLEAR_EXCEPTION(env);
    return jstr;
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _getFunctionName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_netscape_jsdebug_Value__1getFunctionName
  (JNIEnv *env, jobject self)
{
    jstring jstr;
    const char* retval;
    GET_VALUE_X(FunctionName, env, self, retval);
    CHECK_RETURN_VALUE(retval, NULL);

    jstr = (*env)->NewStringUTF(env, retval);
    ASSERT_CLEAR_EXCEPTION(env);
    return jstr;
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _getClassName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_netscape_jsdebug_Value__1getClassName
  (JNIEnv *env, jobject self)
{
    jstring jstr;
    const char* retval;
    GET_VALUE_X(ClassName, env, self, retval);
    CHECK_RETURN_VALUE(retval, NULL);

    jstr = (*env)->NewStringUTF(env, retval);
    ASSERT_CLEAR_EXCEPTION(env);
    return jstr;
}

static jobject _constructProperty(JNIEnv *env, jclass clazz,
                                  JSDContext* jsdc, JSDProperty* jsdprop)
{
    jobject obj;
    jmethodID mid;
    jfieldID fid;
    JSString* jsstr;
    jstring str;
    JSDValue* jsdval;
    jobject objField;
    uintN flags;
    uintN slot;
    int32 i;
    JSBool valid;
    short internalFlags = 0;

    flags = JSD_GetPropertyFlags(jsdc, jsdprop);
    slot  = JSD_GetPropertyVarArgSlot(jsdc, jsdprop);

    mid = (*env)->GetMethodID(env, clazz, "<init>", "()V");
    ASSERT_RETURN_VALUE(mid, NULL);
    obj = (*env)->NewObject(env, clazz, mid);
    ASSERT_RETURN_VALUE(obj, NULL);

    /* set the value */
    fid = (*env)->GetFieldID(env, clazz, "_value", "Lnetscape/jsdebug/Value;");
    ASSERT_RETURN_VALUE(fid, NULL);

    ASSERT_RETURN_VALUE(jsdval = JSD_GetPropertyValue(jsdc, jsdprop), NULL);
    ASSERT_RETURN_VALUE(objField = _constructValue(env, jsdc, jsdval), NULL);
    (*env)->SetObjectField(env, obj, fid, objField);

    ASSERT_RETURN_VALUE(jsdval = JSD_GetPropertyName(jsdc, jsdprop), NULL);
    /* set the name */
    if(JSD_IsValueString(jsdc, jsdval))
    {
        internalFlags |= 0x01; /* Property.NAME_IS_STRING */
        jsstr = JSD_GetValueString(jsdc, jsdval);
        if(jsstr)
            str = (*env)->NewString(env,JS_GetStringChars(jsstr),
                                        JS_GetStringLength(jsstr));
        JSD_DropValue(jsdc, jsdval);
        ASSERT_RETURN_VALUE(jsstr && str, NULL);
        fid = (*env)->GetFieldID(env, clazz, "_nameString",
                                 "Ljava/lang/String;");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetObjectField(env, obj, fid, str);
    }
    else
    {
        if(valid = JSD_IsValueInt(jsdc, jsdval))
            i = JSD_GetValueInt(jsdc, jsdval);
        JSD_DropValue(jsdc, jsdval);
        ASSERT_RETURN_VALUE(valid, NULL);
        fid = (*env)->GetFieldID(env, clazz, "_nameInt", "I");
        ASSERT_RETURN_VALUE(fid, NULL);
        (*env)->SetIntField(env, obj, fid, i);
    }

    /* set the alias */
    if(flags & JSPD_ALIAS)
    {
        ASSERT_RETURN_VALUE(jsdval = JSD_GetPropertyAlias(jsdc, jsdprop), NULL);
        if(JSD_IsValueString(jsdc, jsdval))
        {
            internalFlags |= 0x02; /* Property.ALIAS_IS_STRING */
            jsstr = JSD_GetValueString(jsdc, jsdval);
            if(jsstr)
                str = (*env)->NewString(env,JS_GetStringChars(jsstr),
                                            JS_GetStringLength(jsstr));
            JSD_DropValue(jsdc, jsdval);
            ASSERT_RETURN_VALUE(jsstr && str, NULL);
            ASSERT_RETURN_VALUE(str, NULL);
            fid = (*env)->GetFieldID(env, clazz, "_aliasString",
                                     "Ljava/lang/String;");
            ASSERT_RETURN_VALUE(fid, NULL);
            (*env)->SetObjectField(env, obj, fid, str);
        }
        else
        {
            if(valid = JSD_IsValueInt(jsdc, jsdval))
                i = JSD_GetValueInt(jsdc, jsdval);
            JSD_DropValue(jsdc, jsdval);
            ASSERT_RETURN_VALUE(valid, NULL);
            fid = (*env)->GetFieldID(env, clazz, "_aliasInt", "I");
            ASSERT_RETURN_VALUE(fid, NULL);
            (*env)->SetIntField(env, obj, fid, i);
        }
    }

    /* set the flags */
    fid = (*env)->GetFieldID(env, clazz, "_flags", "I");
    ASSERT_RETURN_VALUE(fid, NULL);
    (*env)->SetIntField(env, obj, fid, flags);

    /* set the slot ; XXX only matters id arg or prop, right?*/
    fid = (*env)->GetFieldID(env, clazz, "_slot", "I");
    ASSERT_RETURN_VALUE(fid, NULL);
    (*env)->SetIntField(env, obj, fid, slot);

    /* set the internal flags */
    fid = (*env)->GetFieldID(env, clazz, "_internalFlags", "S");
    ASSERT_RETURN_VALUE(fid, NULL);
    (*env)->SetShortField(env, obj, fid, (jshort) internalFlags);


    ASSERT_CLEAR_EXCEPTION(env);
    return obj;
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _getProperties
 * Signature: ()[Lnetscape/jsdebug/Property;
 */
JNIEXPORT jobjectArray JNICALL Java_netscape_jsdebug_Value__1getProperties
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    JSDContext* jsdc;
    JSDValue* jsdval;
    jobjectArray propArray = NULL;
    jclass clazz;
    uintN i;
    uintN count;
    jobject prop ;
    JSDProperty* iterp = NULL;
    JSDProperty* jsdprop;

    ASSERT_RETURN_VALUE(jsdjc = _getJSDJContext(env), NULL);
    ASSERT_RETURN_VALUE(jsdc = jsdjc->jsdc, NULL);
    CHECK_RETURN_VALUE(jsdval = _getNativeJSDValue(env, self), NULL);
    CHECK_RETURN_VALUE(JSD_IsValueObject(jsdc, jsdval), NULL);
    CHECK_RETURN_VALUE(!JSD_IsValueNull(jsdc, jsdval), NULL);
    CHECK_RETURN_VALUE(count = JSD_GetCountOfProperties(jsdc, jsdval), NULL);

    clazz = (*env)->FindClass(env, "netscape/jsdebug/Property");
    ASSERT_RETURN_VALUE(clazz, NULL);

    propArray = (*env)->NewObjectArray(env, count, clazz, NULL);
    ASSERT_RETURN_VALUE(propArray, NULL);

    for(i = 0; i < count; i++)
    {
        jsdprop = JSD_IterateProperties(jsdc, jsdval, &iterp);
        if(!jsdprop)
        {
            JS_ASSERT(0);
            propArray = NULL;
            break;
        }
        prop = _constructProperty(env, clazz, jsdc, jsdprop);
        JSD_DropProperty(jsdc, jsdprop);
        if(!prop)
        {
            JS_ASSERT(0);
            propArray = NULL;
            break;
        }
        (*env)->SetObjectArrayElement(env, propArray, i, prop);
        ASSERT_CLEAR_EXCEPTION(env);
    }
    JS_ASSERT(i != count || !JSD_IterateProperties(jsdc, jsdval, &iterp));
    return propArray;
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _getPrototype
 * Signature: ()Lnetscape/jsdebug/Value;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_Value__1getPrototype
  (JNIEnv *env, jobject self)
{
    GET_VALUE_THING(Prototype, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _getParent
 * Signature: ()Lnetscape/jsdebug/Value;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_Value__1getParent
  (JNIEnv *env, jobject self)
{
    GET_VALUE_THING(Parent, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _getConstructor
 * Signature: ()Lnetscape/jsdebug/Value;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_Value__1getConstructor
  (JNIEnv *env, jobject self)
{
    GET_VALUE_THING(Constructor, env, self);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _equals
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_jsdebug_Value__1equals
  (JNIEnv *env, jobject self, jint val1, jint val2)
{
    JSDContext* jsdc;
    JSDJContext* jsdjc;
    JSDValue* jsdval1;
    JSDValue* jsdval2;
    ASSERT_RETURN_VALUE(jsdjc = _getJSDJContext(env), JNI_FALSE);
    ASSERT_RETURN_VALUE(jsdc = jsdjc->jsdc, JNI_FALSE);

    ASSERT_RETURN_VALUE(jsdval1 = (JSDValue*) val1, JNI_FALSE);
    ASSERT_RETURN_VALUE(jsdval2 = (JSDValue*) val2, JNI_FALSE);

    JS_ASSERT(jsdval1 == _getNativeJSDValue(env, self));

    return JSD_GetValueWrappedJSVal(jsdc, jsdval1) ==
           JSD_GetValueWrappedJSVal(jsdc, jsdval2);
}

/*
 * Class:     netscape_jsdebug_Value
 * Method:    _getProperty
 * Signature: (Ljava/lang/String;)Lnetscape/jsdebug/Property;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_Value__1getProperty
  (JNIEnv *env, jobject self, jstring jName)
{
    JSDContext* jsdc;
    JSDJContext* jsdjc;
    JSDValue* jsdval;
    JSString* jsstr;
    JSContext* cx;
    const jchar *jchars;
    jboolean isCopy;
    jsize nameLen;
    JSDProperty* jsdprop;
    jclass clazz;
    jobject prop;

    clazz = (*env)->FindClass(env, "netscape/jsdebug/Property");
    ASSERT_RETURN_VALUE(clazz, NULL);
    ASSERT_RETURN_VALUE(jsdjc = _getJSDJContext(env), NULL);
    ASSERT_RETURN_VALUE(jsdc = jsdjc->jsdc, NULL);
    CHECK_RETURN_VALUE(jsdval = _getNativeJSDValue(env, self), NULL);
    CHECK_RETURN_VALUE(JSD_IsValueObject(jsdc, jsdval), NULL);
    ASSERT_RETURN_VALUE(cx = _getDefaultJSContext(env), NULL);
    jchars = (*env)->GetStringChars(env, jName, &isCopy);
    ASSERT_RETURN_VALUE(jchars, NULL);
    nameLen = (*env)->GetStringLength(env, jName);
    jsstr = JS_NewUCStringCopyN(cx, jchars, nameLen);
    (*env)->ReleaseStringChars(env, jName, jchars);

    jsdprop = JSD_GetValueProperty(jsdc, jsdval, jsstr);
    if(!jsdprop)
        return NULL;

    prop = _constructProperty(env, clazz, jsdc, jsdprop);
    JSD_DropProperty(jsdc, jsdprop);

    return prop;
}

/***************************************************************************/
/***************************************************************************/
/* Registration code... */

static JNINativeMethod _natives_DebugController[] =
{
  {"setInstructionHook0",
   "(Lnetscape/jsdebug/PC;)V",
   Java_netscape_jsdebug_DebugController_setInstructionHook0},
  {"sendInterrupt0",
   "()V",
   Java_netscape_jsdebug_DebugController_sendInterrupt0},
  {"_setController",
   "(Z)V",
   Java_netscape_jsdebug_DebugController__1setController},
  {"executeScriptInStackFrame0",
   "(Lnetscape/jsdebug/JSStackFrameInfo;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;",
   Java_netscape_jsdebug_DebugController_executeScriptInStackFrame0},
  {"executeScriptInStackFrameValue0",
   "(Lnetscape/jsdebug/JSStackFrameInfo;Ljava/lang/String;Ljava/lang/String;I)Lnetscape/jsdebug/Value;",
   Java_netscape_jsdebug_DebugController_executeScriptInStackFrameValue0},
  {"getNativeMajorVersion",
   "()I",
   Java_netscape_jsdebug_DebugController_getNativeMajorVersion},
  {"getNativeMinorVersion",
   "()I",
   Java_netscape_jsdebug_DebugController_getNativeMinorVersion}
};

static JNINativeMethod _natives_JSPC[] =
{
  {"getSourceLocation",
   "()Lnetscape/jsdebug/SourceLocation;",
   Java_netscape_jsdebug_JSPC_getSourceLocation}
};

static JNINativeMethod _natives_JSSourceTextProvider[] =
{
  {"loadSourceTextItem0",
   "(Ljava/lang/String;)Lnetscape/jsdebug/SourceTextItem;",
   Java_netscape_jsdebug_JSSourceTextProvider_loadSourceTextItem0},
  {"refreshSourceTextVector",
   "()V",
   Java_netscape_jsdebug_JSSourceTextProvider_refreshSourceTextVector}
};

static JNINativeMethod _natives_JSStackFrameInfo[] =
{
  {"getCaller0",
   "()Lnetscape/jsdebug/StackFrameInfo;",
   Java_netscape_jsdebug_JSStackFrameInfo_getCaller0},
  {"getPC",
   "()Lnetscape/jsdebug/PC;",
   Java_netscape_jsdebug_JSStackFrameInfo_getPC},
  {"getCallObject",
   "()Lnetscape/jsdebug/Value;",
   Java_netscape_jsdebug_JSStackFrameInfo_getCallObject},
  {"getScopeChain",
   "()Lnetscape/jsdebug/Value;",
   Java_netscape_jsdebug_JSStackFrameInfo_getScopeChain},
  {"getThis",
   "()Lnetscape/jsdebug/Value;",
   Java_netscape_jsdebug_JSStackFrameInfo_getThis}
};

static JNINativeMethod _natives_JSThreadState[] =
{
  {"countStackFrames",
   "()I",
   Java_netscape_jsdebug_JSThreadState_countStackFrames},
  {"getCurrentFrame",
   "()Lnetscape/jsdebug/StackFrameInfo;",
   Java_netscape_jsdebug_JSThreadState_getCurrentFrame}
};

static JNINativeMethod _natives_Script[] =
{
  {"getClosestPC",
   "(I)Lnetscape/jsdebug/JSPC;",
   Java_netscape_jsdebug_Script_getClosestPC}
};

static JNINativeMethod _natives_Value[] =
{
  {"isObject",
   "()Z",
   Java_netscape_jsdebug_Value_isObject},
  {"isNumber",
   "()Z",
   Java_netscape_jsdebug_Value_isNumber},
  {"isInt",
   "()Z",
   Java_netscape_jsdebug_Value_isInt},
  {"isDouble",
   "()Z",
   Java_netscape_jsdebug_Value_isDouble},
  {"isString",
   "()Z",
   Java_netscape_jsdebug_Value_isString},
  {"isBoolean",
   "()Z",
   Java_netscape_jsdebug_Value_isBoolean},
  {"isNull",
   "()Z",
   Java_netscape_jsdebug_Value_isNull},
  {"isVoid",
   "()Z",
   Java_netscape_jsdebug_Value_isVoid},
  {"isPrimitive",
   "()Z",
   Java_netscape_jsdebug_Value_isPrimitive},
  {"isFunction",
   "()Z",
   Java_netscape_jsdebug_Value_isFunction},
  {"isNative",
   "()Z",
   Java_netscape_jsdebug_Value_isNative},
  {"getBoolean",
   "()Z",
   Java_netscape_jsdebug_Value_getBoolean},
  {"getInt",
   "()I",
   Java_netscape_jsdebug_Value_getInt},
  {"getDouble",
   "()D",
   Java_netscape_jsdebug_Value_getDouble},
  {"finalize",
   "()V",
   Java_netscape_jsdebug_Value_finalize},
  {"_refresh",
   "()V",
   Java_netscape_jsdebug_Value__1refresh},
  {"_getString",
   "()Ljava/lang/String;",
   Java_netscape_jsdebug_Value__1getString},
  {"_getFunctionName",
   "()Ljava/lang/String;",
   Java_netscape_jsdebug_Value__1getFunctionName},
  {"_getClassName",
   "()Ljava/lang/String;",
   Java_netscape_jsdebug_Value__1getClassName},
  {"_getProperties",
   "()[Lnetscape/jsdebug/Property;",
   Java_netscape_jsdebug_Value__1getProperties},
  {"_getPrototype",
   "()Lnetscape/jsdebug/Value;",
   Java_netscape_jsdebug_Value__1getPrototype},
  {"_getParent",
   "()Lnetscape/jsdebug/Value;",
   Java_netscape_jsdebug_Value__1getParent},
  {"_getConstructor",
   "()Lnetscape/jsdebug/Value;",
   Java_netscape_jsdebug_Value__1getConstructor},
  {"_equals",
   "(II)Z",
   Java_netscape_jsdebug_Value__1equals},
  {"_getProperty",
   "(Ljava/lang/String;)Lnetscape/jsdebug/Property;",
   Java_netscape_jsdebug_Value__1getProperty},
};

/*************************************************/

typedef struct {
    char* classname;
    JNINativeMethod* natives;
    jint count;
} ClassyJNINativeMethod;

#undef ARRAY_COUNT
#define ARRAY_COUNT(a) (sizeof(a)/sizeof(a[0]))

#undef ITEM
#define ITEM(t) {"netscape/jsdebug/"#t,_natives_##t,ARRAY_COUNT(_natives_##t)}

static ClassyJNINativeMethod _natives[] =
{
  ITEM(DebugController),
  ITEM(JSPC),
  ITEM(JSSourceTextProvider),
  ITEM(JSStackFrameInfo),
  ITEM(JSThreadState),
  ITEM(Script),
  ITEM(Value)
};

#define NATIVE_COUNT (ARRAY_COUNT(_natives))

/*************************************************/

JSBool
jsdj_RegisterNatives(JSDJContext* jsdjc)
{
    JSBool ok = JS_TRUE;
    JNIEnv *env;
    int i;

    ASSERT_RETURN_VALUE(jsdjc, JS_FALSE);
    ASSERT_RETURN_VALUE(env = jsdj_GetJNIEnvForCurrentThread(jsdjc), JS_FALSE);
    ASSERT_CLEAR_EXCEPTION(env);

    for( i = 0; i < NATIVE_COUNT; i++ )
    {
        jclass clazz;
        if( 0 == (clazz = (*env)->FindClass(env, _natives[i].classname)) )
        {
            fprintf(stderr, "failed to find class %s while registering natives\n",
                    _natives[i].classname );
            JS_ASSERT(0);
            CHECK_CLEAR_EXCEPTION(env);
            return JS_FALSE;
        }
        if( 0 != (*env)->RegisterNatives(env, clazz, _natives[i].natives,
                                         _natives[i].count) )
        {
            fprintf(stderr, "failed to register natives for %s\n",
                    _natives[i].classname );
            JS_ASSERT(0);
            CHECK_CLEAR_EXCEPTION(env);
            return JS_FALSE;
        }
    }
    ASSERT_CLEAR_EXCEPTION(env);

    return JS_TRUE;
}
