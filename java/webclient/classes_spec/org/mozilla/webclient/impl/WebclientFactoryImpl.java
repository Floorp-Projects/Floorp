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

// WebclientFactoryImpl.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;
import org.mozilla.util.Utilities;

import java.io.File;
import java.io.FileNotFoundException;

import org.mozilla.webclient.WebclientFactory;
import org.mozilla.webclient.BrowserControl;

import org.mozilla.webclient.BrowserControlCanvas; // PENDING(edburns): remove this dependency

public class WebclientFactoryImpl extends Object implements WebclientFactory
{
//
// Protected Constants
//

//
// Class Variables
//

private Class browserControlCanvasClass = null;
private String platformCanvasClassName = null;

    /**
     * <p>Owns the responsibility of loading the native language
     * libraries, if necessary.</p>
     *
     * <p>Knows how to create instances of the webclient API
     * interfaces.</p>
     */

protected WrapperFactory wrapperFactory = null;

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

//
// Constructors and Initializers    
//

public WebclientFactoryImpl()
{
}

//
// Public methods
//

    
public void setAppData(String absolutePathToNativeBrowserBinDir) throws FileNotFoundException, ClassNotFoundException
{
    try {
        getWrapperFactory().initialize(absolutePathToNativeBrowserBinDir);
    }
    catch (SecurityException se) {
        throw new ClassNotFoundException(se.getMessage(), se);
    }
    catch (UnsatisfiedLinkError ule) {
        throw new ClassNotFoundException(ule.getMessage(), ule);
    }
}
        /******
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
            BrowserControlImpl.appInitialize(absolutePathToNativeBrowserBinDir);
        }
        catch (Exception e) {
            throw new ClassNotFoundException("Can't initialize native browser: " + 
                                             e.getMessage());
        }
        ********************/

public void appTerminate() throws Exception
{
    getWrapperFactory().terminate();
}

public BrowserControl newBrowserControl() throws InstantiationException, IllegalAccessException, IllegalStateException
{
    getWrapperFactory().verifyInitialized();
    return getWrapperFactory().newBrowserControl();
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
    ParameterCheck.nonNull(toDelete);
    getWrapperFactory().deleteBrowserControl(toDelete);
    ((BrowserControlImpl)toDelete).delete();
}

//
// Helper Methods
//

WrapperFactory getWrapperFactory()
{
    if (null != wrapperFactory) {
        return wrapperFactory;
    }

    wrapperFactory = (WrapperFactory)
        Utilities.
        getImplFromServices("org.mozilla.webclient.impl.WrapperFactory");

    return wrapperFactory;
}

} // end of class WebclientFactoryImpl
