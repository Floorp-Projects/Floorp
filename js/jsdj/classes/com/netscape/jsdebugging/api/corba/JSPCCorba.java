/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package com.netscape.jsdebugging.api.corba;

import com.netscape.jsdebugging.api.*;

/**
 * This subinterface of PC provides JavaScript-specific information.
 */
public class JSPCCorba implements JSPC
{
    public JSPCCorba(DebugControllerCorba controller,
                     com.netscape.jsdebugging.remote.corba.IJSPC pc)
    {
        this( controller, pc, new ScriptCorba(controller, pc.script) );
    }

    public JSPCCorba(DebugControllerCorba controller,
                     com.netscape.jsdebugging.remote.corba.IJSPC pc, 
                     ScriptCorba script)
    {
        _controller = controller;
        _pc = pc;
        _script = script;
        _sourceLocation = null;

        
    }

    public Script getScript()                   {return _script;}
    public int getPC()                          {return _pc.offset;}
    // XXX currently no way to track invalidity!!!
    public boolean isValid()                    {return true;}
    public SourceLocation getSourceLocation()
    {
        if( null == _sourceLocation )
        {
            try
            {
                _sourceLocation = new JSSourceLocationCorba(this, 
                     _controller.getRemoteController().getSourceLocation(_pc));
            }
            catch (Exception e)
            {
                // eat it;
                e.printStackTrace();
                System.err.println("error in JSPCCorba.getSourceLocation()");
            }
        }
        return _sourceLocation;
    }

    public boolean equals(Object obj)
    {
        try
        {
            JSPCCorba other = (JSPCCorba) obj;
            return  other._script.equals(_script) &&
                    other._pc.offset == _pc.offset;
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }
    public int hashCode()       {return _script.hashCode()+_pc.offset*7;}
    public String toString()    {return _pc.toString();}

    public com.netscape.jsdebugging.remote.corba.IJSPC getWrappedJSPC() {return _pc;}

    private DebugControllerCorba    _controller;
    private com.netscape.jsdebugging.remote.corba.IJSPC   _pc;
    private ScriptCorba             _script;
    private JSSourceLocationCorba   _sourceLocation;
}
