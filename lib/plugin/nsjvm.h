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
// NETSCAPE JAVA VM PLUGIN EXTENSIONS
// 
// This interface allows a Java virtual machine to be plugged into
// Communicator to implement the APPLET tag and host applets.
////////////////////////////////////////////////////////////////////////////////

#ifndef nsjvm_h___
#define nsjvm_h___

#include "nsplugin.h"
#include "jri.h"        // XXX for now
#include "jni.h"
#include "prthread.h"

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Manager
// This interface defines additional entry points that are available
// to JVM plugins for browsers that support JVM plugins.

class NPIJVMPluginManager : public NPIPluginManager {
public:

    virtual void NP_LOADDS
    BeginWaitCursor(void) = 0;

    virtual void NP_LOADDS
    EndWaitCursor(void) = 0;

    virtual const char* NP_LOADDS
    GetProgramPath(void) = 0;

    virtual const char* NP_LOADDS
    GetTempDirPath(void) = 0;

    enum FileNameType { SIGNED_APPLET_DBNAME, TEMP_FILENAME };

    virtual nsresult NP_LOADDS
    GetFileName(const char* fn, FileNameType type,
                char* resultBuf, PRUint32 bufLen) = 0;

    virtual nsresult NP_LOADDS
    NewTempFileName(const char* prefix, char* resultBuf, PRUint32 bufLen) = 0;

    virtual PRBool NP_LOADDS
    HandOffJSLock(PRThread* oldOwner, PRThread* newOwner) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Debugger Stuff (XXX move to subclass)

    virtual PRBool NP_LOADDS
    SetDebugAgentPassword(PRInt32 pwd) = 0;

};

#define NP_IJVMPLUGINMANAGER_IID                     \
{ /* a1e5ed50-aa4a-11d1-85b2-00805f0e4dfe */         \
    0xa1e5ed50,                                      \
    0xaa4a,                                          \
    0x11d1,                                          \
    {0x85, 0xb2, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////

enum JVMStatus {
    JVMStatus_Enabled,  // but not Running
    JVMStatus_Disabled, // explicitly disabled
    JVMStatus_Running,  // enabled and started
    JVMStatus_Failed    // enabled but failed to start
};

enum JVMError {
    JVMError_Ok                 = NPPluginError_NoError,
    JVMError_Base               = 0x1000,
    JVMError_InternalError      = JVMError_Base,
    JVMError_NoClasses,
    JVMError_WrongClasses,
    JVMError_JavaError,
    JVMError_NoDebugger
};

enum JVMDebugPort {
    JVMDebugPort_None           = 0,
    JVMDebugPort_SharedMemory   = -1
    // anything else is a port number
};

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Interface
// This interface defines additional entry points that a plugin developer needs
// to implement in order to implement a Java virtual machine plugin. 

class NPIJVMPlugin : public NPIPlugin {
public:

    virtual JVMStatus NP_LOADDS
    StartupJVM() = 0;

    virtual JVMStatus NP_LOADDS
    ShutdownJVM(PRBool fullShutdown = PR_TRUE) = 0;

    virtual PRBool NP_LOADDS
    GetJVMEnabled() = 0;

    virtual void NP_LOADDS
    SetJVMEnabled(PRBool enable) = 0;

    virtual JVMStatus NP_LOADDS
    GetJVMStatus() = 0;

    // Find or create a JNIEnv for the specified thread. The thread
    // parameter may be NULL indicating the current thread.
    // XXX JNIEnv*
    virtual JRIEnv* NP_LOADDS
    EnsureExecEnv(PRThread* thread = NULL) = 0;

    virtual void NP_LOADDS
    AddToClassPath(const char* dirPath) = 0;

    virtual void NP_LOADDS
    ShowConsole() = 0;

    virtual void NP_LOADDS
    HideConsole() = 0;

    virtual PRBool NP_LOADDS
    IsConsoleVisible() = 0;

    virtual void NP_LOADDS
    PrintToConsole(const char* msg) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Debugger Stuff (XXX move to subclass)

    virtual JVMError NP_LOADDS
    StartDebugger(JVMDebugPort port) = 0;
    
};

#define NP_IJVMPLUGIN_IID                            \
{ /* da6f3bc0-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xda6f3bc0,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
// Java VM Plugin Instance Peer Interface
// This interface provides additional hooks into the plugin manager that allow 
// a plugin to implement the plugin manager's Java virtual machine.

enum NPTagAttributeName {
    NPTagAttributeName_Width,
    NPTagAttributeName_Height,
    NPTagAttributeName_Classid,
    NPTagAttributeName_Code,
    NPTagAttributeName_Codebase,
    NPTagAttributeName_Docbase,
    NPTagAttributeName_Archive,
    NPTagAttributeName_Name,
    NPTagAttributeName_MayScript
};

class NPIJVMPluginInstancePeer : public NPIPluginInstancePeer {
public:

    // XXX Does this overlap with GetArgNames/GetArgValues?
    // XXX What happens if someone says something like:
    //     <object codebase=...> <param name=codebase value=...>
    // Which takes precedent?
    virtual char* NP_LOADDS
    GetAttribute(NPTagAttributeName name) = 0;

    // XXX reload method?

    // Returns a unique id for the current document on which the
    // plugin is displayed.
    virtual PRUint32 NP_LOADDS
    GetDocumentID() = 0;
    
};

#define NP_IJVMPLUGININSTANCEPEER_IID                \
{ /* 27b42df0-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x27b42df0,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////
#endif /* nsjvm_h___ */
