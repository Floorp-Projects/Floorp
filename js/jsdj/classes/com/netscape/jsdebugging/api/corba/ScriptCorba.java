/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.api.corba;

import com.netscape.jsdebugging.api.*;

public class ScriptCorba implements Script
{
    ScriptCorba(DebugControllerCorba controller,
                com.netscape.jsdebugging.remote.corba.IScript script)
    {
        _controller = controller;
        _script = script;

        int count = _script.sections.length;
        if( 0 != count )
        {
            _sections = new ScriptSectionCorba[count];
            for( int i = 0; i < count; i++ )
                _sections[i] = new ScriptSectionCorba(_script.sections[i].base,
                                                      _script.sections[i].extent);
        }
        else
        {
            // make one big section
            _sections = new ScriptSectionCorba[1];
            _sections[0] = new ScriptSectionCorba(_script.base, _script.extent);
        }
    }

    public String   getURL()               {return _script.url;}
    public String   getFunction()          {return _script.funname;}
    public int      getBaseLineNumber()    {return _script.base;}
    public int      getLineExtent()        {return _script.extent;}

    // XXX right now we have no scheme for tracking destruction of scripts
    public boolean  isValid()              {return true;}
    public JSPC     getClosestPC(int line)
    {
        try
        {
            return new 
                JSPCCorba(_controller,
                          _controller.getRemoteController().getClosestPC(_script, line));
        }
        catch (Exception e)
        {
            // eat it;
            e.printStackTrace();
            System.err.println("error in ScriptCorba.getClosestPC");
            return null;
        }
    }
    public ScriptSection[] getSections()   {return _sections;}

    public String   toString()             {return _script.toString();}
    public int      hashCode()             {return _script.jsdscript;}
    public boolean  equals(Object obj)
    {
        try
        {
            return ((ScriptCorba)obj)._script.jsdscript == _script.jsdscript;
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }

    public com.netscape.jsdebugging.remote.corba.IScript getRealScript() {return _script;}

    private ScriptSectionCorba[]                _sections;
    private DebugControllerCorba                _controller;
    private com.netscape.jsdebugging.remote.corba.IScript   _script;
}    
