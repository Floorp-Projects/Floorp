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

package com.netscape.jsdebugging.api.rhino;

import com.netscape.jsdebugging.api.*;
import netscape.security.ForbiddenTargetException;
import netscape.security.PrivilegeManager;

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
public class DebugControllerRhino implements DebugController
{
    public long getSupportsFlags()
    {
        return  0;
    }

    public static boolean setGlobalDebugManager(com.netscape.javascript.debug.IDebugManager manager)
    {
        if(null != _controller)
            return false;
        _controller = manager;
        return true;
    }

    public static DebugControllerRhino getDebugController()
        throws ForbiddenTargetException
    {
        if( null == _controllerRhino )
            _controllerRhino = new DebugControllerRhino();
        if( null == _controllerRhino )
            throw new ForbiddenTargetException();
        if( null == _controllerRhino._controller )
        {
            _controllerRhino = null;
            throw new ForbiddenTargetException();
        }
        return _controllerRhino;        
    }

    private DebugControllerRhino()
    {
    }

    /**
     * Request notification of Script loading events.  Whenever a Script
     * is loaded into or unloaded from the VM the appropriate method of 
     * the ScriptHook argument will be called.
     * returns the previous hook object.
     */
    public ScriptHook setScriptHook(ScriptHook h)
        throws ForbiddenTargetException
    {
        ScriptHookRhino newWrapper = null;
        if( null != h )
            newWrapper = new ScriptHookRhino(h);

        com.netscape.javascript.debug.IScriptHook oldWrapper = 
                _controller.setScriptHook(newWrapper);

        if( null != oldWrapper && oldWrapper instanceof ScriptHookRhino )
            return ((ScriptHookRhino)oldWrapper).getWrappedHook();
        return null;
    }

    /**
     * Find the current observer of Script events, or return null if there
     * is none.
     */
    public ScriptHook getScriptHook()
    {
        com.netscape.javascript.debug.IScriptHook oldWrapper = 
                _controller.getScriptHook();
        if( null != oldWrapper && oldWrapper instanceof ScriptHookRhino )
            return ((ScriptHookRhino)oldWrapper).getWrappedHook();
        return null;
    }

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
        throws ForbiddenTargetException
    {
        // XXX this is hack to deal with fact that the native v1.0 JSD 
        // for Navigator can't handle a null instruction hook.
        if( null == h )
            return null;

        com.netscape.javascript.debug.IPC newWrappedPC = null;
        if( null != pc )
            newWrappedPC = ((JSPCRhino)pc).getWrappedJSPC();

        InstructionHookRhino newWrapper = null;
        if( null != h )
            newWrapper = new InstructionHookRhino(h, newWrappedPC);

        com.netscape.javascript.debug.IInstructionHook oldWrapper =
                _controller.setInstructionHook(newWrappedPC, newWrapper);

        if( null != oldWrapper && oldWrapper instanceof InstructionHookRhino )
            return ((InstructionHookRhino)oldWrapper).getWrappedHook();
        return null;
    }

    /**
     * Get the hook at the given program counter value, or return
     * null if there is none.
     */
    public InstructionHook getInstructionHook(PC pc)
        throws ForbiddenTargetException
    {
        if( null == pc )
            return null;
        com.netscape.javascript.debug.IPC newWrappedPC = ((JSPCRhino)pc).getWrappedJSPC();

        com.netscape.javascript.debug.IInstructionHook oldWrapper = 
                _controller.getInstructionHook(newWrappedPC);

        if( null != oldWrapper && oldWrapper instanceof InstructionHookRhino )
            return ((InstructionHookRhino)oldWrapper).getWrappedHook();
        return null;
    }

    public InterruptHook setInterruptHook( InterruptHook h )
        throws ForbiddenTargetException
    {
        InterruptHookRhino newWrapper = null;
        if( null != h )
            newWrapper = new InterruptHookRhino(h);

        com.netscape.javascript.debug.IInterruptHook oldWrapper = 
                _controller.setInterruptHook(newWrapper);

        if( null != oldWrapper && oldWrapper instanceof InterruptHookRhino )
            return ((InterruptHookRhino)oldWrapper).getWrappedHook();
        return null;
    }

    public InterruptHook getInterruptHook()
        throws ForbiddenTargetException
    {
        com.netscape.javascript.debug.IInterruptHook oldWrapper = 
                _controller.getInterruptHook();
        if( null != oldWrapper && oldWrapper instanceof InterruptHookRhino )
            return ((InterruptHookRhino)oldWrapper).getWrappedHook();
        return null;
    }

    public void sendInterrupt()
        throws ForbiddenTargetException
    {
        _controller.sendInterrupt();
    }

    public void sendInterruptStepInto(ThreadStateBase debug)
    {
        System.err.println("call to unimplemented function sendInterruptStepInto() failed");
    }

    public void sendInterruptStepOver(ThreadStateBase debug)
    {
        System.err.println("call to unimplemented function sendInterruptStepOver() failed");
    }

    public void sendInterruptStepOut(ThreadStateBase debug)
    {
        System.err.println("call to unimplemented function sendInterruptStepOut() failed");
    }

