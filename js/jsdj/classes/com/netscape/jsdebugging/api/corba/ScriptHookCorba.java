/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
