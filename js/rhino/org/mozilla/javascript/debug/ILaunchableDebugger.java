/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
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
