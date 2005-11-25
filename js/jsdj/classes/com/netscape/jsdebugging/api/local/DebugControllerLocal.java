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

package com.netscape.jsdebugging.api.local;

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
public class DebugControllerLocal implements DebugController
{
    public long getSupportsFlags()
    {
        return  SUPPORTS_EXEC_TO_VALUE          |
                SUPPORTS_CATCH_EXEC_ERRORS      |
                SUPPORTS_FRAME_CALL_SCOPE_THIS  ;
    }            

    public static DebugControllerLocal getDebugController()
        throws ForbiddenTargetException
    {
        if( null == _controllerLocal )
            _controllerLocal = new DebugControllerLocal();
        if( null == _controllerLocal || null == _controllerLocal._controller )
            throw new ForbiddenTargetException();
        return _controllerLocal;        
    }

    private DebugControllerLocal()
    {
        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            _controller = netscape.jsdebug.DebugController.getDebugController();
            if( null != _controller && 0 == _controller.getNativeContext() )
            {
                _controller = null;
                System.err.println("Unable to load natives");
            }
        }
        catch( ForbiddenTargetException e )
        {
            System.out.println(e);
            _controller = null;
        }
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
        ScriptHookLocal newWrapper = null;
        if( null != h )
            newWrapper = new ScriptHookLocal(h);

        netscape.jsdebug.ScriptHook oldWrapper = 
                _controller.setScriptHook(newWrapper);

        if( null != oldWrapper && oldWrapper instanceof ScriptHookLocal )
            return ((ScriptHookLocal)oldWrapper).getWrappedHook();
        return null;
    }

    /**
     * Find the current observer of Script events, or return null if there
     * is none.
     */
    public ScriptHook getScriptHook()
    {
        netscape.jsdebug.ScriptHook oldWrapper = 
                _controller.getScriptHook();
        if( null != oldWrapper && oldWrapper instanceof ScriptHookLocal )
            return ((ScriptHookLocal)oldWrapper).getWrappedHook();
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

        netscape.jsdebug.JSPC newWrappedPC = null;
        if( null != pc )
            newWrappedPC = ((JSPCLocal)pc).getWrappedJSPC();

        InstructionHookLocal newWrapper = null;
        if( null != h )
            newWrapper = new InstructionHookLocal(h, newWrappedPC);

        netscape.jsdebug.InstructionHook oldWrapper =
                _controller.setInstructionHook(newWrappedPC, newWrapper);

        if( null != oldWrapper && oldWrapper instanceof InstructionHookLocal )
            return ((InstructionHookLocal)oldWrapper).getWrappedHook();
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
        netscape.jsdebug.JSPC newWrappedPC = ((JSPCLocal)pc).getWrappedJSPC();

        netscape.jsdebug.InstructionHook oldWrapper = 
                _controller.getInstructionHook(newWrappedPC);

        if( null != oldWrapper && oldWrapper instanceof InstructionHookLocal )
            return ((InstructionHookLocal)oldWrapper).getWrappedHook();
        return null;
    }

    public InterruptHook setInterruptHook( InterruptHook h )
        throws ForbiddenTargetException
    {
        InterruptHookLocal newWrapper = null;
        if( null != h )
            newWrapper = new InterruptHookLocal(h);

        netscape.jsdebug.InterruptHook oldWrapper = 
                _controller.setInterruptHook(newWrapper);

        if( null != oldWrapper && oldWrapper instanceof InterruptHookLocal )
            return ((InterruptHookLocal)oldWrapper).getWrappedHook();
        return null;
    }

    public InterruptHook getInterruptHook()
        throws ForbiddenTargetException
    {
        netscape.jsdebug.InterruptHook oldWrapper = 
                _controller.getInterruptHook();
        if( null != oldWrapper && oldWrapper instanceof InterruptHookLocal )
            return ((InterruptHookLocal)oldWrapper).getWrappedHook();
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
        DebugBreakHookLocal newWrapper = null;
        if( null != h )
            newWrapper = new DebugBreakHookLocal(h);

        netscape.jsdebug.DebugBreakHook oldWrapper = 
                _controller.setDebugBreakHook(newWrapper);

        if( null != oldWrapper && oldWrapper instanceof DebugBreakHookLocal )
            return ((DebugBreakHookLocal)oldWrapper).getWrappedHook();
        return null;
    }

    public DebugBreakHook getDebugBreakHook()
        throws ForbiddenTargetException
    {
        netscape.jsdebug.DebugBreakHook oldWrapper = 
                _controller.getDebugBreakHook();

        if( null != oldWrapper && oldWrapper instanceof DebugBreakHookLocal )
            return ((DebugBreakHookLocal)oldWrapper).getWrappedHook();
        return null;
    }

    public ExecResult executeScriptInStackFrame( JSStackFrameInfo frame,
                                                 String text,
                                                 String filename,
                                                 int lineno )
        throws ForbiddenTargetException
    {
        JSThreadStateLocal ts = (JSThreadStateLocal)frame.getThreadState();
        Evaluator e = new Evaluator(this,ts,frame,text,filename,lineno);
        return e.eval(Evaluator.TO_STRING);
    }

    public ExecResult executeScriptInStackFrameValue( JSStackFrameInfo frame,
                                                      String text,
                                                      String filename,
                                                      int lineno )
        throws ForbiddenTargetException
    {
        JSThreadStateLocal ts = (JSThreadStateLocal)frame.getThreadState();
        Evaluator e = new Evaluator(this,ts,frame,text,filename,lineno);
        return e.eval(Evaluator.TO_VALUE);
    }


    public JSErrorReporter setErrorReporter(JSErrorReporter h)
        throws ForbiddenTargetException
    {
        JSErrorReporterLocal newWrapper = null;
        if( null != h )
            newWrapper = new JSErrorReporterLocal(h);

        netscape.jsdebug.JSErrorReporter oldWrapper = 
                _controller.setErrorReporter(newWrapper);

        if( null != oldWrapper && oldWrapper instanceof JSErrorReporterLocal)
            return ((JSErrorReporterLocal)oldWrapper).getWrappedHook();
        return null;
    }

    public JSErrorReporter getErrorReporter()
        throws ForbiddenTargetException
    {
        netscape.jsdebug.JSErrorReporter oldWrapper = 
                _controller.getErrorReporter();

        if( null != oldWrapper && oldWrapper instanceof JSErrorReporterLocal)
            return ((JSErrorReporterLocal)oldWrapper).getWrappedHook();
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

    public netscape.jsdebug.DebugController getWrappedController()
    {
        return _controller;
    }

    // data...

    private netscape.jsdebug.DebugController _controller = null;

    private static DebugControllerLocal _controllerLocal = null;
}

