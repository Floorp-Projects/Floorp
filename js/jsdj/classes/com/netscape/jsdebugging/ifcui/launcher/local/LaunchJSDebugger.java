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

package com.netscape.jsdebugging.ifcui.launcher.local;

import com.netscape.jsdebugging.ifcui.JSDebuggerApp;

/**
* Launches the JavaScript Debugger in a new thread.
* <p>
* This exists so that the debugger applet can be run in a standalone
* Java VM (as a Java Application) with no special modification.
*
* @author  John Bandhauer
*/
public class LaunchJSDebugger implements Runnable
{
    public static void main(String[] args)
    {
        System.out.println("Launching JSDebugger...");
        new Thread(new LaunchJSDebugger()).start();
    }

    public void run()
    {
        JSDebuggerApp app = new JSDebuggerApp();
        app.run();
    }
}