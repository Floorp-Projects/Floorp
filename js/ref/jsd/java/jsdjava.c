
#include "jsdj.h"

PR_PUBLIC_API(JSDJContext*)
JSDJ_CreateContext()
{
    return jsdj_CreateContext();
}    

PR_PUBLIC_API(void)
JSDJ_DestroyContext(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    jsdj_DestroyContext(jsdjc);
}    

PR_PUBLIC_API(void)
JSDJ_SetJNIEnvForCurrentThread(JSDJContext* jsdjc, JNIEnv* env)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    PR_ASSERT(env);
    jsdj_SetJNIEnvForCurrentThread(jsdjc, env);
}    

PR_PUBLIC_API(JNIEnv*)
JSDJ_GetJNIEnvForCurrentThread(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    return jsdj_GetJNIEnvForCurrentThread(jsdjc);
}    

PR_PUBLIC_API(void)
JSDJ_SetJSDContext(JSDJContext* jsdjc, JSDContext* jsdc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    PR_ASSERT(jsdc);
    jsdj_SetJSDContext(jsdjc, jsdc);
}    

PR_PUBLIC_API(JSDContext*)
JSDJ_GetJSDContext(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    return jsdj_GetJSDContext(jsdjc);
}    

PR_PUBLIC_API(PRBool)
JSDJ_RegisterNatives(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    return jsdj_RegisterNatives(jsdjc);
}    

PR_PUBLIC_API(void)
JSDJ_SetStartStopCallback(JSDJContext* jsdjc, JSDJStartStopCallback cb, 
                          void* closure)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    jsdj_SetStartStopCallback(jsdjc, cb, closure);
}

/***************************************************************************/
#ifdef JSD_STANDALONE_JAVA_VM

PR_PUBLIC_API(JNIEnv*)
JSDJ_CreateJavaVMAndStartDebugger(JSDJContext* jsdjc)
{
    JSDJ_ASSERT_VALID_CONTEXT(jsdjc);
    return jsdj_CreateJavaVMAndStartDebugger(jsdjc);
}    

#endif /* JSD_STANDALONE_JAVA_VM */
/***************************************************************************/
