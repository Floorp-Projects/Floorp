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
