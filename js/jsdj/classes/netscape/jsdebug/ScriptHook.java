/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.jsdebug;

/**
* ScriptHook must be subclassed to respond to loading and 
* unloading of scripts
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
*/
public class ScriptHook extends Hook 
{
    /**
     * Create a ScriptHook for current the current VM.
     */
    public ScriptHook() {}

    /**
     * Override this method to respond when a new script is
     * loaded into the VM.
     *
     * @param script a script object created by the controller to 
     * represent this script. This exact same object will be used
     * in all further references to the script.
     */
    public void justLoadedScript(Script script) {
        // defaults to no action
    }

    /**
     * Override this method to respond when a class is
     * about to be unloaded from the VM.
     *
     * @param script which will be unloaded
     */
    public void aboutToUnloadScript(Script script) {
        // defaults to no action
    }
}
