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
