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

/*
* 'Model' that manages most interaction with debug API
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;
import com.netscape.jsdebugging.api.*;

public class ControlTyrant
    extends Observable
    implements Observer, Target, JSErrorReporter
{
    public static final int RUNNING = 0;
    public static final int STOPPED = 1;

    // ctors

    public ControlTyrant(Emperor emperor) throws ForbiddenTargetException
    {
        super();
        _app = Application.application();
        _emperor = emperor;

        PrivilegeManager.enablePrivilege("Debugger");
        _dc = _emperor.getDebugController();

        _semaphore = new CtrlSemaphore();

        // set our interrupt hook (with chaining)
        _interruptHook = new CtrlInterruptHook(this);
        _interruptHook.setNextHook(_dc.setInterruptHook(_interruptHook));

        // set our debugBreak hook (with chaining)
        _debugBreakHook = new CtrlDebugBreakHook(this);
        _debugBreakHook.setNextHook(_dc.setDebugBreakHook(_debugBreakHook));

        _dc.setErrorReporter(this);

        if( _emperor.getHostMode() == _emperor.REMOTE_SERVER )
            _useServerSideStepper = true;
        else
            _useServerSideStepper = false;

        if(AS.DEBUG)
        {
            _uiThreadForAssertCheck = Thread.currentThread();
        }

        // add ourself as observer of ???

        // notify that we are running (if anyone cares at this point!)
        _state = RUNNING;
        _notifyOfStateChange();
    }

    // accessors

    public int      getState()      {return _state;}
    public boolean  getInterrupt()  {return _interrupt;}
    public boolean  getEnabled()    {return _enabled;}
    public Emperor  getEmperor()    {return _emperor;}

    public JSThreadState getThreadState()
    {
        if( STOPPED == _state )
            return _threadState;
        if(AS.S)ER.T(false,"getThreadState called when not really stopped",this);
        return null;
    }

    public JSPC getPC()
    {
        if( STOPPED == _state )
            return _pc;
        if(AS.S)ER.T(false,"getPC called when not really stopped",this);
        return null;
    }

    public JSSourceLocation getSourceLocation()
    {
        if( STOPPED == _state )
            return _sourceLocation;
        if(AS.S)ER.T(false,"getSourceLocation called when not really stopped",this);
        return null;
    }

    // command handlers

    public synchronized void interrupt(boolean b)
    {
        PrivilegeManager.enablePrivilege("Debugger");
        if( b == _interrupt )
            return;
        _interrupt = b;
        if( _interrupt )
        {
            _stepHandler = null;
            _serverSideStepperIsSet = false;
            _dc.sendInterrupt();
        }
    }

    public synchronized void runit()
    {
        _continueAndNotify(true);
    }

    public synchronized void abort()
    {
        if( _state != STOPPED || _semaphore.available() || null == _threadState )
        {
            if(AS.S)ER.T(false,"abort called when not really stopped",this);
            return;
        }

        _threadState.setContinueState( ThreadStateBase.DEBUG_STATE_THROW );
        _continueAndNotify(true);
    }

    private static final int STEP_OVER = 0;
    private static final int STEP_INTO = 1;
    private static final int STEP_OUT  = 2;

    public synchronized void stepOver()
    {
        if( _useServerSideStepper )
            _setServerSideStepper(STEP_OVER);
        else
            _setStepHandler( new StepOver(_threadState, _sourceLocation, _pc) );
    }

    public synchronized void stepInto()
    {
        if( _useServerSideStepper )
            _setServerSideStepper(STEP_INTO);
        else
            _setStepHandler( new StepInto(_sourceLocation, _pc) );
    }

    public synchronized void stepOut()
    {
        if( _useServerSideStepper )
            _setServerSideStepper(STEP_OUT);
        else
            _setStepHandler( new StepOut(_threadState, _pc) );
    }

    private synchronized void _setServerSideStepper( int type )
    {
        if( _state != STOPPED || _semaphore.available() )
        {
            if(AS.S)ER.T(false,"_setServerSideStepper called when not really stopped",this);
            return;
        }
        _interrupt = false;
        _stepHandler = null;
        PrivilegeManager.enablePrivilege("Debugger");
        switch(type)
        {
            case STEP_OVER:
                _dc.sendInterruptStepOver(_threadState);
                break;
            case STEP_INTO:
                _dc.sendInterruptStepInto(_threadState);
                break;
            case STEP_OUT :
                _dc.sendInterruptStepOut(_threadState);
                break;
            default:
                if(AS.S)ER.T(false,"invalid type passed to _setServerSideStepper",this);
                return;
        }
        _serverSideStepperIsSet = true;
        _serverSideStepperType = type;

        _continueAndNotify(true);
    }

    private synchronized void _setStepHandler( StepHandler handler )
    {
        if( _state != STOPPED || _semaphore.available() )
        {
            if(AS.S)ER.T(false,"_setStepHandler called when not really stopped",this);
            return;
        }
        _interrupt = false;
        _stepHandler = handler;
        _serverSideStepperIsSet = false;
        PrivilegeManager.enablePrivilege("Debugger");
        _dc.sendInterrupt();
        _continueAndNotify(true);
    }

    public synchronized void disableDebugger()
    {
        if( false == _enabled )
            return;

        _enabled = false;

        if( _state == STOPPED && ! _semaphore.available() )
        {
            _continueAndNotify(false);
        }

        _notifyOfDebuggerDisabled();
    }

    private void _continueAndNotify( boolean notify )
    {
        if( _state != STOPPED || _semaphore.available() )
        {
            if(AS.S)ER.T(false,"_continueAndNotify called when not really stopped",this);
            return;
        }

//        if(AS.DEBUG){System.out.println( "running again" );}

        // transition state
        _state = RUNNING;
        if( notify )
            _notifyOfStateChange();

        synchronized(_semaphore)
        {
            _semaphore.release();
            _threadState.resume();
//            if(AS.DEBUG){System.out.println( "returning after interrupt" );}
        }
    }

    public synchronized void evaluatingBreakpoint(boolean b)
    {
        _evaluatingBreakpoint += (b ? 1 : -1);
        if(AS.S)ER.T(_evaluatingBreakpoint >= 0,"_evaluatingBreakpoint less than zero",this);
    }

    public synchronized void breakpointHookCalledButElectedNotToStop(JSThreadState ts)
    {
        if( _useServerSideStepper )
        {
            if(_serverSideStepperIsSet )
                _dc.reinstateStepper(ts);
        }
        else
        {
            if( null != _stepHandler )
            {
                PrivilegeManager.enablePrivilege("Debugger");
                _dc.sendInterrupt();
            }
        }
    }

    // implement our hooks

    // called by both breakpoints and interrupt hook (on JS thread)
    void aboutToExecute(JSThreadState debug, JSPC pc, Hook hook)
    {
        if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "1)  entered" );
        if( ! _enabled )
        {
            if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "2)  exit, ! _enabled" );
            return;
        }

        if( _evaluatingBreakpoint > 0 )
        {
            if(AS.DEBUG)System.out.println("ignoring break while evaluating breakpoint");
            return;
        }

        PrivilegeManager.enablePrivilege("Debugger");

        // grab the semaphore (return if not available)
        if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "3)  about to call _semaphore.grab()" );
        if( ! _semaphore.grab() )
        {
            if(AS.DEBUG)System.out.println( "blowing past nested break at: " + pc );
            if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "4)  exit  _semaphore.grab() failed" );
            return;
        }
        if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "5)  exit  _semaphore.grab() succeeded" );

        if( hook instanceof CtrlDebugBreakHook )
        {
            _stepHandler = null;
            _serverSideStepperIsSet = false;
            _interrupt   = false;
        }

        // Keep going if the interrupt no longer needed.
        if( hook instanceof CtrlInterruptHook &&
            null == _stepHandler && ! _interrupt && ! _serverSideStepperIsSet )
        {
            _semaphore.release();
            return;
        }

        // save arguments

        _threadState = debug;
        _pc = pc;
        if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "6)  about to call _pc.getSourceLocation()");
        _sourceLocation = (JSSourceLocation) _pc.getSourceLocation();
        if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "7)  _pc.getSourceLocation() returned");

        if(false)
        {
            String leadin = "interrupted at:";
            String url = _sourceLocation.getURL()+"#"+(_sourceLocation.getLine());
            String fun = (null != _pc.getScript().getFunction()) ? (_pc.getScript().getFunction()+"()") : "toplevel";
            String script = "["+(_pc.getScript().getBaseLineNumber())+","+
                            (_pc.getScript().getBaseLineNumber()+_pc.getScript().getLineExtent()-1)+"]";
            String where = "pc: " + _pc.getPC();

            if(AS.DEBUG){System.out.println(leadin+" "+url+" "+fun+" "+script+" "+where);}
//            if(AS.DEBUG)Thread.dumpStack();
        }

        // Hitting any other type of hook clears the interrupt stepper
        if( ! (hook instanceof CtrlInterruptHook) )
        {
            _stepHandler = null;
            _serverSideStepperIsSet = false;
        }

        // If there is a step handler then let it process the hook
        if( null != _stepHandler )
        {
            switch( _stepHandler.step(_threadState,_pc,_sourceLocation,hook) )
            {
                case StepHandler.STOP:
                    _stepHandler = null;
                    if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "8)  StepHandler.STOP");
                    break;
                case StepHandler.CONTINUE_SEND_INTERRUPT:
                    if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "9)  StepHandler.CONTINUE_SEND_INTERRUPT about to sendInterrupt");
                    _dc.sendInterrupt();
                    if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "10)  sendInterrupt returned");
                    _semaphore.release();
                    if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "10.5)  _semaphore.release() returned");
                    return;
                case StepHandler.CONTINUE_DONE:
                    if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "11)  StepHandler.CONTINUE_DONE");
                    _stepHandler = null;
                    _semaphore.release();
                    return;
            }
        }

        // if(AS.DEBUG){System.out.println( "about to send HIT_EXEC_HOOK" );}
        // if(AS.DEBUG){System.out.println( "this = " + this );}
        // if(AS.DEBUG){System.out.println( "HIT_EXEC_HOOK = " + HIT_EXEC_HOOK );}
        // if(AS.DEBUG){System.out.println( "hook = " + hook );}
        // if(AS.DEBUG){System.out.println( "Thread = " + Thread.currentThread() );}

        if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "12)  about to call _threadState.leaveSuspended()");
        _threadState.leaveSuspended();
        if(AS.DEBUG)Log.trace("ControlTyrant.aboutToExecute", "13)  _threadState.leaveSuspended() returned");

        // post message to UI thread
        _app.performCommandLater( this, HIT_EXEC_HOOK, hook );
    }

    // This where we receive commands from the CommandPoster
    // implement target interface
    public synchronized void performCommand(String cmd, Object data)
    {
        if( ! _enabled )
        {
            _state = STOPPED;
            _continueAndNotify(false);
            return;
        }
        if( cmd.equals(HIT_EXEC_HOOK) )
        {
            SourceTyrant st = _emperor.getSourceTyrant();
            if( null == st ||
                null == _sourceLocation ||
                null == _sourceLocation.getURL() ||
                null == st.findSourceItem(_sourceLocation.getURL()) )
            {
                if(AS.DEBUG){System.out.println( "continuing: source text unavailable");}
                _state = STOPPED;
                _continueAndNotify(false);
                return;
            }

            _emperor.setWaitCursor(true);
            _interrupt = false;

            // if(AS.DEBUG){System.out.println( "got stop command, transitioning state" );}

            _emperor.bringAppToFront();

            // transition to STOPPED state
            _state = STOPPED;
            _notifyOfStateChange();
            _emperor.setWaitCursor(false);
        }
        else if( cmd.equals(HIT_ERROR_REPORTER) )
        {
            _emperor.bringAppToFront();
            Sound.soundNamed("droplet.au").play();
//            AWTCompatibility.awtToolkit().beep();

            ErrorReporterDialog erd =
                        new ErrorReporterDialog(_emperor, (ErrorReport) data);

            _emperor.enableAppClose(false);
            erd.showModally();
            _emperor.enableAppClose(true);

            _debugBreakResponse = erd.getAnswer();
            notify();
        }
        else
        {
            if(AS.S)ER.T(false,"unhandled command received in perform command: " + cmd,this);
        }

    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {

    }

    /*
    * NOTE: this ErrorReporter may be called on a thread other than
    * the IFC UI thread
    */
    public synchronized JSErrorReporter setErrorReporter(JSErrorReporter er)
    {
        JSErrorReporter old = _errorReporter;
        _errorReporter = er;
        return old;

    }
    public JSErrorReporter getErrorReporter()
    {
        return null != _errorReporter ? _errorReporter : this ;
    }

    // implement JSErrorReporter interface
    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
        if( ! _enabled )
            return JSErrorReporter.PASS_ALONG;

        if( null != _errorReporter && this != _errorReporter )
            return _errorReporter.reportError(msg,filename,lineno,linebuf,tokenOffset);

        synchronized(this)
        {
            ErrorReport er = new ErrorReport( msg, filename, lineno,
                                              linebuf, tokenOffset );

            _debugBreakResponse = -1;
            _app.performCommandLater( this, HIT_ERROR_REPORTER, er );

            while( -1 == _debugBreakResponse && _enabled )
            {
                try
                {
                    wait();
                }
                catch(Exception e)
                {
                    if(AS.DEBUG){System.out.println("threw during wait for command response");}
                    if(AS.DEBUG){System.out.println(e);}
                }
            }
            if( -1 == _debugBreakResponse )
                _debugBreakResponse = JSErrorReporter.PASS_ALONG;

            return _debugBreakResponse;
        }
    }

    private void _notifyOfStateChange()
    {
        if(AS.S)ER.T(Thread.currentThread()==_uiThreadForAssertCheck,"_notifyObservers called on thread other than UI thread",this);
        setChanged();
        notifyObservers( new ControlTyrantUpdate( ControlTyrantUpdate.STATE_CHANGED,_state) );
    }

    private void _notifyOfDebuggerDisabled()
    {
        if(AS.S)ER.T(Thread.currentThread()==_uiThreadForAssertCheck,"_notifyObservers called on thread other than UI thread",this);
        setChanged();
        notifyObservers( new ControlTyrantUpdate( ControlTyrantUpdate.DEBUGGER_DISABLED,_state) );
    }


    // data...
    private Application         _app;
    private int                 _state;
    private boolean             _interrupt;
    private Emperor             _emperor;
    private DebugController     _dc;
    private CtrlSemaphore       _semaphore;
    private JSThreadState       _threadState;
    private JSPC                _pc;
    private JSSourceLocation    _sourceLocation;

    private StepHandler         _stepHandler;

    private CtrlInterruptHook   _interruptHook;
    private CtrlDebugBreakHook  _debugBreakHook;
    private int                 _debugBreakResponse;

    private boolean             _enabled   = true;
    private Thread              _uiThreadForAssertCheck = null;
    private JSErrorReporter     _errorReporter;

    private boolean             _useServerSideStepper   = false;
    private boolean             _serverSideStepperIsSet = false;
    private int                 _serverSideStepperType;

    private int                 _evaluatingBreakpoint = 0;

    private final String HIT_EXEC_HOOK      = "HIT_EXEC_HOOK";
    private final String HIT_ERROR_REPORTER = "HIT_ERROR_REPORTER";

}

