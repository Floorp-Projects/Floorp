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

/**
 * This interface represents compiled JavaScript scripts and functions.
 * <p>
 * This interface is implemented by the debug system. Consumers of the debug
 * system should never need to create their own IScript objects. The
 * debug system would certainly not recognize such objects.
 *
 * @author John Bandhauer
 */

public interface IScript
{
    /**
    * Get the URL or filename from which this script was compiled.
    * <p>
    * (immutable while script is valid)
    * @return the name
    */
    public String getURL();

    /**
    * Get the function name of the script.
    * <p>
    * (immutable while script is valid)
    * @return the function name if function, else null if top level script
    */
    public String getFunction();

    /**
    * Get the first line number of the source from which the script was compiled.
    * <p>
    * (immutable while script is valid)
    * @return base line number
    */
    public int getBaseLineNumber();

    /**
    * Get the number of lines of source from which the script was compiled.
    * <p>
    * (immutable while script is valid)
    * @return number of source lines
    */
    public int getLineExtent();

    /**
    * Determine if the script is valid.
    * <p>
    * Scripts which correspond to code which has been unloaded are set
    * as invlaid. If a script hook is in place, then the hook's
    * aboutToUnloadScript method will be called before the script is
    * marked as invalid.
    *
    * @return is the script valid
    * @see org.mozilla.javascript.debug.IScriptHook#aboutToUnloadScript
    */
    public boolean isValid();

    /**
    * Get the best place to set a breakpoint for a given source line.
    * <p>
    * Returns the closest program counter location for the given
    * line within the script. Will fail if the script is invalid
    * or if the line is out of range.
    *
    * @return pc cooresponding to the source line
    * @see org.mozilla.javascript.debug.IPC
    * @see org.mozilla.javascript.debug.IDebugManager#setInstructionHook
    */
    public IPC getClosestPC(int line);

    /**
    * Get underlying org.mozilla.javascript.NativeFunction.
    * <p>
    * org.mozilla.javascript.NativeFunction is derived from
    * org.mozilla.javascript.ScriptableObject. So, the returned object can be
    * used to access any information about the underlying script which is
    * accessible from any instance of org.mozilla.javascript.ScriptableObject.
    *
    * @return wrapped NativeFunction
    * @see org.mozilla.javascript.ScriptableObject
    */
    public org.mozilla.javascript.NativeFunction getWrappedFunction();
}
