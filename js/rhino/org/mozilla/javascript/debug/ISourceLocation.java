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
 * This interface represents a source location of a point in the code.
 * <p>
 * This interface is implemented by the debug system. Consumers of the debug
 * system should never need to create their own ISourceLocation objects. The 
 * debug system would certainly not recognize such objects.
 *
 * @author John Bandhauer
 * @see org.mozilla.javascript.debug.IPC
 */

public interface ISourceLocation
{
    /**
    * Get the source line number for this point in the code.
    * <p>
    * (immutable while underlying script is valid)
    * @return the line number
    */
    public int getLine();

    /**
    * Get the URL or filename from which the script was compiled.
    * <p>
    * (immutable while underlying script is valid)
    * @return the name
    */
    public String getURL();

    /**
    * Get the IPC object that this source location is associated with.
    * <p>
    * (immutable while underlying script is valid)
    * @return the pc object
    * @see org.mozilla.javascript.debug.IPC
    */
    public IPC getPC();
}    
