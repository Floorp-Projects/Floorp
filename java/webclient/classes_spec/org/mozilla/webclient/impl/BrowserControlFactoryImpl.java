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
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 */

package org.mozilla.webclient.impl;

// BrowserControlFactoryImpl.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.io.File;
import java.io.FileNotFoundException;

import org.mozilla.webclient.BrowserControlFactoryInterface;
import org.mozilla.webclient.BrowserControl;

import org.mozilla.webclient.BrowserControlImpl; // PENDING(edburns): remove this dependency
import org.mozilla.webclient.BrowserControlCanvas; // PENDING(edburns): remove this dependency

public class BrowserControlFactoryImpl extends Object implements BrowserControlFactoryInterface
{
//
// Protected Constants
//

//
// Class Variables
//

    private boolean appDataHasBeenSet = false;
    private Class browserControlCanvasClass = null;
    private String platformCanvasClassName = null;
    private String browserType = null;

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

//
// Constructors and Initializers    
//

public BrowserControlFactoryImpl()
{
}

//
// Class methods
//

public void setAppData(String absolutePathToNativeBrowserBinDir) throws FileNotFoundException, ClassNotFoundException
{
    this.setAppData(BrowserControl.BROWSER_TYPE_NATIVE, 
                    absolutePathToNativeBrowserBinDir);
}


    /**

     * This method is used to set per-application instance data, such as
     * the location of the browser binary.

     * @param myBrowserType.  Either "native" or "nonnative"

     * @param absolutePathToNativeBrowserBinDir the path to the bin dir
     * of the native browser, including the bin.  ie:
     * "D:\Projects\mozilla\dist\win32_d.obj\bin"

     */

public void setAppData(String myBrowserType, String absolutePathToNativeBrowserBinDir) throws FileNotFoundException, ClassNotFoundException
{
    browserType = myBrowserType;
    if (!appDataHasBeenSet) {
        // figure out the correct value for platformCanvasClassName
        if (browserType.equals(BrowserControl.BROWSER_TYPE_NON_NATIVE)) {
            platformCanvasClassName = "org.mozilla.webclient.wrapper_nonnative.JavaBrowserControlCanvas";
        }
        else {
            ParameterCheck.nonNull(absolutePathToNativeBrowserBinDir);
            
            // verify that the directory exists:
            File binDir = new File(absolutePathToNativeBrowserBinDir);
            if (!binDir.exists()) {
                throw new FileNotFoundException("Directory " + absolutePathToNativeBrowserBinDir + " is not found.");
            }
            
            // This hack is necessary for Sun Bug #4303996
            java.awt.Canvas c = new java.awt.Canvas();
            platformCanvasClassName = determinePlatformCanvasClassName();
        }
        // end of figuring out the correct value for platformCanvasClassName
        if (platformCanvasClassName != null) {
            browserControlCanvasClass = Class.forName(platformCanvasClassName);
        }
        else {
            throw new ClassNotFoundException("Could not determine BrowserControlCanvas class to load\n");
        }
        
        try {
            BrowserControlImpl.appInitialize(browserType, absolutePathToNativeBrowserBinDir);
        }
        catch (Exception e) {
            throw new ClassNotFoundException("Can't initialize native browser: " + 
                                             e.getMessage());
        }
        appDataHasBeenSet = true;
    }
}

public void appTerminate() throws Exception
{
    BrowserControlImpl.appTerminate();
}

public BrowserControl newBrowserControl() throws InstantiationException, IllegalAccessException, IllegalStateException
{
    if (!appDataHasBeenSet) {
        throw new IllegalStateException("Can't create BrowserControl instance: setAppData() has not been called.");
    }
    Assert.assert_it(null != browserControlCanvasClass);
    
    BrowserControlCanvas newCanvas = null;
    BrowserControl result = null; 
    newCanvas = (BrowserControlCanvas) browserControlCanvasClass.newInstance();
    if (null != newCanvas &&
        null != (result = new BrowserControlImpl(browserType, newCanvas))) {
        newCanvas.initialize(result);
    }

    return result;
}

/**

 * BrowserControlFactory.deleteBrowserControl is called with a
 * BrowserControl instance obtained from
 * BrowserControlFactory.newBrowserControl.  This method renders the
 * argument instance completely un-usable.  It should be called when the
 * BrowserControl instance is no longer needed.  This method simply
 * calls through to the non-public BrowserControlImpl.delete() method.

 * @see org.mozilla.webclient.ImplObject#delete

 */

public void deleteBrowserControl(BrowserControl toDelete)
{
    Assert.assert_it(false);
    //ParameterCheck.nonNull(toDelete);
    //((BrowserControlImpl)toDelete).delete();
}

//
// General Methods
//

/**

 * Called from setAppData() in the native case.  This method simply
 * figures out the proper name for the class that is the
 * BrowserControlCanvas. 

 * @return  "org.mozilla.webclient.wrapper_native.win32.Win32BrowserControlCanvas" or "org.mozilla.webclient.wrapper_native.gtk.GtkBrowserControlCanvas"

 */

private String determinePlatformCanvasClassName()
{
    String result = null;
    // cause the native library to be loaded
    // PENDING(edburns): do some magic to determine the right kind of
    // MozWebShellCanvas to instantiate
    
    // How about this:
    // I try loading sun.awt.windows.WDrawingSurfaceInfo. If it doesn't
    // load, then I try loading sun.awt.motif.MDrawingSufaceInfo. If
    // none loads, then I return a error message.
    // If you think up of a better way, let me know.
    // -- Mark
    // Here is what I think is a better way: query the os.name property.
    // This works in JDK1.4, as well.
    // -- edburns

    String osName = System.getProperty("os.name");
    
    if (null != osName) {
       if (-1 != osName.indexOf("indows")) {
           result = "org.mozilla.webclient.wrapper_native.win32.Win32BrowserControlCanvas";
       }
       else {
           result = "org.mozilla.webclient.wrapper_native.gtk.GtkBrowserControlCanvas";
       }
    }
    
    return result;
}

} // end of class BrowserControlFactoryImpl
