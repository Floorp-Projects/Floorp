
/*
** Header for JavaScript Debugger JNI support (internal functions)
** jband 12-30-97                                          
*/

#ifndef jsdj_h___
#define jsdj_h___

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
#include "prassert.h"      /* for PR_ASSERT */
#include "jsdjava.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
PR_END_EXTERN_C

PR_BEGIN_EXTERN_C

/***************************************************************************/
/* defines copied from Java sources.
** NOTE: javah used to put these in the h files, but with JNI does not seem
** to do this anymore. Be careful with synchronization of these
**
*/

/* From: ThreadStateBase.java  */

#define THR_STATUS_UNKNOWN      0x01
#define THR_STATUS_ZOMBIE       0x02
#define THR_STATUS_RUNNING      0x03
#define THR_STATUS_SLEEPING     0x04
#define THR_STATUS_MONWAIT      0x05
#define THR_STATUS_CONDWAIT     0x06
#define THR_STATUS_SUSPENDED    0x07
#define THR_STATUS_BREAK        0x08

#define DEBUG_STATE_DEAD        0x01
#define DEBUG_STATE_RUN         0x02
#define DEBUG_STATE_RETURN      0x03
#define DEBUG_STATE_THROW       0x04

/***************************************************************************/
/* Our structures */

typedef struct JSDJContext
{
    JSDContext* jsdc;
    JNIEnv*     env;    /* XXX currently simplified for single threaded case*/
    jobject     controller;
    JSDJStartStopCallback   startStopCallback;
    void*       startStopCallbackData;


} JSDJContext;

/***************************************************************************/
/* Code validation support */

#ifdef DEBUG
extern void JSDJ_ASSERT_VALID_CONTEXT(JSDJContext* jsdjc);
#else
#define JSDJ_ASSERT_VALID_CONTEXT(x)     ((void)0)
#endif

/***************************************************************************/
/* higher level functions */

extern JSDJContext*
jsdj_CreateContext();

extern void
jsdj_DestroyContext(JSDJContext* jsdjc);

extern void
jsdj_SetJNIEnvForCurrentThread(JSDJContext* jsdjc, JNIEnv* env);

extern JNIEnv*
jsdj_GetJNIEnvForCurrentThread(JSDJContext* jsdjc);

extern void
jsdj_SetJSDContext(JSDJContext* jsdjc, JSDContext* jsdc);

extern JSDContext*
jsdj_GetJSDContext(JSDJContext* jsdjc);

extern PRBool
jsdj_RegisterNatives(JSDJContext* jsdjc);

extern void
jsdj_SetStartStopCallback(JSDJContext* jsdjc, JSDJStartStopCallback cb, 
                          void* closure);

/***************************************************************************/
#ifdef JSD_STANDALONE_JAVA_VM

extern JNIEnv*
jsdj_CreateJavaVMAndStartDebugger(JSDJContext* jsdjc);

/**
* extern JNIEnv*
* jsdj_CreateJavaVM(JSDContext* jsdc);
*/

#endif /* JSD_STANDALONE_JAVA_VM */
/***************************************************************************/

PR_END_EXTERN_C

#endif /* jsdj_h___ */

