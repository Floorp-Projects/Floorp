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
 * This interface supports low-level new object debug hooks.
 * <p>
 * This interface can be implemented and used to hook JavaScript 
 * object creation; i.e. calls for JavaScript constructor 
 * functions. org.mozilla.javascript.debug.IDebugManager uses this 
 * system and provides a higher level abstraction more appropriate 
 * as a debugger API.
 *
 * @see org.mozilla.javascript.Context#setNewObjectHook
 * @see org.mozilla.javascript.debug.IDebugManager
 * @author John Bandhauer
 */

public interface DeepNewObjectHook
{
    /**
     * Notification that a constructor is about to be called.
     *
     * @param cx current context
     * @param fun constructor function
     * @param args constructor arguments array
     * @return arbitrary object which is subsequently passed to postNewObject
     * @see org.mozilla.javascript.debug.NativeDelegate#newObjectDebug
     */
    public Object preNewObject(Context cx, Object fun, Object[] args);

    /**
     * Notification that a constructor returned.
     *
     * @param cx current context
     * @param ref value returned from previous call to preNewObject
     * @param retVal value returned by the constructor (null if exception thrown)
     * @param e exception thrown by constructor (null if none thrown)
     * @see org.mozilla.javascript.debug.NativeDelegate#newObjectDebug
     */
    public void postNewObject(Context cx, Object ref, Scriptable retVal, 
                              Exception e);
}
