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
