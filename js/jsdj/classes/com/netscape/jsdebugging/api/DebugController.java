/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package com.netscape.jsdebugging.api;

import netscape.security.ForbiddenTargetException;

/**
 * This is the master control panel for observing events in the VM.
 * Each method setXHook() must be passed an object that extends
 * the interface XHook.  When an event of the specified type
 * occurs, a well-known method on XHook will be called (see the
 * various XHook interfacees for details).  The method call takes place
 * on the same thread that triggered the event in the first place,
 * so that any monitors held by the thread which triggered the hook
 * will still be owned in the hook method.
 */
public interface DebugController
{
    public static final long SUPPORTS_EXEC_TO_VALUE             = 1L << 0;
    public static final long SUPPORTS_CATCH_EXEC_ERRORS         = 1L << 1;
    public static final long SUPPORTS_FRAME_CALL_SCOPE_THIS     = 1L << 2;

    /**
     * Return flags which indicate what 'extended' capabilities are supported
     * by the implementing api
     *
     * NOTE: this function is only available for version 1.1 and above
     *     i.e. if(getMajorVersion() > 1 || getMinorVersion() >= 1)
     *
     */
    public long getSupportsFlags();


    /**
     * Request notification of Script loading events.  Whenever a Script
     * is loaded into or unloaded from the VM the appropriate method of 
     * the ScriptHook argument will be called.
     * returns the previous hook object.
     */
    public ScriptHook setScriptHook(ScriptHook h)
        throws ForbiddenTargetException;

    /**
     * Find the current observer of Script events, or return null if there
     * is none.
     */
    public ScriptHook getScriptHook()
        throws ForbiddenTargetException;

    /**
     * Set a hook at the given program counter value.  When
     * a thread reaches that instruction, a ThreadState object will be
     * created and the appropriate method of the hook object
     * will be called.
     * returns the previous hook object.
     */

    public InstructionHook setInstructionHook(
        PC pc,
        InstructionHook h) 
        throws ForbiddenTargetException;

    /**
     * Get the hook at the given program counter value, or return
     * null if there is none.
     */
    public InstructionHook getInstructionHook(PC pc)
        throws ForbiddenTargetException;


    public InterruptHook setInterruptHook( InterruptHook h )
        throws ForbiddenTargetException;

    public InterruptHook getInterruptHook()
        throws ForbiddenTargetException;

    public void sendInterrupt()
        throws ForbiddenTargetException;

    public void sendInterruptStepInto(ThreadStateBase debug)
        throws ForbiddenTargetException;

    public void sendInterruptStepOver(ThreadStateBase debug)
        throws ForbiddenTargetException;

    public void sendInterruptStepOut(ThreadStateBase debug)
        throws ForbiddenTargetException;

    public void reinstateStepper(ThreadStateBase debug)
        throws ForbiddenTargetException;


    public DebugBreakHook setDebugBreakHook( DebugBreakHook h )
        throws ForbiddenTargetException;

    public DebugBreakHook getDebugBreakHook()
        throws ForbiddenTargetException;

    public ExecResult executeScriptInStackFrame( JSStackFrameInfo frame,
                                                 String text,
                                                 String filename,
                                                 int lineno )
        throws ForbiddenTargetException;

    public ExecResult executeScriptInStackFrameValue( JSStackFrameInfo frame,
                                                      String text,
                                                      String filename,
                                                      int lineno )
        throws ForbiddenTargetException;

    public JSErrorReporter getErrorReporter()
        throws ForbiddenTargetException;

    public JSErrorReporter setErrorReporter(JSErrorReporter er)
        throws ForbiddenTargetException;

    public void iterateScripts(ScriptHook h)
        throws ForbiddenTargetException;

    public int getMajorVersion();
    public int getMinorVersion();
}
