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

public class ScriptLocal implements Script
{
    ScriptLocal(netscape.jsdebug.Script script)
    {
        _script = script;
        _sections = new ScriptSectionLocal[1];
        _sections[0] = new ScriptSectionLocal(_script.getBaseLineNumber(),
                                              _script.getLineExtent());
    }

    public String   getURL()               {return _script.getURL();}
    public String   getFunction()          {return _script.getFunction();}
    public int      getBaseLineNumber()    {return _script.getBaseLineNumber();}
    public int      getLineExtent()        {return _script.getLineExtent();}
    public boolean  isValid()              {return _script.isValid();}
    public JSPC     getClosestPC(int line)
    {
        return new JSPCLocal(_script.getClosestPC(line), this);
    }

    public ScriptSection[] getSections()   {return _sections;}

    public String   toString()             {return _script.toString();}
    public int      hashCode()             {return _script.hashCode();}
    public boolean  equals(Object obj)
    {
        try
        {
            return ((ScriptLocal)obj)._script == _script;
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }

    public netscape.jsdebug.Script getRealScript() {return _script;}

    private ScriptSectionLocal[]    _sections;
    private netscape.jsdebug.Script _script;
}    
