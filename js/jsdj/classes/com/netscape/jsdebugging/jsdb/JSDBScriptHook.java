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

/* JavaScript command line Debugger
 * @author Alex Rakhlin
 */

package com.netscape.jsdebugging.jsdb;

import com.netscape.jsdebugging.api.*;
import java.util.*;

class JSDBScriptHook
    extends ScriptHook
{
    public JSDBScriptHook(DebugController controller)
    {
        _scripts = new Vector();
        _controller = controller;
    }

    public void justLoadedScript(Script script)
    {
       //System.out.println("LOADED SCRIPT "+script.getURL()+" "+script.getFunction());
       _scripts.addElement (script);
       ((JSDBInterruptHook)_controller.getInterruptHook()).getBreakpoints().setHooksForLoadedScript(script);
    }

    public void aboutToUnloadScript(Script script)
    {
        //System.out.println("UNLOADED SCRIPT "+script.getURL()+" "+script.getFunction());
        _scripts.removeElement (script);
        ((JSDBInterruptHook)_controller.getInterruptHook()).getBreakpoints().clearHooksForScript(script);
    }

    public Vector getScripts () {return _scripts;}

    private Vector _scripts;
    private DebugController _controller;

}


