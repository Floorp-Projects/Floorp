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
