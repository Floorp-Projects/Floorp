
#include "jsdj.h"

/***************************************************************************/

#define ASSERT_RETURN_VOID(x)   \
PR_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        PR_ASSERT(0);           \
        return;                 \
    }                           \
PR_END_MACRO

#define ASSERT_RETURN_VALUE(x,v)\
PR_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        PR_ASSERT(0);           \
        return v;               \
    }                           \
PR_END_MACRO

#define CHECK_RETURN_VOID(x)    \
PR_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        return;                 \
    }                           \
PR_END_MACRO

#define CHECK_RETURN_VALUE(x,v) \
PR_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        return v;               \
    }                           \
PR_END_MACRO

#define ASSERT_GOTO(x,w)        \
PR_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        PR_ASSERT(0);           \
        goto w;                 \
    }                           \
PR_END_MACRO

#define CHECK_GOTO(x,w)         \
PR_BEGIN_MACRO                  \
    if(!(x))                    \
    {                           \
        goto w;                 \
    }                           \
PR_END_MACRO

#ifdef DEBUG
#define ASSERT_CLEAR_EXCEPTION(e)   \
PR_BEGIN_MACRO                      \
    if((*e)->ExceptionOccurred(e))  \
    {                               \
        (*e)->ExceptionDescribe(e); \
        PR_ASSERT(0);               \
    }                               \
    (*e)->ExceptionClear(e);        \
PR_END_MACRO
#else  /* ! DEBUG */
#define ASSERT_CLEAR_EXCEPTION(e) (*e)->ExceptionClear(e)
#endif /* DEBUG */

#define CHECK_CLEAR_EXCEPTION(e) (*e)->ExceptionClear(e)


/***************************************************************************/

#ifdef DEBUG
void JSDJ_ASSERT_VALID_CONTEXT(JSDJContext* jsdjc)
{
    PR_ASSERT(jsdjc);
}    
#endif /* DEBUG */

static JSDJContext* _HACK_CONTEXT = NULL;

JSDJContext*
jsdj_CreateContext()
{
    JSDJContext* jsdjc;
    jsdjc = (JSDJContext*) calloc(1, sizeof(JSDJContext));
    _HACK_CONTEXT = jsdjc;
    return jsdjc;
}    

void
jsdj_DestroyContext(JSDJContext* jsdjc)
{
    free(jsdjc);
    _HACK_CONTEXT = NULL;
}    

void
jsdj_SetJNIEnvForCurrentThread(JSDJContext* jsdjc, JNIEnv* env)
{
    jsdjc->env = env;
}    

JNIEnv*
jsdj_GetJNIEnvForCurrentThread(JSDJContext* jsdjc)
{
    return jsdjc->env;
}    

void
jsdj_SetJSDContext(JSDJContext* jsdjc, JSDContext* jsdc)
{
    jsdjc->jsdc = jsdc;
}    

JSDContext*
jsdj_GetJSDContext(JSDJContext* jsdjc)
{
    return jsdjc->jsdc;
}

void
jsdj_SetStartStopCallback(JSDJContext* jsdjc, JSDJStartStopCallback cb, 
                          void* closure)
{
    jsdjc->startStopCallback     = cb;
    jsdjc->startStopCallbackData = closure;
}

static JSDJContext*
_getJSDJContext(JNIEnv *env)
{
    return _HACK_CONTEXT;
}    

/***************************************************************************/
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

void PR_CALLBACK
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
uintN PR_CALLBACK
_executionHook(JSDContext*     jsdc, 
               JSDThreadState* jsdstate,
               uintN           type,
               void*           callerdata)
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

