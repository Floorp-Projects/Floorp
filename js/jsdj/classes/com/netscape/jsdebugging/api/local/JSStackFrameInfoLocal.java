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

package com.netscape.jsdebugging.api.local;

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
public class JSStackFrameInfoLocal implements JSStackFrameInfo
{
    public JSStackFrameInfoLocal( netscape.jsdebug.JSStackFrameInfo info,
                                  JSThreadStateLocal                ts )
    {
        _info = info;
        _ts = ts;
        try
        {
            _pc = new JSPCLocal((netscape.jsdebug.JSPC)_info.getPC());
        }
        catch( netscape.jsdebug.InvalidInfoException e )
        {
            _pc = null;
        }

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
        if( ! _info.isValid() )
            throw new InvalidInfoException();
        try    
        {
            return new ValueLocal(_info.getCallObject());
        }
        catch(netscape.jsdebug.InvalidInfoException e)
        {
            throw new InvalidInfoException();
        }
    }

    public Value getScopeChain() throws InvalidInfoException
    {
        if( ! _info.isValid() )
            throw new InvalidInfoException();
        try    
        {
            return new ValueLocal(_info.getScopeChain());
        }
        catch(netscape.jsdebug.InvalidInfoException e)
        {
            throw new InvalidInfoException();
        }
    }

    public Value getThis() throws InvalidInfoException
    {
        if( ! _info.isValid() )
            throw new InvalidInfoException();
        try    
        {
            return new ValueLocal(_info.getThis());
        }
        catch(netscape.jsdebug.InvalidInfoException e)
        {
            throw new InvalidInfoException();
        }
    }

    public netscape.jsdebug.JSStackFrameInfo getWrappedInfo() {return _info;}

    private netscape.jsdebug.JSStackFrameInfo _info;    
    private JSThreadStateLocal                _ts;
    private JSPCLocal                         _pc;
}