    public void reinstateStepper(ThreadStateBase debug)
    {
        System.err.println("call to unimplemented function reinstateStepper() failed");
    }

    public DebugBreakHook setDebugBreakHook( DebugBreakHook h )
        throws ForbiddenTargetException
    {
        DebugBreakHookRhino newWrapper = null;
        if( null != h )
            newWrapper = new DebugBreakHookRhino(h);

        com.netscape.javascript.debug.IDebugBreakHook oldWrapper = 
                _controller.setDebugBreakHook(newWrapper);

        if( null != oldWrapper && oldWrapper instanceof DebugBreakHookRhino )
            return ((DebugBreakHookRhino)oldWrapper).getWrappedHook();
        return null;
    }

    public DebugBreakHook getDebugBreakHook()
        throws ForbiddenTargetException
    {
        com.netscape.javascript.debug.IDebugBreakHook oldWrapper = 
                _controller.getDebugBreakHook();

        if( null != oldWrapper && oldWrapper instanceof DebugBreakHookRhino )
            return ((DebugBreakHookRhino)oldWrapper).getWrappedHook();
        return null;
    }

    public ExecResult executeScriptInStackFrame( JSStackFrameInfo frame,
                                                 String text,
                                                 String filename,
                                                 int lineno )
        throws ForbiddenTargetException
    {
        JSThreadStateRhino ts = (JSThreadStateRhino)frame.getThreadState();

        if( ts.isWaitingForResume() )
        {
            Evaluator e = new Evaluator(this,ts,frame,text,filename,lineno);
            return new ExecResultRhino( e.eval() );
        }
        else
        {
            return new ExecResultRhino( 
                        _controller.executeScriptInStackFrame( 
                                ((JSStackFrameInfoRhino)frame).getWrappedInfo(),
                                text,
                                filename,
                                lineno) );
        }
    }

    public ExecResult executeScriptInStackFrameValue( JSStackFrameInfo frame,
                                                      String text,
                                                      String filename,
                                                      int lineno )
        throws ForbiddenTargetException
    {
        return null; // XXX implement this...
    }

    public JSErrorReporter setErrorReporter(JSErrorReporter h)
        throws ForbiddenTargetException
    {
        JSErrorReporterRhino newWrapper = null;
        if( null != h )
            newWrapper = new JSErrorReporterRhino(h);

        com.netscape.javascript.debug.IErrorReporter oldWrapper = 
                _controller.setErrorReporter(newWrapper);

        if( null != oldWrapper && oldWrapper instanceof JSErrorReporterRhino)
            return ((JSErrorReporterRhino)oldWrapper).getWrappedHook();
        return null;
    }

    public JSErrorReporter getErrorReporter()
        throws ForbiddenTargetException
    {
        com.netscape.javascript.debug.IErrorReporter oldWrapper = 
                _controller.getErrorReporter();

        if( null != oldWrapper && oldWrapper instanceof JSErrorReporterRhino)
            return ((JSErrorReporterRhino)oldWrapper).getWrappedHook();
        return null;
    }

    public void iterateScripts(ScriptHook h)
    {
        // XXX does nothing at present...
    }

    public int getMajorVersion()
    {
        return _controller.getMajorVersion();
    }
    public int getMinorVersion()
    {
        return _controller.getMinorVersion();
    }

    public com.netscape.javascript.debug.IDebugManager getWrappedController()
    {
        return _controller;
    }

    // data...

    private static com.netscape.javascript.debug.IDebugManager _controller = null;

    private static DebugControllerRhino _controllerRhino = null;
}

class Evaluator implements Callback
{
    public Evaluator(DebugControllerRhino dc,
                     JSThreadStateRhino ts,
                     JSStackFrameInfo frame,
                     String text,
                     String filename,
                     int lineno)
    {
        _dc         = dc      ;
        _ts         = ts      ;
        _frame      = frame   ;
        _text       = text    ;
        _filename   = filename;
        _lineno     = lineno  ;
        _completed  = false;
        _result     = null;
    }

    public String eval()
    {
        if( ! _completed && _ts.addCallbackEvent(this) )
        {
            synchronized(this)
            {
                while( ! _completed )
                {
                    try 
                    {
                        wait();
                    }
                    catch(Exception e)
                    {
                       // eat it...
                        e.printStackTrace();
                        System.out.println(e);
                        break;
                    }
                }
            }
        }
        return _result;
    }

    // implement Callback    
    public synchronized void call()
    {
        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            _result = 
                _dc.getWrappedController().executeScriptInStackFrame( 
                            ((JSStackFrameInfoRhino)_frame).getWrappedInfo(),
                            _text,
                            _filename,
                            _lineno);
        }
        catch(Exception e)
        {
            // eat it...
            e.printStackTrace();
            System.out.println(e);
        }
        _completed = true;
        notify();
    }

    private DebugControllerRhino _dc;
    private JSThreadStateRhino   _ts;
    private JSStackFrameInfo     _frame;
    private String               _text;
    private String               _filename;
    private int                  _lineno;
    private boolean              _completed;
    private String               _result;
}    



