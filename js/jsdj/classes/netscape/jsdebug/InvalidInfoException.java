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
* Exception to indicate bad thread state etc...
*
* @author  John Bandhauer
* @author  Nick Thompson
* @version 1.0
* @since   1.0
*/
public class InvalidInfoException extends Exception {
    /**
     * Constructs a InvalidInfoException without a detail message.
     * A detail message is a String that describes this particular exception.
     */
    public InvalidInfoException() {
	super();
    }

    /**
     * Constructs a InvalidInfoException with a detail message.
     * A detail message is a String that describes this particular exception.
     * @param s the detail message
     */
    public InvalidInfoException(String s) {
	super(s);
    }
}
