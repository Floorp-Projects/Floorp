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
 * @version $Id: BrowserControlCanvasFactory.java,v 1.2 1999/08/10 03:59:03 mark.lin%eng.sun.com Exp $
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
    Class browserControlCanvasClass = null;
    String className = null;
    
    BrowserControlCanvas result = null;
    // PENDING(edburns): do some magic to determine the right kind of
    // MozWebShellCanvas to instantiate

    // How about this:
    // I try loading sun.awt.windows.WDrawingSurfaceInfo. If it doesn't
    // load, then I try loading sun.awt.motif.MDrawingSufaceInfo. If
    // none loads, then I return a error message.
    // If you think up of a better way, let me know.
    // -- Mark

    try {
        Class win32DrawingSurfaceInfoClass = 
            Class.forName("sun.awt.windows.WDrawingSurfaceInfo");
        
        if (win32DrawingSurfaceInfoClass != null) {
            className = "org.mozilla.webclient.win32.Win32MozWebShellCanvas";
        }
    } catch (Exception e) {
        className = null;
    }

    try {
        Class motifDrawingSurfaceInfoClass = 
            Class.forName("sun.awt.motif.MDrawingSurfaceInfo");
        
        if (motifDrawingSurfaceInfoClass != null) {
            className = "org.mozilla.webclient.motif.MotifBrowserControlCanvas";
        }
    } catch (Exception e) {
        className = null;
    }

    if (className != null) {
        try {
            if (null != (browserControlCanvasClass = Class.forName(className))) {
                result = (BrowserControlCanvas) browserControlCanvasClass.newInstance();
            }
        } catch (Exception e) {
            System.out.println("Got Exception: " + e.getMessage());
            e.printStackTrace();
        }
    } else {
        System.out.println("Could not determine WebShellCanvas class to load\n");
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
    System.out.println("doing asserts");
    Assert.setEnabled(true);
    Log.setApplicationName("BrowserControlCanvasFactory");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: BrowserControlCanvasFactory.java,v 1.2 1999/08/10 03:59:03 mark.lin%eng.sun.com Exp $");

	BrowserControlCanvas canvas = BrowserControlCanvasFactory.newBrowserControlCanvas();

	Assert.assert(null != canvas);
}

// ----UNIT_TEST_END

} // end of class BrowserControl!CanvasFactory
