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
* The PC class is an opaque representation of a program counter.  It
* may have different implementations for interpreted Java methods,
* methods compiled by the JIT, JavaScript methods, etc.
*
* @author  John Bandhauer
* @author  Nick Thompson
* @version 1.0
* @since   1.0
*/
public abstract class PC {
    /**
     * Get the SourceLocation associated with this PC value.
     * returns null if the source location is unavailable.
     */
    public abstract SourceLocation getSourceLocation();

    /**
     * Ask whether two program counter values are equal.
     */
    public abstract boolean equals(Object obj);
}
