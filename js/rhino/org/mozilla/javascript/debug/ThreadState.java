/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;

public class ThreadState
    implements IThreadState
{
    ThreadState(Context cx, ScriptManager scriptManager)
    {
        _cx = cx;
        _scriptManager = scriptManager;
    }

    // implements IThreadState
    public int countStackFrames() throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        if(null == _stack)
            _buildStack();
        return _stack.length;
    }

    public IStackFrame getCurrentFrame() throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        if(null == _topFrame)
            _buildTopFrame();
        return _topFrame;
    }

    public IStackFrame[] getStack() throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        if(null == _stack)
            _buildStack();
        return _stack;
    }

    public boolean isValid() {return null != _cx;}

    public int getContinueState() throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        return _continueState;
    }
    public int setContinueState(int state) throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        int oldState = _continueState;
        _continueState = state;
        return oldState;
    }

    /********************************************************/
    /********************************************************/
    // XXX THUNKED OUT...
//    public Thread getThread()                                   {return null;}
//    public boolean isRunningHook()                              {return false;}
//    public int getStatus()                                      {return 0;}
//    public void leaveSuspended()                                {return ;}
//    public void resume()                                        {return ;}
    /********************************************************/
    /********************************************************/

    public Object getReturnValue() throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        return _returnValue;
    }

    public Object setReturnValue(Object o) throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        Object result = _returnValue;
        _returnValue = o;
        return result;
    }

    public Throwable getException() throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        return _exception;
    }

    public Throwable setException(Throwable t) throws IllegalStateException
    {
        if(!isValid())
            throw new IllegalStateException("ThreadState not Valid");
        Throwable result = _exception;
        _exception = t;
        return result;
    }

    /********************************************************/
    /********************************************************/


    public void invalidate() {_cx = null;}
    public Context getContext() {return _cx;}
    public ScriptManager getScriptManager() {return _scriptManager;}

    /********************************************************/

    public String toString()
    {
        return "ThreadState with "+countStackFrames()+ " frames(s)";
    }


    public void dump()
    {
        int count = countStackFrames();
        System.out.println(toString());
        for(int i = 0; i< count; i++)
            System.out.println("    "+getStack()[i]);
    }


    private void _buildTopFrame()
    {
        Context cx = _cx;
        if( null != cx )
            _topFrame = new StackFrame(
                            ScriptRuntime.getCurrentActivation(cx), this);
    }

    private void _buildStack()
    {
        if(null == _topFrame)
            _buildTopFrame();
        
        int count;
        IStackFrame cur;

        // get count
        for(count = 0, cur = _topFrame; cur != null; count++)
            cur = cur.getCaller();

        _stack = new IStackFrame[count];

        int i;
        for(i = count-1, cur = _topFrame; i >= 0; i--, cur = cur.getCaller())
            _stack[i] = cur;
    }

    private Context _cx;
    private ScriptManager _scriptManager;
    private IStackFrame _topFrame;
    private IStackFrame[] _stack;
    private int _continueState = DEBUG_STATE_RUN;
    private Throwable _exception;
    private Object _returnValue;
}    
