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

/**
 * This interface supports hooking script loading and unloading.
 * <p>
 * This interface can be implemented and used to detect script
 * loading and unloading. Script unloading is a 'lazy' activity
 * and does not necessarily happen immediately after the script becomes
 * 'not in use'. Also, There is no guarantee that the script object will
 * not become active again. If the same underlying script becomes 
 * active again after the call to aboutToUnloadScript, then a new call to
 * justLoadedScript will be made. It is guaranteed that aboutToUnloadScript
 * will not be called for a script if any code has begun running in that 
 * since the DebugManager was initialized for the given context.
 * 
 * @see org.mozilla.javascript.debug.IDebugManager#setScriptHook
 * @author John Bandhauer
 */

public interface IScriptHook
{
    /**
    * Script was loaded (or re-loaded) and code may begin running through it.
    *
    * @param script the script
    */
    public void justLoadedScript(IScript script);

    /**
    * Script about to be unloaded and no code will be running through it.
    *
    * @param script the script
    */
    public void aboutToUnloadScript(IScript script);
}    
