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
