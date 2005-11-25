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

package com.netscape.jsdebugging.engine.rhino;

import com.netscape.jsdebugging.engine.*;
import com.netscape.javascript.*;
import com.netscape.javascript.debug.*;

import com.netscape.jsdebugging.api.rhino.*;

public class RuntimeRhino implements IRuntime
{
    private RuntimeRhino() {}

    RuntimeRhino(boolean enableDebugging, String[] args) {
        if(enableDebugging) {
            _debug_dm = new DebugManager();
            _debug_stm = new SourceTextManagerImpl();
            _debug_dm.setSourceTextManager(_debug_stm);

            DebugControllerRhino.setGlobalDebugManager(_debug_dm);
            SourceTextProviderRhino.setGlobalSourceTextManager(_debug_stm);
            try
            {
                _controller = DebugControllerRhino.getDebugController();
                if( null == _controller )
                    System.out.println ("FAILED to Init DebugController");
                _stp = SourceTextProviderRhino.getSourceTextProvider();
                if( null == _stp )
                    System.out.println ("FAILED to Init SourceTextProvider");
            }
            catch(Throwable t)
            {
                System.out.println("exception thrown " + t );
            }
        }
    }

    public IContext newContext(String[] args) {
        ContextRhino context = new ContextRhino(this, args);
        if(! context.isValid())
            return null;
        return context;
    }

    public boolean isValid() {return ! _destroyed;}

    // finalization not guaranteed to happen in right order - trust programmer
    public void destroy() {
        _destroyed = true;
        _debug_dm   = null;
        _debug_stm  = null;
        _controller = null;
        _stp        = null;
    }

    public com.netscape.jsdebugging.api.DebugController 
                                getDebugController()    {return _controller;}

    public com.netscape.jsdebugging.api.SourceTextProvider 
                                getSourceTextProvider() {return _stp;}

    DebugManager      getDebugManager()      {return _debug_dm; }
    SourceTextManager getSourceTextManager() {return _debug_stm;}

    private DebugManager      _debug_dm;
    private SourceTextManager _debug_stm;
    private boolean _destroyed = false;
    private com.netscape.jsdebugging.api.DebugController _controller;
    private com.netscape.jsdebugging.api.SourceTextProvider _stp;
}    
