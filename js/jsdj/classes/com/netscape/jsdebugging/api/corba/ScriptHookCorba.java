/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.api.corba;

import com.netscape.jsdebugging.api.*;

/**
 * ScriptHook must be subinterfaceed to respond to significant events
 * in the virtual machine.
 */
public class ScriptHookCorba 
    extends com.netscape.jsdebugging.remote.corba._sk_IScriptHook
{
    private static int _objectCounter = 0;

    public ScriptHookCorba(DebugControllerCorba controller,
                           ScriptHook hook)
    {
        super( "ScriptHookCorba_"+_objectCounter++);
        _controller = controller;
        _hook = hook;
    }

    public void justLoadedScript(com.netscape.jsdebugging.remote.corba.IScript script)
    {
        _hook.justLoadedScript( new ScriptCorba(_controller, script) );
    }

    public void aboutToUnloadScript(com.netscape.jsdebugging.remote.corba.IScript script)
    {
        _hook.aboutToUnloadScript( new ScriptCorba(_controller, script) );
    }

    public ScriptHook getWrappedHook() {return _hook;}

    private DebugControllerCorba _controller;
    private ScriptHook _hook;
}
