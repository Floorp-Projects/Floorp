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

package com.netscape.jsdebugging.api.rhino;

import com.netscape.jsdebugging.api.*;

/**
 * This subinterface of PC provides JavaScript-specific information.
 */
public class JSPCRhino implements JSPC
{
    public JSPCRhino(com.netscape.javascript.debug.IPC pc)
    {
        this( pc, new ScriptRhino(pc.getScript()) );
    }

    public JSPCRhino(com.netscape.javascript.debug.IPC pc, ScriptRhino script)
    {
        _pc = pc;
        _script = script;
        _sourceLocation = 
            new JSSourceLocationRhino(this, 
                    (com.netscape.javascript.debug.ISourceLocation)_pc.getSourceLocation());
    }

    public Script getScript()                   {return _script;}
    public int getPC()                          {return _pc.getPC();}
    public boolean isValid()                    {return _pc.isValid();}
    public SourceLocation getSourceLocation()   {return  _sourceLocation;}

    public boolean equals(Object obj)
    {
        try
        {
            JSPCRhino other = (JSPCRhino) obj;
            return  other._pc.getScript() == _pc.getScript() &&
                    other._pc.getPC() == _pc.getPC();
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }
    public int hashCode()                       {return _pc.hashCode();}
    public String toString()                    {return _pc.toString();}

    public com.netscape.javascript.debug.IPC getWrappedJSPC() {return _pc;}

    private  com.netscape.javascript.debug.IPC   _pc;
    private  ScriptRhino             _script;
    private  JSSourceLocationRhino   _sourceLocation;
}
