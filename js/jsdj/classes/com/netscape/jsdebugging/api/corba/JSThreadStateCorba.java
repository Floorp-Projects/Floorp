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

package com.netscape.jsdebugging.api.corba;

import com.netscape.jsdebugging.api.*;

public class JSThreadStateCorba implements JSThreadState
{
    public JSThreadStateCorba(DebugControllerCorba controller,
                              com.netscape.jsdebugging.remote.corba.IJSThreadState ts)
    {
        _controller = controller;
        _ts = ts;
        if( null != _ts.stack && 0 != _ts.stack.length )
        {
            _stack = new JSStackFrameInfoCorba[_ts.stack.length];
            for( int i = 0; i < _ts.stack.length; i++ )
                _stack[i] = new JSStackFrameInfoCorba(_controller, _ts.stack[i], this);
        }
    }
    
    // XXX currently no way to track invalidity!!!
    public boolean isValid()        {return true;}
    public int getStatus()          {return _ts.status;}

    public int countStackFrames()
        throws InvalidInfoException
        {
            if( null == _stack )
                throw new InvalidInfoException();
            return _stack.length;
        }

    public StackFrameInfo getCurrentFrame()
        throws InvalidInfoException
        {
            if( null == _stack )
                throw new InvalidInfoException();
            return _stack[_stack.length-1];
        }

    /**
     * Get the thread's stack as an array.  stack[stack.length-1] is the
     * current frame, and stack[0] is the beginning of the stack.
     */
    public StackFrameInfo[] getStack()
        throws InvalidInfoException
        {
            if( null == _stack )
                throw new InvalidInfoException();
            return _stack;
        }

    /**
     * Return true if the thread is currently running a hook
     * for this ThreadState
     */
    public boolean isRunningHook()
    {
        try
        {
            return _controller.getRemoteController().isRunningHook(_ts.id);
        }
        catch (Exception e)
        {
            // eat it;
            e.printStackTrace();
            System.err.println("error in JSThreadStateCorba.isRunningHook");
            return false;
        }
    }

    /**
     * Return true if the hook on this thread has already completed
     * and we are waiting for a call to resume()
     */
    public boolean isWaitingForResume()
    {
        try
        {
            return _controller.getRemoteController().isWaitingForResume(_ts.id);
        }
        catch (Exception e)
        {
            // eat it;
            e.printStackTrace();
            System.err.println("error in JSThreadStateCorba.isWaitingForResume");
            return false;
        }
    }

    /**
     * Leave the thread in a suspended state when the hook method(s)
     * finish.  This can be undone by calling resume().
     */
    public synchronized void leaveSuspended()
    {
        try
        {
            _controller.getRemoteController().leaveThreadSuspended(_ts.id);
        }
        catch (Exception e)
        {
            // eat it;
            e.printStackTrace();
            System.err.println("error in JSThreadStateCorba.isWaitingForResume");
        }
    }

    /**
    * Resume the thread. 
    * Two cases:
    *   1) Hook is still running, this will undo the leaveSuspended 
    *      and the thread will still be waiting for the hook to 
    *      return.
    *   2) Hook has already returned and we are in an event loop 
    *      waiting to contine the suspended thread, this will
    *      force completion of events pending on the suspended 
    *      thread and resume it.
    */
    public synchronized void resume()
    {
        try
        {
            _controller.getRemoteController().resumeThread(_ts.id);
        }
        catch (Exception e)
        {
            // eat it;
            e.printStackTrace();
            System.err.println("error in JSThreadStateCorba.resume");
        }
    }

    /**
     * This gets the current continue state of the debug frame, which
     * will be one of the DEBUG_STATE_* values above.
     */
    public int getContinueState()   {return _ts.continueState;}

    public synchronized int setContinueState(int state)
    {
        int old = _ts.continueState;
        try
        {
            _controller.getRemoteController().setThreadContinueState(_ts.id, state);
            _ts.continueState = state;
        }
        catch (Exception e)
        {
            // eat it;
            e.printStackTrace();
            System.err.println("error in JSThreadStateCorba.setContinueState");
        }
        return old;
    }

    public com.netscape.jsdebugging.remote.corba.IJSThreadState getWrappedThreadState() {return _ts;}

    private DebugControllerCorba                _controller;
    private com.netscape.jsdebugging.remote.corba.IJSThreadState _ts;
    private JSStackFrameInfoCorba[]          _stack;
}
