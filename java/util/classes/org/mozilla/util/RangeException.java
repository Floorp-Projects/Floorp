/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The original code is "The Lighthouse Foundation Classes (LFC)"
 * 
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1997, 1998, 1999 Sun
 * Microsystems, Inc. All Rights Reserved.
 */


package org.mozilla.util;

import java.lang.RuntimeException;

/**
 * An RangeException is an exception that is thrown when a value
 * is outside of its acceptable range.
 *
 * @version  $Id: RangeException.java,v 1.1 1999/07/30 01:02:58 edburns%acm.org Exp $
 * @author   The LFC Team (Rob Davis, Paul Kim, Alan Chung, Ray Ryan, etc)
 * @author   David-John Burrowes (he moved it to the AU package)
 */
public class RangeException extends RuntimeException {

    //
    // STATIC VARIABLES
    //
 
    /**
     * The RCSID for this class.
     */
    private static final String RCSID =
        "$Id: RangeException.java,v 1.1 1999/07/30 01:02:58 edburns%acm.org Exp $";

    /**
     * Constructs an exception with no error string.
     */
    public RangeException() {
        super();
    }

    /**
     * Constructs an exception with an error string.
     *
     * @param message A message that will hopefully tell someone about the cause
     *                of this exception.
     */
    public RangeException(String message) {
        super(message);
    }
}
