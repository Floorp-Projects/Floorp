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
                System.out.println("execption thrown " + t );
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
