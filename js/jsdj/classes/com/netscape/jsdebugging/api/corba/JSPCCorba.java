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
