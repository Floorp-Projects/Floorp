
/*
** Header for JavaScript Debugger JNI interfaces
** jband 12-30-97
*/

#ifndef jsdjava_h___
#define jsdjava_h___

/* Get prtypes.h included first. After that we can use PR macros for doing
*  this extern "C" stuff!
*/ 
#ifdef __cplusplus
extern "C"
{
#endif
#include "prtypes.h"
#ifdef __cplusplus
}
#endif

PR_BEGIN_EXTERN_C
#include "jsdebug.h"
#include "jni.h"
PR_END_EXTERN_C


PR_BEGIN_EXTERN_C

/***************************************************************************/
/* Opaque typedefs for handles */

typedef struct JSDJContext        JSDJContext;

/***************************************************************************/
/* High Level functions */

extern PR_PUBLIC_API(JSDJContext*)
JSDJ_CreateContext();

extern PR_PUBLIC_API(void)
JSDJ_DestroyContext(JSDJContext* jsdjc);

extern PR_PUBLIC_API(void)
JSDJ_SetJNIEnvForCurrentThread(JSDJContext* jsdjc, JNIEnv* env);

extern PR_PUBLIC_API(JNIEnv*)
JSDJ_GetJNIEnvForCurrentThread(JSDJContext* jsdjc);

extern PR_PUBLIC_API(void)
JSDJ_SetJSDContext(JSDJContext* jsdjc, JSDContext* jsdc);

extern PR_PUBLIC_API(JSDContext*)
JSDJ_GetJSDContext(JSDJContext* jsdjc);

extern PR_PUBLIC_API(PRBool)
JSDJ_RegisterNatives(JSDJContext* jsdjc);

#define JSDJ_START_SUCCESS  1
#define JSDJ_START_FAILURE  2
#define JSDJ_STOP           3

typedef void
(*JSDJStartStopCallback)(JSDJContext* jsdjc, int event, void *closure);

extern PR_PUBLIC_API(void)
JSDJ_SetStartStopCallback(JSDJContext* jsdjc, JSDJStartStopCallback cb, 
                          void* closure);

/***************************************************************************/
#ifdef JSD_STANDALONE_JAVA_VM

extern PR_PUBLIC_API(JNIEnv*)
JSDJ_CreateJavaVMAndStartDebugger(JSDJContext* jsdjc);

#endif /* JSD_STANDALONE_JAVA_VM */
/***************************************************************************/

PR_END_EXTERN_C

#endif /* jsdjava_h___ */

