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
* Hook (with conditional support) for breakpoints
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import netscape.security.PrivilegeManager;
import com.netscape.jsdebugging.api.*;

class BreakpointHook
    extends InstructionHook
    implements ChainableHook, JSErrorReporter
{
    public BreakpointHook(Emperor emperor, PC pc, Object tyrant, Breakpoint bp)
    {
        super(pc);
        _emperor = emperor;
        setTyrant(tyrant);
        setBreakpoint(bp);
    }

    public void aboutToExecute(ThreadStateBase debug)
    {
        // for safety we make sure not to throw anything at native caller.
        try {
            _ctrlTyrant.evaluatingBreakpoint(true);
            boolean stop = _shouldStop((JSThreadState)debug);
            _ctrlTyrant.evaluatingBreakpoint(false);

            if( stop )
                _ctrlTyrant.aboutToExecute( (JSThreadState)debug, (JSPC)getPC(), this );
            else
                _ctrlTyrant.breakpointHookCalledButElectedNotToStop((JSThreadState)debug);
            if( null != _nextHook )
                _nextHook.aboutToExecute(debug);
        } catch(Throwable t){} // eat it.
    }

    public Breakpoint getBreakpoint()              {return _bp;}
    public void       setBreakpoint(Breakpoint bp) {_bp = bp;}

    // implement ChainableHook
    public void setTyrant(Object tyrant) {_ctrlTyrant = (ControlTyrant) tyrant;}
    public void setNextHook(Hook nextHook) {_nextHook = (InstructionHook) nextHook;}
    public InstructionHook getNextHook() {return _nextHook;}


    private boolean _shouldStop(JSThreadState threadState)
    {
//        System.out.println( "1) in _shouldStop" );
        if( null == _ctrlTyrant || null == _bp )
            return false;

//        System.out.println( "2) in _shouldStop" );
        // currently don't stopping when already stopped
        if( ControlTyrant.STOPPED == _ctrlTyrant.getState() )
            return false;

        String breakCondition = _bp.getBreakCondition();
        if( null == breakCondition )
            return true;

//        System.out.println( "3) in _shouldStop" );
        JSErrorReporter oldER = null;
        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            DebugController dc = _emperor.getDebugController();

            String filename;
            if( _ctrlTyrant.getEmperor().isPre40b6() ||
                Emperor.REMOTE_SERVER == _ctrlTyrant.getEmperor().getHostMode() )
                filename = "HiddenBreakpointEval";
            else
                filename = _bp.getURL();

            JSStackFrameInfo frame = (JSStackFrameInfo) threadState.getCurrentFrame();

            _errorString = null;
            if( ! _emperor.isCorbaHostConnection() )
            {
                oldER = _ctrlTyrant.setErrorReporter(this);
            }

//            System.out.println( "4) in _shouldStop, condition = " + breakCondition);

            ExecResult fullresult =
                dc.executeScriptInStackFrame(frame,breakCondition,filename,1);

            String result = fullresult.getResult();

            if( ! _emperor.isCorbaHostConnection() )
            {
                _ctrlTyrant.setErrorReporter(oldER);
                oldER = null;
            }

//            System.out.println( "5) in _shouldStop");
            if( null != _errorString )
                return false;

//            System.out.println( "6) in _shouldStop, result = " + result );
            if( null == result || ! result.equals("true") )
                return false;
//            System.out.println( "7) in _shouldStop");
        }
        catch(Exception e)
        {
            // if we hit an expection, then we'll just not stop here...
            if( null != oldER )
                _ctrlTyrant.setErrorReporter(oldER);
//            System.out.println( "8) in _shouldStop");
            return false;
        }
//            System.out.println( "9) in _shouldStop");
        return true;
    }

    // implement JSErrorReporter interface
    public synchronized int reportError( String msg,
                                         String filename,
                                         int    lineno,
                                         String linebuf,
                                         int    tokenOffset )
    {
        _errorString = msg;
        return JSErrorReporter.RETURN;
    }

    private Emperor         _emperor;
    private ControlTyrant   _ctrlTyrant;
    private InstructionHook _nextHook;
    private Breakpoint      _bp;
    private String          _errorString;
}