/***************************************************************************/
// used internally only...
class CtrlSemaphore
{
    public boolean              available() {return _available;}
    public synchronized void    release()    {_available = true;}
    public synchronized boolean grab()
    {
        if(! _available)
            return false;
        _available = false;
        return true;
    }

    private boolean _available = true;
}

// used internally only...
class CtrlInterruptHook
    extends InterruptHook
    implements ChainableHook
{
    CtrlInterruptHook( ControlTyrant ctrlTyrant )
    {
        setTyrant( ctrlTyrant );
    }

    public void aboutToExecute(ThreadStateBase debug, PC pc)
    {
//        System.out.println( "called the right hook..." );

        // for safety we make sure not to throw anything at native caller.
        try {
            if( null != _ctrlTyrant )
            {
                if(AS.S)ER.T(debug instanceof JSThreadState,"wrong kind of threadstate",this);
                if(AS.S)ER.T(pc instanceof JSPC,"wrong kind of pc",this);
                _ctrlTyrant.aboutToExecute( (JSThreadState)debug, (JSPC) pc, this );
            }
            if( null != _nextHook )
                _nextHook.aboutToExecute(debug, pc);
        } catch(Throwable t){t.printStackTrace();} // eat it.
    }

    // implement ChainableHook
    public void setTyrant(Object tyrant) {_ctrlTyrant = (ControlTyrant) tyrant;}
    public void setNextHook(Hook nextHook) {_nextHook = (InterruptHook) nextHook;}

    private ControlTyrant _ctrlTyrant;
    private InterruptHook _nextHook;

}

// used internally only...
class CtrlDebugBreakHook
    extends DebugBreakHook
    implements ChainableHook
{
    CtrlDebugBreakHook( ControlTyrant ctrlTyrant )
    {
        setTyrant( ctrlTyrant );
    }

    public void aboutToExecute(ThreadStateBase debug, PC pc)
    {
        // for safety we make sure not to throw anything at native caller.
        try {
            if( null != _ctrlTyrant )
            {
                if(AS.S)ER.T(debug instanceof JSThreadState,"wrong kind of threadstate",this);
                if(AS.S)ER.T(pc instanceof JSPC,"wrong kind of pc",this);
                _ctrlTyrant.aboutToExecute( (JSThreadState)debug, (JSPC) pc, this );
            }
            if( null != _nextHook )
                _nextHook.aboutToExecute(debug, pc);
        } catch(Throwable t){} // eat it.
    }

    // implement ChainableHook
    public void setTyrant(Object tyrant) {_ctrlTyrant = (ControlTyrant) tyrant;}
    public void setNextHook(Hook nextHook) {_nextHook = (DebugBreakHook) nextHook;}

    private ControlTyrant _ctrlTyrant;
    private DebugBreakHook _nextHook;
}

