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
import org.mozilla.javascript.debug.util.*;

class StackFrame
    implements IStackFrame
{
    StackFrame(NativeCall frame, ThreadState state)
    {
        if(AS.S)ER.T(null!=frame,"null frame",this);
        if(AS.S)ER.T(null!=state,"null state",this);
        _frame = frame;
        _state = state;
    }

    // implements IStackFrame
    public boolean isValid() {return _state.isValid();}
    public IStackFrame getCaller()
    {
        if(null == _caller)
        {
            NativeCall caller = _frame.getCaller();
            if(null != caller)
                _caller = new StackFrame(caller, _state);
        }
        return _caller;
    }
    public IPC getPC()
    {
        if(null == _pc)
            _pc = new PC(_frame, _state);
        return _pc;
    }

    public ThreadState getThreadState() {return _state;}
    public NativeCall getWrappedFrame() {return _frame;}

    public String toString()
    {
        return "StackFrame for "+getPC();
    }

    private NativeCall _frame;
    private ThreadState _state;
    private StackFrame _caller;
    private PC _pc;
}    
