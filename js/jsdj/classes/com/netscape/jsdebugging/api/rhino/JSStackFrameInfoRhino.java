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

/**
 * This interface provides access to the execution stack of a thread.
 * It has several subinterfacees to distinguish between different kinds of
 * stack frames: these currently include activations of Java methods
 * or JavaScript functions.
 * It is possible that synchronize blocks and try blocks deserve their own
 * stack frames - to allow for later extensions a debugger should skip over
 * stack frames it doesn't understand.
 * Note that this appears very Java-specific.  However, multiple threads and
 * exceptions are relevant to JavaScript as well because of its
 * interoperation with Java.
 */
public class JSStackFrameInfoRhino implements JSStackFrameInfo
{
    public JSStackFrameInfoRhino( com.netscape.javascript.debug.IStackFrame info,
                                  JSThreadStateRhino                ts )
    {
        _info = info;
        _ts = ts;
//        try
//        {
            _pc = new JSPCRhino((com.netscape.javascript.debug.IPC)_info.getPC());
//        }
//        catch( com.netscape.javascript.debug.InvalidInfoException e )
//        {
//            _pc = null;
//        }

    }

    public boolean isValid()                {return _info.isValid();}
    public ThreadStateBase getThreadState() {return _ts;}
    public PC getPC()  throws InvalidInfoException
    {
        if( null == _pc )
            throw new InvalidInfoException();
        return _pc;
    }

    public Value getCallObject() throws InvalidInfoException
    {
        throw new InvalidInfoException("not implemented");
    }

    public Value getScopeChain() throws InvalidInfoException
    {
        throw new InvalidInfoException("not implemented");
    }

    public Value getThis() throws InvalidInfoException
    {
        throw new InvalidInfoException("not implemented");
    }

    public com.netscape.javascript.debug.IStackFrame getWrappedInfo() {return _info;}

    private com.netscape.javascript.debug.IStackFrame _info;    
    private JSThreadStateRhino                _ts;
    private JSPCRhino                         _pc;
}

