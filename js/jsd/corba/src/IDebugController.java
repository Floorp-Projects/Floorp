/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

public interface IDebugController extends org.omg.CORBA.Object
{
    public int getMajorVersion();
    public int getMinorVersion();

    public IJSErrorReporter setErrorReporter(IJSErrorReporter er);
    public IJSErrorReporter getErrorReporter();

    public IScriptHook      setScriptHook(IScriptHook h);
    public IScriptHook      getScriptHook();
    
    public IJSPC            getClosestPC(IScript script, int line);

    public IJSSourceLocation getSourceLocation(IJSPC pc);

    public IJSExecutionHook setInterruptHook(IJSExecutionHook hook);
    public IJSExecutionHook getInterruptHook();

    public IJSExecutionHook setDebugBreakHook(IJSExecutionHook hook);
    public IJSExecutionHook getDebugBreakHook();

    public IJSExecutionHook setInstructionHook(IJSExecutionHook hook, IJSPC pc);
    public IJSExecutionHook getInstructionHook(IJSPC pc);

    public void setThreadContinueState(int threadID, int state);
    public void setThreadReturnValue(int threadID, String value);

    public void sendInterrupt();

    public void sendInterruptStepInto(int threadID);
    public void sendInterruptStepOver(int threadID);
    public void sendInterruptStepOut(int threadID);

    public void reinstateStepper(int threadID);


    public IExecResult executeScriptInStackFrame(int threadID,
                                                 IJSStackFrameInfo frame,
                                                 String text,
                                                 String filename,
                                                 int lineno);

    public boolean isRunningHook(int threadID);
    public boolean isWaitingForResume(int threadID);
    public void leaveThreadSuspended(int threadID);
    public void resumeThread(int threadID);

    public void iterateScripts(IScriptHook h);
}    

