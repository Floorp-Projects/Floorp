/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

/**

 * Abstract base class for all java Objects that are peers of XPCOM
 * objects that need automatic reference counting.

 */

public abstract class ISupportsPeer extends Object
{
//
// Constants
//

//
// Class Variables
//

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

//
// Constructors and Initializers    
//

public ISupportsPeer()
{
}

//
// Class methods
//

//
// General Methods
//

//
// Native methods
//

protected native void nativeAddRef(int nativeNode);
protected native void nativeRelease(int nativeNode);


// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("ISupportsPeer");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: ISupportsPeer.java,v 1.1 2003/09/28 06:29:06 edburns%acm.org Exp $");

}

// ----VERTIGO_TEST_END

} // end of class ISupportsPeer
