/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Methods to support the JVM Plugin API
////////////////////////////////////////////////////////////////////////////////

#include "npglue.h"
#include "prefapi.h"
#include "xp_str.h"
#include "libmocha.h"

#include "nsjvm.h"

#if 0   // XXX for now

static int PR_CALLBACK
JavaPrefChanged(const char *prefStr, void* data)
{
    nsPluginManager* mgr = (nsPluginManager*)data;
    PRBool prefBool;
    PREF_GetBoolPref(prefStr, &prefBool);
    mgr->SetJVMEnabled(prefBool);
    return 0;
} 

void
nsPluginManager::EnsurePrefCallbackRegistered(void)
{
    if (fRegisteredJavaPrefChanged != PR_TRUE) {
        fRegisteredJavaPrefChanged = PR_TRUE;
        PREF_RegisterCallback("security.enable_java", JavaPrefChanged, this);
        JavaPrefChanged("security.enable_java", this);
    }
}

PRBool
nsPluginManager::GetJVMEnabled(void)
{
    EnsurePrefCallbackRegistered();
    return fJVM->GetJVMEnabled();
}

void
nsPluginManager::SetJVMEnabled(PRBool enable)
{
    return fJVM->SetJVMEnabled(enable);
}

JVMStatus
nsPluginManager::GetJVMStatus(void)
{
    EnsurePrefCallbackRegistered();
    return fJVM->GetJVMStatus();
}

////////////////////////////////////////////////////////////////////////////////

// Should be in a header; must solve build-order problem first
extern void VR_Initialize(JRIEnv* env);
extern void SU_Initialize(JRIEnv * env);

JVMStatus
nsPluginManager::StartupJVM()
{
    // Be sure to check the prefs first before asking java to startup.
    if (GetJVMStatus() == JVMStatus_Disabled) {
        return JVMStatus_Disabled;
    }
    JVMStatus status = fJVM->StartupJVM();
    if (status == JVMStatus_Ok) {
        JRIEnv* env = fJVM->EnsureExecEnv();

        /* initialize VersionRegistry native routines */
        /* it is not an error that prevents java from starting if this stuff throws exceptions */
        VR_Initialize(env);
        if (JRI_ExceptionOccurred(env)) {
#if DEBUG
            fJVM->PrintToConsole("LJ:  VR_Initialize failed.  Bugs to dveditz.\n");
#endif	
            goto error;
        }
	
        SU_Initialize(env);
        if (JRI_ExceptionOccurred(env)) {
#if DEBUG
            fJVM->PrintToConsole("LJ:  SU_Initialize failed.  Bugs to atotic.\n");
#endif	
            goto error;
        }
    }
    return status;

  error:
#if DEBUG
    JRI_ExceptionDescribe(env);
#endif	
    JRI_ExceptionClear(env);
    ShutdownJVM();
    return JVMStatus_JavaError;
}

JVMStatus
nsPluginManager::ShutdownJVM(PRBool fullShutdown = PR_TRUE)
{
    return fJVM->ShutdownJVM();
}

#endif // 0

////////////////////////////////////////////////////////////////////////////////

void NP_LOADDS
nsPluginManager::BeginWaitCursor(void)
{
    if (fWaiting == 0) {
#ifdef XP_PC
        fOldCursor = (void*)GetCursor();
	HCURSOR newCursor = LoadCursor(NULL, IDC_WAIT);
	if (newCursor)
	    SetCursor(newCursor);
#endif
#ifdef XP_MAC
	startAsyncCursors();
#endif
    }
    fWaiting++;
}

