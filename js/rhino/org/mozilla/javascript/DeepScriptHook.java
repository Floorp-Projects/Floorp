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
 * This interface supports low-level script load/unload debug hooks.
 * <p>
 * This interface can be implemented and used to hook script loading 
 * and unloading. org.mozilla.javascript.debug.IDebugManager uses this 
 * system and provides a higher level abstraction more appropriate 
 * as a debugger API.
 *
 * @see org.mozilla.javascript.Context#setScriptHook
 * @see org.mozilla.javascript.debug.IDebugManager
 * @author John Bandhauer
 */

public interface DeepScriptHook
{
    /**
     * Notification that a script is being loaded.
     *
     * @param cx current context
     * @param obj script or function being loaded
     */
    public void loadingScript(Context cx, NativeFunction obj);

    /**
     * Notification that a script is being unloaded.
     * <p>
     * NOTE: this currently happens as part of the Java garbage 
     * collection process; i.e. it is called by the finalize method
     * of the script and is thus likely to be called on a system 
     * thread. Also, this is thus not likely to be called if there are 
     * any outstanding references to the script.
     *
     * @param cx context which originally loaded the script
     * @param obj script or function being unloaded
     */
    public void unloadingScript(Context cx, NativeFunction obj);
}
