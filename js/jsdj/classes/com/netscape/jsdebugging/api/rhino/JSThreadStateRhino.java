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

package com.netscape.jsdebugging.api.rhino;

import com.netscape.jsdebugging.api.*;

public class JSThreadStateRhino implements JSThreadState
{
    public JSThreadStateRhino( com.netscape.javascript.debug.IThreadState ts )
    {
        _ts = ts;
//        try
//        {
            com.netscape.javascript.debug.IStackFrame[] stack = _ts.getStack();

            if( null != stack && 0 != stack.length )
            {
                _stack = new JSStackFrameInfoRhino[stack.length];
                for( int i = 0; i < stack.length; i++ )
                    _stack[i] = new JSStackFrameInfoRhino(
                                    (com.netscape.javascript.debug.IStackFrame)stack[i], 
                                    this);
            }
//        }
//        catch( com.netscape.javascript.debug.InvalidInfoException e )
//        {
//            _stack = null;
//        }
    }
    
    public boolean isValid()        {return null != _stack && _ts.isValid();}
    public int getStatus()          {return THR_STATUS_UNKNOWN;}

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
        return _runningHook;
    }

    /**
     * Return true if the hook on this thread has already completed
     * and we are waiting for a call to resume()
     */
    public boolean isWaitingForResume()
    {
        return _runningEventQueue;
    }


    /**
     * Leave the thread in a suspended state when the hook method(s)
     * finish.  This can be undone by calling resume().
     */
    public synchronized void leaveSuspended()
    {
        if( _runningHook && ! _runningEventQueue )
            _leaveSuspended = true;
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
        if( ! _runningHook )
            return;

        _leaveSuspended = false;

        // signal stopped thread
        if( _runningEventQueue )
            notifyAll();
    }

    /**
     * This gets the current continue state of the debug frame, which
     * will be one of the DEBUG_STATE_* values above.
     */
    public int getContinueState()   {return _ts.getContinueState();}

    public int setContinueState(int state)
    {
        // XXX this is a hack to deal with the fact that the current debugger
        // uses 'DEBUG_STATE_THROW' to mean 'abort' - i.e. return null.
        // So, we remap it.
        switch(state)
        {
            default:
            case DEBUG_STATE_DEAD:
            case DEBUG_STATE_RUN:
            case DEBUG_STATE_RETURN:
                break;
            case DEBUG_STATE_THROW:
                state = DEBUG_STATE_RETURN;
                _ts.setReturnValue(null);
                break;
        }
        return _ts.setContinueState(state);
    }

    public synchronized void aboutToCallHook()
    {
        _runningHook = true;
        
    }
    public synchronized void returnedFromCallHook()
    {
        if(_leaveSuspended)
        {
            _runningEventQueue = true;
            while( _leaveSuspended )
            {
                try 
                {
                    wait();
                }
                catch(Exception e)
                {
                    e.printStackTrace(); 
                    System.out.println(e);
                    _leaveSuspended = false;
                }
                processAllEvents();
            }
            _runningEventQueue = false;
        }
        _runningHook = false;
    }

    public synchronized boolean addCallbackEvent(Callback cb)
    {
        if( _runningEventQueue )
        {
            addEvent(cb);
            notify();
            return true;
        }
        return false;
    }

    private synchronized void addEvent(Callback cb)
    {
        Event e = new Event();
        e.cb = cb;
        if( null != _lastEvent )
            _lastEvent.next = e;
        _lastEvent = e;
        if( null == _firstEvent )
            _firstEvent = e;
    }

    private synchronized Event getEvent()
    {
        Event e = _firstEvent;
        if( null != e )
        {
            _firstEvent = e.next;
            if( _lastEvent == e )
                _lastEvent = null;
            e.next = null;
        }
        return e;
    }

    private synchronized void processAllEvents()
    {
        Event e;
        while( null != (e = getEvent()) )
        {
            if( null != e.cb )
                e.cb.call();
        }
    }

    private com.netscape.javascript.debug.IThreadState _ts;
    private JSStackFrameInfoRhino[]          _stack;
    private boolean                          _runningHook = false;
    private boolean                          _leaveSuspended = false;
    private boolean                          _runningEventQueue = false;
    private Event                            _firstEvent = null;
    private Event                            _lastEvent = null;
}


class Event
{
    public Event next = null;
    public Callback cb = null;
}    