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


// CUAssertionFailureException.java
//
// $Id: AssertionFailureException.java,v 1.1 1999/07/30 01:02:57 edburns%acm.org Exp $
//

package org.mozilla.util;

/**
 *  This exception is thrown when an <B>Assert.assert()</B> fails.
 *
 * @see Assert
 */
public class AssertionFailureException
	extends RuntimeException {


public AssertionFailureException () { super(); }
public AssertionFailureException (String s) { super("\n" + s + "\n"); }


}  // End of class AssertionFailureException
