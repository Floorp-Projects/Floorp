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
 * John Bandhauer
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

package org.mozilla.javascript;

/**
 * This interface supports low-level execute debug hooks.
 * <p>
 * This interface can be implemented and used to hook JavaScript 
 * script execution. org.mozilla.javascript.debug.IDebugManager uses this 
 * system and provides a higher level abstraction more appropriate 
 * as a debugger API.
 *
 * @see org.mozilla.javascript.Context#setExecuteHook
 * @see org.mozilla.javascript.debug.IDebugManager
 * @author John Bandhauer
 */

public interface DeepExecuteHook
{
    /**
     * Notification that a top-level script is about to be executed.
     *
     * @param cx current context
     * @param fun script
     * @param scope scope
     * @return arbitrary object which is subsequently passed to postExecute
     * @see org.mozilla.javascript.debug.NativeDelegate#executeDebug
     */
    public Object preExecute(Context cx, NativeFunction fun, Scriptable scope);

    /**
     * Notification that a top-level script execution returned.
     *
     * @param cx current context
     * @param ref value returned from previous call to preExecute
     * @param retVal value returned by the script (null if exception thrown)
     * @param e exception thrown by script (null if none thrown)
     * @see org.mozilla.javascript.debug.NativeDelegate#executeDebug
     */
    public void postExecute(Context cx, Object ref, Object retVal, 
                            JavaScriptException e);
}
