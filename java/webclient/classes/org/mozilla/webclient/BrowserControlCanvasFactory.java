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

import java.io.File;
import java.io.FileNotFoundException;

/**
 *
 *  <B>BrowserControlCanvasFactory</B> creates concrete instances of BrowserControlCanvas

 * <B>Lifetime And Scope</B> <P>

 * This is a static class, it is neven instantiated.

 *
 * @version $Id: BrowserControlCanvasFactory.java,v 1.4 1999/10/08 00:48:01 edburns%acm.org Exp $
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

    private static boolean appDataHasBeenSet = false;
    private static Class browserControlCanvasClass = null;
    private static String platformCanvasClassName = null;

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

    /**

     * This method is used to set per-application instance data, such as
     * the location of the browser binary.

     * @param absolutePathToNativeBrowserBinDir the path to the bin dir
     * of the native browser, including the bin.  ie:
     * "D:\Projects\mozilla\dist\win32_d.obj\bin"

     */

public static void setAppData(String absolutePathToNativeBrowserBinDir) throws FileNotFoundException, ClassNotFoundException
{
    if (!appDataHasBeenSet) {
        ParameterCheck.nonNull(absolutePathToNativeBrowserBinDir);
        
        // verify that the directory exists:
        File binDir = new File(absolutePathToNativeBrowserBinDir);
        if (!binDir.exists()) {
            throw new FileNotFoundException("Directory " + absolutePathToNativeBrowserBinDir + " is not found.");
        }

        // cause the native library to be loaded
        // PENDING(edburns): do some magic to determine the right kind of
        // MozWebShellCanvas to instantiate
        
        // How about this:
        // I try loading sun.awt.windows.WDrawingSurfaceInfo. If it doesn't
        // load, then I try loading sun.awt.motif.MDrawingSufaceInfo. If
        // none loads, then I return a error message.
        // If you think up of a better way, let me know.
        // -- Mark
        
        Class win32DrawingSurfaceInfoClass = 
            Class.forName("sun.awt.windows.WDrawingSurfaceInfo");
        
        if (win32DrawingSurfaceInfoClass != null) {
            platformCanvasClassName = "org.mozilla.webclient.win32.Win32BrowserControlCanvas";
        }
        
        if (null == platformCanvasClassName) {
            Class motifDrawingSurfaceInfoClass = 
                Class.forName("sun.awt.motif.MDrawingSurfaceInfo");
            
            if (motifDrawingSurfaceInfoClass != null) {
                platformCanvasClassName = "org.mozilla.webclient.motif.MotifBrowserControlCanvas";
            }
        }
        if (platformCanvasClassName != null) {
            browserControlCanvasClass = Class.forName(platformCanvasClassName);
        }
        else {
            throw new ClassNotFoundException("Could not determine WebShellCanvas class to load\n");
        }
        
        BrowserControlCanvas.initialize(absolutePathToNativeBrowserBinDir);
        appDataHasBeenSet = true;
    }
}

public static BrowserControlCanvas newBrowserControlCanvas() throws InstantiationException, IllegalAccessException, IllegalStateException
{
    if (!appDataHasBeenSet) {
        throw new IllegalStateException("Can't create BrowserControlCanvasInstance: setAppData() has not been called.");
    }
    Assert.assert(null != browserControlCanvasClass);
    
    BrowserControlCanvas result = null;
    result = (BrowserControlCanvas) browserControlCanvasClass.newInstance();
    
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
    Log.setApplicationVersionDate("$Id: BrowserControlCanvasFactory.java,v 1.4 1999/10/08 00:48:01 edburns%acm.org Exp $");

    BrowserControlCanvas canvas = null;
    try {
        BrowserControlCanvasFactory.setAppData(args[0]);
        canvas = BrowserControlCanvasFactory.newBrowserControlCanvas();
    }
    catch (Exception e) {
        System.out.println(e.getMessage());
    }

	Assert.assert(null != canvas);
}

// ----UNIT_TEST_END

} // end of class BrowserControl!CanvasFactory
