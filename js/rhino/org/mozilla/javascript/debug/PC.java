/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
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
