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
//import netscape.palomar.util.ER;

public class ScriptRhino implements Script
{
    ScriptRhino(com.netscape.javascript.debug.IScript script)
    {
//        if(ASS)ER.T(null != script,"script was null",this);
        _script = script;
        _sections = new ScriptSectionRhino[1];
        _sections[0] = new ScriptSectionRhino(_script.getBaseLineNumber(),
                                              _script.getLineExtent());
    }

    public String   getURL()               {return _script.getURL();}
    public String   getFunction()          {return _script.getFunction();}
    public int      getBaseLineNumber()    {return _script.getBaseLineNumber();}
    public int      getLineExtent()        {return _script.getLineExtent();}
    public boolean  isValid()              {return _script.isValid();}
    public JSPC     getClosestPC(int line)
    {
        com.netscape.javascript.debug.IPC pc = _script.getClosestPC(line);
        if(null == pc)
            return null;
        return new JSPCRhino(pc, this);
    }

    public ScriptSection[] getSections()   {return _sections;}

    public String   toString()             {return _script.toString();}
    public int      hashCode()             {return _script.hashCode();}
    public boolean  equals(Object obj)
    {
        try
        {
            return ((ScriptRhino)obj)._script == _script;
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }

    public com.netscape.javascript.debug.IScript getRealScript() {return _script;}

    private ScriptSectionRhino[] _sections;
    private com.netscape.javascript.debug.IScript _script;
//    private static final boolean ASS = true; // enable ASSERT support?
}    
