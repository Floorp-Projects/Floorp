
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "_jni/com_netscape_nativejsengine_JSRuntime.h"
#include "_jni/com_netscape_nativejsengine_JSContext.h"

#include "jsapi.h"
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */

#ifdef JSDEBUGGER
#include "jsdebug.h"
#include "jsdjava.h"
#endif

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

typedef struct ContextInfo {
    JNIEnv* env;
    jobject contextObject;
} ContextInfo;

/***************************************************************************/

#ifdef JSDEBUGGER
static void
_jamSourceIntoJSD(JSContext *cx, const char* src, int len, const char* filename)
{
    jclass clazz_self;
    jclass clazz;
    JSDJContext* jsdjc;
    jobject rtObject;
    jobject contextObject;
    jmethodID mid;
    jfieldID fid;
    ContextInfo* info;
    JNIEnv* env;

    info = (ContextInfo*) JS_GetContextPrivate(cx);
    ASSERT_RETURN_VOID(info);

    env = info->env;
    ASSERT_RETURN_VOID(env);

    contextObject = info->contextObject;
    ASSERT_RETURN_VOID(contextObject);

    clazz_self = (*env)->GetObjectClass(env, contextObject);
    ASSERT_RETURN_VOID(clazz_self);

    fid = (*env)->GetFieldID(env, clazz_self, "_runtime", 
                             "Lcom/netscape/nativejsengine/JSRuntime;");
    ASSERT_RETURN_VOID(fid);

    rtObject = (*env)->GetObjectField(env, contextObject, fid);
    ASSERT_RETURN_VOID(rtObject);

    clazz = (*env)->GetObjectClass(env, rtObject);
    ASSERT_RETURN_VOID(clazz);

    mid  = (*env)->GetMethodID(env, clazz, "getNativeDebugSupport", "()J");
    ASSERT_RETURN_VOID(mid);

    jsdjc = (JSDJContext*) (*env)->CallObjectMethod(env, rtObject, mid);
    if(jsdjc)
    {
        JSDContext* jsdc;

        jsdc = JSDJ_GetJSDContext(jsdjc);
        ASSERT_RETURN_VOID(jsdc);

        JSD_AddFullSourceText(jsdc, src, len, filename);
    }
}
#endif   

static JSBool
_loadSingleFile(JSContext *cx, JSObject *obj, const char* filename)
{
    char* buf;
    FILE* file;
    int file_len;
    jsval result;

    errno = 0;
    file = fopen(filename, "rb");
    if (!file) {
        JS_ReportError(cx, "can't open %s: %s", filename, strerror(errno));
        return JS_FALSE;
    }

    fseek(file, 0, SEEK_END);
    file_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    if(! file_len) {
        fclose(file);
        JS_ReportError(cx, "%s is empty", filename);
        return JS_FALSE;
    }

    buf = (char*) malloc(file_len);
    if(! buf) {
        fclose(file);
        JS_ReportError(cx, "memory alloc error while trying to read %s", filename);
        return JS_FALSE;
    }
    fread(buf, 1, file_len, file);
    fclose(file);

#ifdef JSDEBUGGER
    _jamSourceIntoJSD(cx, buf, file_len, filename);
#endif   

    JS_EvaluateScript(cx, obj, buf, file_len, filename, 1, &result);

    free(buf);
    return JS_TRUE;
}    
                

static void _sendPrintStringToJava(JNIEnv* env, jobject contextObject,
                                   jmethodID mid, const char* str)
{
    if(! str)
        return;
    (*env)->CallObjectMethod(env, contextObject, mid, 
                             (*env)->NewStringUTF(env, str));
}

