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
 * This interface represents a program counter location within a script.
 * <p>
 * This interface is implemented by the debug system. Consumers of the debug
 * system should never need to create their own IPC objects. The 
 * debug system would certainly not recognize such objects.
 *
 * @author John Bandhauer
 * @see org.mozilla.javascript.debug.IScript
 * @see org.mozilla.javascript.debug.ISourceLocation
 */

public interface IPC
{
    /**
    * Get the script to which the pc pertains.
    * <p>
    * (immutable while pc is valid)
    * @return the script
    */
    public IScript getScript();

    /**
    * Get the wrapped numerical program counter value.
    * <p>
    * NOTE: this may not directly map to a memory location and is
    * not applicable to diferent scripts. It should be treated as 
    * an abstract value, but it is guaranteed that no two distinct
    * IPC objects associated with the same IScript will have identical
    * pc values.
    * <p>
    * (immutable while pc is valid)
    * @return the program counter integer
    */
    public int getPC();

    /**
    * Determine if the object is valid.
    * <p>
    * This IPC will be set invalid when the script to which it pertains
    * is set invalid.
    *
    * @return is the Object valid
    * @see org.mozilla.javascript.debug.IScript#isValid
    */
    public boolean isValid();

    /**
    * Get the object that describes the source of thie location.
    * <p>
    * This IPC will be set invalid when the script to which it pertains
    * is set invalid.
    *
    * @return source location object
    * @see org.mozilla.javascript.debug.ISourceLocation
    */
    public ISourceLocation getSourceLocation();
}    
