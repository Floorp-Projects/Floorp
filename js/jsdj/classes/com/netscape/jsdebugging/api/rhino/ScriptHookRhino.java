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

/**
 * ScriptHook must be subinterfaceed to respond to significant events
 * in the virtual machine.
 */
public class ScriptHookRhino implements com.netscape.javascript.debug.IScriptHook
{
    public ScriptHookRhino( ScriptHook hook )
    {
        _hook = hook;
    }

    public void justLoadedScript(com.netscape.javascript.debug.IScript script)
    {
        _hook.justLoadedScript( new ScriptRhino(script) );
    }

    public void aboutToUnloadScript(com.netscape.javascript.debug.IScript script)
    {
        _hook.aboutToUnloadScript( new ScriptRhino(script) );
    }

    public ScriptHook getWrappedHook() {return _hook;}

    private ScriptHook _hook;
}