static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i, n;
    JSString *str;

    ContextInfo* info;
    jmethodID mid;
    jclass clazz;
    JNIEnv* env;

    info = (ContextInfo*) JS_GetContextPrivate(cx);
    ASSERT_RETURN_VALUE(info, JS_FALSE);

    env = info->env;
    ASSERT_RETURN_VALUE(env, JS_FALSE);

    clazz = (*env)->GetObjectClass(env, info->contextObject);
    ASSERT_RETURN_VALUE(clazz, JS_FALSE);

    mid  = (*env)->GetMethodID(env, clazz, "_print", "(Ljava/lang/String;)V");
    ASSERT_RETURN_VALUE(mid, JS_FALSE);

    for (i = n = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;

        if(i)
            _sendPrintStringToJava(env, info->contextObject, mid, "");
        _sendPrintStringToJava(env, info->contextObject, mid, JS_GetStringBytes(str));
        n++;
    }
    if (n)
        _sendPrintStringToJava(env, info->contextObject, mid, "\n");
    return JS_TRUE;
}

static JSBool
Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc > 0 && JSVAL_IS_INT(argv[0]))
        *rval = INT_TO_JSVAL(JS_SetVersion(cx, JSVAL_TO_INT(argv[0])));
    else
        *rval = INT_TO_JSVAL(JS_GetVersion(cx));
    return JS_TRUE;
}

static JSBool
Load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSString *str;
    const char *filename;

    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        argv[i] = STRING_TO_JSVAL(str);
        filename = JS_GetStringBytes(str);

        if(! _loadSingleFile(cx, obj, filename))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    {"version",         Version,        0},
    {"load",            Load,           1},
    {"print",           Print,          0},
    {0}
};

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    ContextInfo* info;
    jmethodID mid;
    jclass clazz;
    JNIEnv* env;

    jobject msg      = NULL;
    jobject filename = NULL;
    jobject lineBuf  = NULL;
    int lineno = 0;
    int offset = 0;

    info = (ContextInfo*) JS_GetContextPrivate(cx);
    ASSERT_RETURN_VOID(info);

    env = info->env;
    ASSERT_RETURN_VOID(env);

    clazz = (*env)->GetObjectClass(env, info->contextObject);
    ASSERT_RETURN_VOID(clazz);

    mid  = (*env)->GetMethodID(env, clazz, "_reportError", 
            "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;I)V");
    ASSERT_RETURN_VOID(mid);


    if(message)
        msg = (*env)->NewStringUTF(env, message);

    if(report)
    {
        lineno = report->lineno;
        if(report->filename)
            filename = (*env)->NewStringUTF(env, report->filename);

        if(report->linebuf)
        {
            lineBuf = (*env)->NewStringUTF(env, report->linebuf);
            if(report->tokenptr)
                offset = report->tokenptr - report->linebuf;
        }
    }

    (*env)->CallObjectMethod(env, info->contextObject, mid, 
                             msg, filename, lineno, lineBuf, offset);

}

static JSClass global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

/*
 * Class:     com_netscape_nativejsengine_JSRuntime
 * Method:    _init
 * Signature: (Z)Z
 */
JNIEXPORT jboolean JNICALL Java_com_netscape_nativejsengine_JSRuntime__1init
  (JNIEnv * env, jobject self, jboolean enableDebugging)
{
    JSRuntime *rt;
    jclass clazz;
    jfieldID fid;

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    ASSERT_RETURN_VALUE(rt, JNI_FALSE);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_RETURN_VALUE(clazz, JNI_FALSE);

    fid = (*env)->GetFieldID(env, clazz, "_nativeRuntime", "J");
    ASSERT_RETURN_VALUE(fid, JNI_FALSE);
    (*env)->SetLongField(env, self, fid, (long) rt);


#ifdef JSDEBUGGER
    if(enableDebugging)
    {
        JSDJContext* jsdjc;
        JSDContext* jsdc;

        jsdc = JSD_DebuggerOnForUser(rt, NULL, NULL);
        ASSERT_RETURN_VALUE(jsdc, JNI_FALSE);

        jsdjc = JSDJ_CreateContext();
        ASSERT_RETURN_VALUE(jsdjc, JNI_FALSE);

        JSDJ_SetJSDContext(jsdjc, jsdc);
        JSDJ_SetJNIEnvForCurrentThread(jsdjc, env);

        fid = (*env)->GetFieldID(env, clazz, "_nativeDebugSupport", "J");
        ASSERT_RETURN_VALUE(fid, JNI_FALSE);
        (*env)->SetLongField(env, self, fid, (long) jsdjc);
    }
#else
    if(enableDebugging)
        printf("ERROR - Context created with enableDebugging flag, but no debugging support compiled in!");
#endif

    return JNI_TRUE;
}