class Evaluator 
    implements Callback, JSErrorReporter
{
    static final boolean TO_STRING = false;
    static final boolean TO_VALUE  = true;

    Evaluator(DebugControllerLocal dc,
              JSThreadStateLocal ts,
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
    }

    ExecResultLocal eval(boolean toValue)
    {
        _evalToValue = toValue;

        if( !_ts.isWaitingForResume() )
        {
            call(); // just call directly on this thread.
        }
        else
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
        }
        return _execResult;
    }

    // implement Callback    
    public synchronized void call()
    {
        JSErrorReporter oldER = null;
        boolean setER = false;
        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            oldER = _dc.setErrorReporter(this);
            setER = true;

            if(_evalToValue)
            {
                // we break this out as a separate method because 
                // netscape.jsdebug 1.0 had no Value class and we need to 
                // avoid any unnecessary references to netscape.jsdebug.Value.
                _doEvalToValue();
            }
            else 
            {
                String result = 
                    _dc.getWrappedController().executeScriptInStackFrame( 
                            ((JSStackFrameInfoLocal)_frame).getWrappedInfo(),
                            _text,
                            _filename,
                            _lineno);
                if(null == _execResult)
                    _execResult = new ExecResultLocal(result);
            }

        }
        catch(Exception e)
        {
            // eat it...
            e.printStackTrace();
            System.out.println(e);
        }

        if(setER)
            _dc.setErrorReporter(oldER);
        _completed = true;
        notify();
    }

    private void _doEvalToValue() throws Exception
    {
        netscape.jsdebug.Value result = 
            _dc.getWrappedController().executeScriptInStackFrameValue( 
                    ((JSStackFrameInfoLocal)_frame).getWrappedInfo(),
                    _text,
                    _filename,
                    _lineno);

        if(null == _execResult)
            _execResult = new ExecResultLocal(new ValueLocal(result));
    }
    

    // implements JSErrorReporter
    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
        _execResult = new ExecResultLocal(msg, filename, lineno, 
                                          linebuf, tokenOffset);
        return netscape.jsdebug.JSErrorReporter.RETURN;
    }

    private DebugControllerLocal _dc;
    private JSThreadStateLocal   _ts;
    private JSStackFrameInfo     _frame;
    private String               _text;
    private String               _filename;
    private int                  _lineno;
    private boolean              _completed;
    private boolean              _evalToValue;
    private ExecResultLocal      _execResult;
}    