uintN PR_CALLBACK
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
    ASSERT_GOTO(jsdjc && jsdjc->jsdc, LABEL_FAILURE);
    ASSERT_CLEAR_EXCEPTION(env);

    if(on)
    {
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

    if(jsdjc->startStopCallback)
        (*jsdjc->startStopCallback)(jsdjc, on ? JSDJ_START_SUCCESS : JSDJ_STOP, 
                                    jsdjc->startStopCallbackData);
    return;

LABEL_FAILURE:
    if(jsdjc->startStopCallback)
        (*jsdjc->startStopCallback)(jsdjc, on ? JSDJ_START_FAILURE : JSDJ_STOP,
                                    jsdjc->startStopCallbackData);
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
                PR_ASSERT(0);
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
                PR_ASSERT(0);
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

/*
 * Class:     netscape_jsdebug_JSStackFrameInfo
 * Method:    getPC
 * Signature: ()Lnetscape/jsdebug/PC;
 */
JNIEXPORT jobject JNICALL Java_netscape_jsdebug_JSStackFrameInfo_getPC
  (JNIEnv *env, jobject self)
{
    JSDJContext* jsdjc;
    jclass clazz;
    jfieldID fid;
    JSDStackFrameInfo* jsdframe;
    JSDThreadState* jsdthreadstate;
    JSDScript* jsdscript;
    jobject threadState;
    jobject script;
    int pc;
    jobject retval;

    jsdjc = _getJSDJContext(env);
    ASSERT_RETURN_VALUE(jsdjc && jsdjc->jsdc && jsdjc->controller, NULL);
    ASSERT_CLEAR_EXCEPTION(env);

    clazz = (*env)->GetObjectClass(env, self);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(clazz, NULL);

    /* get jsdframe */
    fid = (*env)->GetFieldID(env, clazz, "_nativePtr", "I");
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(fid, NULL);
    jsdframe = (JSDStackFrameInfo*) (*env)->GetIntField(env, self, fid);
    ASSERT_CLEAR_EXCEPTION(env);
    ASSERT_RETURN_VALUE(jsdframe, NULL);

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


    /* get jsdscript from JSD */
    jsdscript = JSD_GetScriptForStackFrame(jsdjc->jsdc, jsdthreadstate, jsdframe );
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
/* Registration code... */

typedef struct {
    char* classname;
    JNINativeMethod native;
} ClassyJNINativeMethod;

static ClassyJNINativeMethod _natives[] =
{
  {"netscape/jsdebug/DebugController",
     {"setInstructionHook0",
      "(Lnetscape/jsdebug/PC;)V",
      Java_netscape_jsdebug_DebugController_setInstructionHook0
     }
  },
  {"netscape/jsdebug/DebugController",
     {"sendInterrupt0",
      "()V",
      Java_netscape_jsdebug_DebugController_sendInterrupt0
     }
  },
  {"netscape/jsdebug/DebugController",
     {"_setController",
      "(Z)V",
      Java_netscape_jsdebug_DebugController__1setController
     }
  },
  {"netscape/jsdebug/DebugController",
     {"executeScriptInStackFrame0",
      "(Lnetscape/jsdebug/JSStackFrameInfo;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;",
      Java_netscape_jsdebug_DebugController_executeScriptInStackFrame0
     }
  },
  {"netscape/jsdebug/DebugController",
     {"getNativeMajorVersion",
      "()I",
      Java_netscape_jsdebug_DebugController_getNativeMajorVersion
     }
  },
  {"netscape/jsdebug/DebugController",
     {"getNativeMinorVersion",
      "()I",
      Java_netscape_jsdebug_DebugController_getNativeMinorVersion
     }
  },
  {"netscape/jsdebug/JSPC",
     {"getSourceLocation",
      "()Lnetscape/jsdebug/SourceLocation;",
      Java_netscape_jsdebug_JSPC_getSourceLocation
     }
  },
  {"netscape/jsdebug/JSSourceTextProvider",
     {"loadSourceTextItem0",
      "(Ljava/lang/String;)Lnetscape/jsdebug/SourceTextItem;",
      Java_netscape_jsdebug_JSSourceTextProvider_loadSourceTextItem0
     }
  },
  {"netscape/jsdebug/JSSourceTextProvider",
     {"refreshSourceTextVector",
      "()V",
      Java_netscape_jsdebug_JSSourceTextProvider_refreshSourceTextVector
     }
  },
  {"netscape/jsdebug/JSStackFrameInfo",
     {"getCaller0",
      "()Lnetscape/jsdebug/StackFrameInfo;",
      Java_netscape_jsdebug_JSStackFrameInfo_getCaller0
     }
  },
  {"netscape/jsdebug/JSStackFrameInfo",
     {"getPC",
      "()Lnetscape/jsdebug/PC;",
      Java_netscape_jsdebug_JSStackFrameInfo_getPC
     }
  },
  {"netscape/jsdebug/JSThreadState",
     {"countStackFrames",
      "()I",
      Java_netscape_jsdebug_JSThreadState_countStackFrames
     }
  },
  {"netscape/jsdebug/JSThreadState",
     {"getCurrentFrame",
      "()Lnetscape/jsdebug/StackFrameInfo;",
      Java_netscape_jsdebug_JSThreadState_getCurrentFrame
     }
  },
  {"netscape/jsdebug/Script",
     {"getClosestPC",
      "(I)Lnetscape/jsdebug/JSPC;",
      Java_netscape_jsdebug_Script_getClosestPC
     }
  }
};
#define NATIVE_COUNT (sizeof(_natives)/sizeof(_natives[0]))

/***************************************************************************/
/***************************************************************************/
PRBool 
jsdj_RegisterNatives(JSDJContext* jsdjc)
{
    int i;
    JNIEnv *env = jsdjc->env;

    ASSERT_CLEAR_EXCEPTION(env);

    for( i = 0; i < NATIVE_COUNT; i++ )
    {
        jclass clazz;
        if( 0 == (clazz = (*env)->FindClass(env, _natives[i].classname)) )
        {

            fprintf(stderr, "failed to find class %s while registering natives\n",
                    _natives[i].classname );

            PR_ASSERT(0);
            CHECK_CLEAR_EXCEPTION(env);
            return PR_FALSE;
        }
        if( 0 != (*env)->RegisterNatives(env, clazz, &_natives[i].native, 1) )
        {

            fprintf(stderr, "failed to register native %s in %s \n",
                    _natives[i].native.name,                        
                    _natives[i].classname );                        

            PR_ASSERT(0);
            CHECK_CLEAR_EXCEPTION(env);
            return PR_FALSE;
        }
    }
    ASSERT_CLEAR_EXCEPTION(env);
    return PR_TRUE;
}    