/*
 * Class:     com_netscape_nativejsengine_JSRuntime
 * Method:    _exit
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_netscape_nativejsengine_JSRuntime__1exit
  (JNIEnv * env, jobject self)
{
    jfieldID fid;
    jclass clazz;
    JSRuntime *rt;
    JSContext *iterp = NULL;

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_RETURN_VOID(clazz);

    fid = (*env)->GetFieldID(env, clazz, "_nativeRuntime", "J");
    ASSERT_RETURN_VOID(fid);
    rt = (JSRuntime *) (*env)->GetLongField(env, self, fid);
    ASSERT_RETURN_VOID(rt);


    /* 
    * Can't kill runtime if it holds any contexts
    *  
    * However, JSD may make it's own context(s), so don't ASSERT 
    */ 
    CHECK_RETURN_VOID(!JS_ContextIterator(rt, &iterp));

    printf("runtime = %d\n", (int)rt);

    JS_DestroyRuntime(rt);
}

/***************************************************************************/

/*
 * Class:     com_netscape_nativejsengine_JSContext
 * Method:    _init
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_netscape_nativejsengine_JSContext__1init
  (JNIEnv *env, jobject self)
{
    JSContext *cx;
    JSObject *glob;
    jfieldID fid;
    jmethodID mid;
    JSRuntime *rt;
    jobject rtObject;
    jclass clazz;
    jclass clazz_self;
    JSBool ok;
    ContextInfo* info;

#ifdef JSDEBUGGER
    JSDJContext* jsdjc;
#endif

    clazz_self = (*env)->GetObjectClass(env, self);
    ASSERT_RETURN_VALUE(clazz_self, JNI_FALSE);

    fid = (*env)->GetFieldID(env, clazz_self, "_runtime", 
                             "Lcom/netscape/nativejsengine/JSRuntime;");
    ASSERT_RETURN_VALUE(fid, JNI_FALSE);

    rtObject = (*env)->GetObjectField(env, self, fid);
    ASSERT_RETURN_VALUE(rtObject, JNI_FALSE);

    clazz = (*env)->GetObjectClass(env, rtObject);
    ASSERT_RETURN_VALUE(clazz, JNI_FALSE);

    mid  = (*env)->GetMethodID(env, clazz, "getNativeRuntime", "()J");
    ASSERT_RETURN_VALUE(mid, JNI_FALSE);
    
    rt = (JSRuntime *) (*env)->CallObjectMethod(env, rtObject, mid);
    ASSERT_RETURN_VALUE(rt, JNI_FALSE);

    cx = JS_NewContext(rt, 8192);
    ASSERT_RETURN_VALUE(cx, JNI_FALSE);

    JS_SetErrorReporter(cx, my_ErrorReporter);

    glob = JS_NewObject(cx, &global_class, NULL, NULL);
    ASSERT_RETURN_VALUE(glob, JNI_FALSE);

    ok = JS_InitStandardClasses(cx, glob);
    ASSERT_RETURN_VALUE(ok, JNI_FALSE);

    ok = JS_DefineFunctions(cx, glob, shell_functions);
    ASSERT_RETURN_VALUE(ok, JNI_FALSE);

    fid = (*env)->GetFieldID(env, clazz_self, "_nativeContext", "J");
    ASSERT_RETURN_VALUE(fid, JNI_FALSE);
    (*env)->SetLongField(env, self, fid, (long) cx);


    info = (ContextInfo*) malloc(sizeof(ContextInfo));
    ASSERT_RETURN_VALUE(info, JNI_FALSE);
    
    info->env = env;
    info->contextObject = self;

    JS_SetContextPrivate(cx, info);

#ifdef JSDEBUGGER
    mid  = (*env)->GetMethodID(env, clazz, "getNativeDebugSupport", "()J");
    ASSERT_RETURN_VALUE(mid, JNI_FALSE);

    jsdjc = (JSDJContext*) (*env)->CallObjectMethod(env, rtObject, mid);
    if(jsdjc)
    {
        JSDContext* jsdc = JSDJ_GetJSDContext(jsdjc);
        ASSERT_RETURN_VALUE(jsdc, JNI_FALSE);

        JSDJ_SetJNIEnvForCurrentThread(jsdjc, env);
        JSD_JSContextInUse(jsdc, cx);
    }
#endif

    return JNI_TRUE;
}

/*
 * Class:     com_netscape_nativejsengine_JSContext
 * Method:    _exit
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_netscape_nativejsengine_JSContext__1exit
  (JNIEnv *env, jobject self)
{
    jfieldID fid;
    jclass clazz;
    JSContext *cx;
    ContextInfo* info;

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_RETURN_VOID(clazz);

    fid = (*env)->GetFieldID(env, clazz, "_nativeContext", "J");
    ASSERT_RETURN_VOID(fid);

    cx = (JSContext *) (*env)->GetLongField(env, self, fid);
    ASSERT_RETURN_VOID(cx);

    info = (ContextInfo*) JS_GetContextPrivate(cx);
    ASSERT_RETURN_VOID(info);
    free(info);

    printf("context = %d\n", (int)cx);

    JS_DestroyContext(cx);
}

/*
 * Class:     com_netscape_nativejsengine_JSContext
 * Method:    _eval
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_netscape_nativejsengine_JSContext__1eval
  (JNIEnv * env, jobject self, jstring str, jstring filename, jint lineno)
{
    jfieldID fid;
    jclass clazz_self;
    JSContext *cx;
    JSObject *glob;
    jsval rval;
    int len;
    const char* Cstr;
    const char* Cfilename;
    jboolean isCopy;

    clazz_self = (*env)->GetObjectClass(env, self);
    ASSERT_RETURN_VOID(clazz_self);

    fid = (*env)->GetFieldID(env, clazz_self, "_nativeContext", "J");
    ASSERT_RETURN_VOID(fid);

    cx = (JSContext *) (*env)->GetLongField(env, self, fid);
    ASSERT_RETURN_VOID(cx);

    glob = JS_GetGlobalObject(cx);
    ASSERT_RETURN_VOID(glob);

    len = (*env)->GetStringUTFLength(env, str);
    Cstr = (*env)->GetStringUTFChars(env, str, &isCopy); 
    Cfilename = (*env)->GetStringUTFChars(env, filename, &isCopy);

#ifdef JSDEBUGGER
    /*                                                           
    * XXX this just overwrites any previous source for this url! 
    */                                                           
    _jamSourceIntoJSD(cx, Cstr, len, Cfilename);
#endif   

    JS_EvaluateScript(cx, glob, Cstr, len, Cfilename, lineno, &rval);

    (*env)->ReleaseStringUTFChars(env, str, Cstr);
    (*env)->ReleaseStringUTFChars(env, filename, Cfilename);
}

/*
 * Class:     com_netscape_nativejsengine_JSContext
 * Method:    _load
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_netscape_nativejsengine_JSContext__1load
  (JNIEnv *env, jobject self, jstring filename)
{
    jfieldID fid;
    jclass clazz;
    JSContext *cx;
    const char* Cfilename;
    jboolean isCopy;
    JSObject *glob;

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_RETURN_VOID(clazz);

    fid = (*env)->GetFieldID(env, clazz, "_nativeContext", "J");
    ASSERT_RETURN_VOID(fid);

    cx = (JSContext *) (*env)->GetLongField(env, self, fid);
    ASSERT_RETURN_VOID(cx);
    
    glob = JS_GetGlobalObject(cx);
    ASSERT_RETURN_VOID(glob);

    Cfilename = (*env)->GetStringUTFChars(env, filename, &isCopy);

    _loadSingleFile(cx, glob, Cfilename);

    (*env)->ReleaseStringUTFChars(env, filename, Cfilename);
}
