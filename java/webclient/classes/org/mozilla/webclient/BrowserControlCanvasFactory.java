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
 * 
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1997, 1998, 1999 Sun
 * Microsystems, Inc. All Rights Reserved.
 */

package org.mozilla.webclient;

// BrowserControlCanvasFactory.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

/**
 *
 *  <B>BrowserControlCanvasFactory</B> creates concrete instances of BrowserControlCanvas

 * <B>Lifetime And Scope</B> <P>

 * This is a static class, it is neven instantiated.

 *
 * @version $Id: BrowserControlCanvasFactory.java,v 1.1 1999/07/30 01:03:04 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.test.EmbeddedMozilla

 */

public class BrowserControlCanvasFactory extends Object
{
//
// Protected Constants
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

public BrowserControlCanvasFactory()
{
	Assert.assert(false, "This class shouldn't be constructed.");
}

//
// Class methods
//

public static BrowserControlCanvas newBrowserControlCanvas()
{
	BrowserControlCanvas result = null;

	// PENDING(edburns): do some magic to determine the right kind of
	// BrowserControlCanvas to instantiate

	Class browserControlCanvasClass = null;
	String className = "org.mozilla.webclient.Win32BrowserControlCanvas";

    try {
        if (null != (browserControlCanvasClass = Class.forName(className))) {
			result = (BrowserControlCanvas) browserControlCanvasClass.newInstance();
		}
	}
    catch (Exception e) {
        System.out.println(e.getMessage());
    }
	
	return result;
}

//
// General Methods
//

// ----UNIT_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    Log.setApplicationName("BrowserControlCanvasFactory");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: BrowserControlCanvasFactory.java,v 1.1 1999/07/30 01:03:04 edburns%acm.org Exp $");

	BrowserControlCanvas canvas = BrowserControlCanvasFactory.newBrowserControlCanvas();

	Assert.assert(null != canvas);
}

// ----UNIT_TEST_END

} // end of class BrowserControlCanvasFactory
