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

class PC
    implements IPC
{
    PC(NativeCall frame, ThreadState threadState)
    {
        _frame = frame;
        _threadState = threadState;
        _pc = -1; // signify 'not yet set'
    }

    PC(Script script, int pc)
    {
        _script = script;
        _pc = pc;
    }

    // implements IPC
    public IScript getScript()
    {
        if(null == _script)
        {
            NativeFunction o = _frame.getFunctionObject();
            _script = _threadState.getScriptManager().
                                    lockScriptForFunction(o);
        }
        return _script;
    }

    public int getPC()
    {
        if(-1 == _pc) 
        {
            _pc = _frame.debugPC;
        }
        return _pc;        
    }

    public boolean isValid()
    {
        if( null != _threadState )
            return _threadState.isValid();
        else if( null != _script )
            return _script.isValid();       
        return false;
    }

    public ISourceLocation getSourceLocation()
    {
        if(null == _sourceLocation)
        {
            int lineno = 0; // XXX is this a good default???
            NativeFunction fun = null;
            Script script = ((Script)getScript());
            if(null != script)
                fun = script.getWrappedFunction();
            if(null != fun && (fun instanceof INativeFunScript))
                lineno = ((INativeFunScript)fun).pc2lineno(getPC());
            _sourceLocation = new SourceLocation(this, lineno);
        }
        return _sourceLocation;
    }

    public void finalize()
    {
        if(null != _script && null != _threadState)
        {
            _threadState.getScriptManager().releaseScript(_script, false);
            _script = null;
        }
    }

    public String toString()
    {
        return "PC for "+getScript()+" line "+getSourceLocation().getLine();
    }

    public int hashCode()
    {
        return getScript().hashCode() * 17 + getPC();
    }

    public boolean equals(Object obj)
    {
        if(!(obj instanceof PC))
            return false;
        PC o = (PC)obj;
        if( (!isValid()) || (!o.isValid()))
            return false;
        return getPC() == o.getPC() && getScript().equals(o.getScript());
    }

    private NativeCall _frame;
    private Script _script;
    private SourceLocation _sourceLocation;
    private ThreadState _threadState;
    private int _pc;
}    
