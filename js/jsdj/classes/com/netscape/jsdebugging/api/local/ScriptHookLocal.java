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

package com.netscape.jsdebugging.api.local;

import com.netscape.jsdebugging.api.*;

/**
 * ScriptHook must be subinterfaceed to respond to significant events
 * in the virtual machine.
 */
public class ScriptHookLocal extends netscape.jsdebug.ScriptHook
{
    public ScriptHookLocal( ScriptHook hook )
    {
        _hook = hook;
    }

    public void justLoadedScript(netscape.jsdebug.Script script)
    {
        _hook.justLoadedScript( new ScriptLocal(script) );
    }

    public void aboutToUnloadScript(netscape.jsdebug.Script script)
    {
        _hook.aboutToUnloadScript( new ScriptLocal(script) );
    }

    public ScriptHook getWrappedHook() {return _hook;}

    private ScriptHook _hook;
}
