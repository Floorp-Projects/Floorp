/* -*- Mode: C; tab-width: 8 -*-
 * Copyright © 1996, 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

import netscape.jsdebugger.JSDebuggerApp;

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