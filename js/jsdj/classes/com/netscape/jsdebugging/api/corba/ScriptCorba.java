/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
