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

package com.netscape.jsdebugging.ifcui.launcher.rhino;

import com.netscape.javascript.SourceTextManager;
import com.netscape.javascript.debug.IDebugManager;
import com.netscape.javascript.debug.ILaunchableDebugger;
import com.netscape.jsdebugging.ifcui.JSDebuggerApp;
import com.netscape.jsdebugging.api.rhino.DebugControllerRhino;
import com.netscape.jsdebugging.api.rhino.SourceTextProviderRhino;

public class LaunchNetscapeJavaScriptDebugger
    implements ILaunchableDebugger, Runnable
{
    // implements LaunchableDebugger
    public Thread launch(IDebugManager dbm, SourceTextManager stm, 
                         boolean daemon)
    {
        DebugControllerRhino.setGlobalDebugManager(dbm);
        SourceTextProviderRhino.setGlobalSourceTextManager(stm);

        LaunchNetscapeJavaScriptDebugger self = 
                            new LaunchNetscapeJavaScriptDebugger();
        Thread t = new Thread(self);
        if(daemon)
            t.setDaemon(true);
        t.start();
        return t;
    }

    // implements Runnable
    public void run()
    {
        JSDebuggerApp app = new JSDebuggerApp();
        app.setMode("RHINO");
        app.run();
    }
}