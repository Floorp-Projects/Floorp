/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
