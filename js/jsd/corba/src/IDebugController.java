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
 * Netscape Communications Corporation.
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

