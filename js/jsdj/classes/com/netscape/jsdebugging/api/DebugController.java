/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