void NP_LOADDS
nsPluginManager::EndWaitCursor(void)
{
    fWaiting--;
    if (fWaiting == 0) {
#ifdef XP_PC
	if (fOldCursor)
	    SetCursor((HCURSOR)fOldCursor);
#endif
#ifdef XP_MAC
	stopAsyncCursors();
#endif
        fOldCursor = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

const char* NP_LOADDS
nsPluginManager::GetProgramPath(void)
{
    return fProgramPath;
}

const char* NP_LOADDS
nsPluginManager::GetTempDirPath(void)
{
    // XXX I don't need a static really, the browser holds the tempDir name
    // as a static string -- it's just the XP_TempDirName that strdups it.
    static const char* tempDirName = NULL;
    if (tempDirName == NULL)
        tempDirName = XP_TempDirName();
    return tempDirName;
}

nsresult NP_LOADDS
nsPluginManager::GetFileName(const char* fn, FileNameType type,
                             char* resultBuf, PRUint32 bufLen)
{
    // XXX This should be rewritten so that we don't have to malloc the name.
    XP_FileType filetype;

    if (type == SIGNED_APPLET_DBNAME)
        filetype = xpSignedAppletDB;
    else if (type == TEMP_FILENAME)
        filetype = xpTemporary;
    else 
        return NS_ERROR_ILLEGAL_VALUE;

    char* tempName = WH_FileName(fn, filetype);
    if (tempName != NULL) return NS_ERROR_OUT_OF_MEMORY;
    XP_STRNCPY_SAFE(resultBuf, tempName, bufLen);
    XP_FREE(tempName);
    return NS_OK;
}

nsresult NP_LOADDS
nsPluginManager::NewTempFileName(const char* prefix, char* resultBuf, PRUint32 bufLen)
{
    // XXX This should be rewritten so that we don't have to malloc the name.
    char* tempName = WH_TempName(xpTemporary, prefix);
    if (tempName != NULL) return NS_ERROR_OUT_OF_MEMORY;
    XP_STRNCPY_SAFE(resultBuf, tempName, bufLen);
    XP_FREE(tempName);
    return NS_OK;
}

PRBool NP_LOADDS
nsPluginManager::HandOffJSLock(PRThread* oldOwner, PRThread* newOwner)
{
    return LM_HandOffJSLock(oldOwner, newOwner);
}

////////////////////////////////////////////////////////////////////////////
// Debugger Stuff

extern "C" HWND FindNavigatorHiddenWindow(void);

PRBool NP_LOADDS
nsPluginManager::SetDebugAgentPassword(PRInt32 pwd)
{
#if defined(XP_PC) && defined(_WIN32)
    HWND win = FindNavigatorHiddenWindow();
    HANDLE sem;
    long err;

    /* set up by aHiddenFrameClass in CNetscapeApp::InitInstance */
    err = SetWindowLong(win, 0, pwd);	
    if (err == 0) {
//        PR_LOG(NSJAVA, PR_LOG_ALWAYS,
//               ("SetWindowLong returned %ld (err=%d)\n", err, GetLastError()));
        /* continue so that we try to wake up the debugger */
    }
    sem = OpenSemaphore(SEMAPHORE_MODIFY_STATE, FALSE, "Netscape-Symantec Debugger");
    if (sem) {
        ReleaseSemaphore(sem, 1, NULL);
        CloseHandle(sem);
    }
    return PR_TRUE;
#else
    return PR_FALSE;
#endif
}

////////////////////////////////////////////////////////////////////////////////

#if 0 // XXX for now

PRBool
nsPluginManager::StartDebugger(JVMDebugPort port)
{
    JVMError err = fJVM->StartDebugger(port);
    if (err != JVMError_NoError) {
        JRIEnv* env = fJVM->EnsureJNIEnv();
        ReportJVMError(env, err);
        return PR_FALSE;
    }
    return PR_TRUE;
}

void
nsPluginManager::ReportJVMError(JRIEnv* env, JVMError err)
{
    MWContext* cx = XP_FindSomeContext();
    char *s;
    switch (err) {
      case LJStartupError_NoClasses: {
          const char* cp = CLASSPATH();
          cp = lj_ConvertToPlatformPathList(cp);
          s = PR_smprintf(XP_GetString(XP_JAVA_NO_CLASSES),
                          lj_jar_name, lj_jar_name,
                          (cp ? cp : "<none>"));
          free((void*)cp);
          break;
      }

      case LJStartupError_JavaError: {
          const char* msg = GetJavaErrorString(env);
#ifdef DEBUG
          JRI_ExceptionDescribe(env);
#endif
          s = PR_smprintf(XP_GetString(XP_JAVA_STARTUP_FAILED), 
                          (msg ? msg : ""));
          if (msg) free((void*)msg);
          break;
      }
      case LJStartupError_NoDebugger: {
          s = PR_smprintf(XP_GetString(XP_JAVA_DEBUGGER_FAILED));
          break;
      }
      default:
        return;	/* don't report anything */
    }
    if (s) {
        FE_Alert(cx, s);
        free(s);
    }
}

/* stolen from java_lang_Object.h (jri version) */
#define classname_java_lang_Object	"java/lang/Object"
#define name_java_lang_Object_toString	"toString"
#define sig_java_lang_Object_toString 	"()Ljava/lang/String;"

const char*
nsPluginManager::GetJavaErrorString(JRIEnv* env)
{
    /* XXX javah is a pain wrt mixing JRI and JDK native methods. 
       Since we need to call a method on Object, we'll do it the hard way 
       to avoid using javah for this.
       Maybe we need a new JRI entry point for toString. Yikes! */

    const char* msg;
    struct java_lang_Class* classObject;
    JRIMethodID toString;
    struct java_lang_String* excString;
    struct java_lang_Throwable* exc = JRI_ExceptionOccurred(env);

    if (exc == NULL) {
        return strdup("");	/* XXX better "no error" message? */
    }

    /* Got to do this before trying to find a class (like Object). 
       This is because the runtime refuses to do this with a pending 
       exception! I think it's broken. */

    JRI_ExceptionClear(env);

    classObject = (struct java_lang_Class*)
        JRI_FindClass(env, classname_java_lang_Object);
    toString = JRI_GetMethodID(env, classObject,
                               name_java_lang_Object_toString,
                               sig_java_lang_Object_toString);
    excString = (struct java_lang_String *)
        JRI_CallMethod(env)(env, JRI_CallMethod_op,
                            (struct java_lang_Object*)exc, toString);

    /* XXX change to JRI_GetStringPlatformChars */
    msg = JRI_GetStringPlatformChars(env, excString, NULL, 0);
    if (msg == NULL) return NULL;
    return strdup(msg);
}

#endif // 0

////////////////////////////////////////////////////////////////////////////////

