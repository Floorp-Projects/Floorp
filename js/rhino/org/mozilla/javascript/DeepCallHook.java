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
 * May 6, 1999.
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
 * This interface supports low-level call debug hooks
 * <p>
 * This interface can be implemented and used to hook JavaScript 
 * function calls. org.mozilla.javascript.debug.IDebugManager uses this 
 * system and provides a higher level abstraction more appropriate 
 * as a debugger API.
 *
 * @see org.mozilla.javascript.Context#setCallHook
 * @see org.mozilla.javascript.debug.IDebugManager
 * @author John Bandhauer
 */

public interface DeepCallHook
{
    /**
     * Notification that a function is about to be called.
     *
     * @param cx current context
     * @param fun function to be called
     * @param thisArg the 'this' of the object
     * @param args constructor arguments array
     * @return arbitrary object which is subsequently passed to postCall
     * @see org.mozilla.javascript.debug.NativeDelegate#callDebug
     */
    public Object preCall(Context cx, Object fun, Object thisArg, Object[] args);

    /**
     * Notification that a call returned.
     *
     * @param cx current context
     * @param ref value returned from previous call to preCall
     * @param retVal value returned by the callee (null if exception thrown)
     * @param e exception thrown by callee (null if none thrown)
     * @see org.mozilla.javascript.debug.NativeDelegate#callDebug
     */
    public void postCall(Context cx, Object ref, Object retVal, 
                         JavaScriptException e);
}
