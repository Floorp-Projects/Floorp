/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// DEBUG API class

package org.mozilla.javascript.debug;

import org.mozilla.javascript.SourceTextManager;

/**
 * This interface is a generic scheme for launching a debugger UI.
 * <p>
 * This interface can be implemented by a debugger class to allow a given
 * embedding to start up a debugger without having to be compiled against
 * the classes of the debugger. As long as a debugger implements this
 * interface, then a given embedding can launch the debugger by only
 * knowing (at runtime) the name of the debugger class that implements this
 * interface. This supplies a useful level of decoupling.
 * <p>
 * For instance...
 * <pre>
 * try {
 *     Class clazz = Class.forName(
 *         "netscape.jsdebugger.api.rhino.LaunchNetscapeJavaScriptDebugger");
 *     ILaunchableDebugger debugger =
 *         (ILaunchableDebugger) clazz.newInstance();
 *     debugger.launch(debug_dm, debug_stm, false);
 *
 * } catch (Exception e) {
 *     // eat it...
 *     System.out.println(e);
 *     System.out.println("Failed to launch the JSDebugger");
 * }
 * </pre>
 *
 * @author John Bandhauer
 * @see org.mozilla.javascript.debug.IDebugManager
 * @see org.mozilla.javascript.SourceTextManager
 */

public interface ILaunchableDebugger
{
    /**
    * Launch the debugger.
    * <p>
    * The debugger should be started on a new thread which is returned.
    *
    * @param dbm the debug manager
    * @param stm the source text manager
    * @param daemon should the new thread be a daemon or user thread
    * @return new thread which is the main thread of the debugger
    */
    public Thread launch(IDebugManager dbm, SourceTextManager stm,
                         boolean daemon);
}
