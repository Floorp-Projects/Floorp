/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package org.mozilla.webclient;

// BrowserControlFactory.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;
import org.mozilla.util.Utilities;

import java.io.FileNotFoundException;


/**
 * <p>The factory from which {@link BrowserControl} instances are
 * created.</p>
 *
 * <p><B>BrowserControlFactory</B> uses {@link
 * Utilities#getImplFromServices} to find an implementation of {@link
 * WebclientFactory}.  All of the public static methods in this class
 * simply call through to this implementation instance.</p>
 *
 * @version $Id: BrowserControlFactory.java,v 1.13 2005/03/17 01:56:55 edburns%acm.org Exp $
 * 
 *
 */

public class BrowserControlFactory extends Object {
    //
    // Protected Constants
    //
    
    //
    // Class Variables
    //
    
    private static WebclientFactory instance = null;
    
    //
    // Constructors and Initializers    
    //
    
    private BrowserControlFactory() {
	Assert.assert_it(false, "This class shouldn't be constructed.");
    }
    
    //
    // Class methods
    //

    /**
     * <p>If called before the first {@link BrowserControl} has been
     * created, set the name of the profile to use for this browser
     * session.  If the underlying browser does not support the concept
     * of profiles, this method is a no-op.</p>
     *
     * @param profileName the name of the profile
     */
    
    public static void setProfile(String profileName) {
	getInstance().setProfile(profileName);
    }

    /**
     *
     * <p>Initialize the webclient API passing in the path to the browser
     * binary, if necessary.  This must be the first method called in the
     * Webclient API, and it must be called once and only once.</p>
     * 
     * <p>If we are embedding a native browser, calling this method will
     * cause the native libraries to be loaded into the JVM.</p>
     *
     * @param absolutePathToNativeBrowserBinDir if non-<code>null</code>
     * this must be the path to the bin dir of the native browser, including
     * the bin.  ie: "D:\\Projects\\mozilla\\dist\\win32_d.obj\\bin".  When
     * embedding a non-native browser, this may be null.
     *
     */
    
    public static void setAppData(String absolutePathToNativeBrowserBinDir) throws FileNotFoundException, ClassNotFoundException {
	getInstance().setAppData(absolutePathToNativeBrowserBinDir);
    }

    /**
     *
     * @deprecated Use {@link #setAppData(java.lang.String)} instead.
     */
    
    public static void setAppData(String notUsed, String absolutePathToNativeBrowserBinDir) throws FileNotFoundException, ClassNotFoundException {
	getInstance().setAppData(absolutePathToNativeBrowserBinDir);
    }

    /**
     * <p>This method must be called when the application no longer
     * needs to use the Webclient API for the rest of its lifetime.</p>
     */
    
    public static void appTerminate() throws Exception {
	getInstance().appTerminate();
    }

    /**
     * <p>Create, initialize, and return a new {@link BrowserControl}
     * instance.  There is one <code>BrowserControl</code> instance per
     * browser window.  For example, in a tabbed browser application
     * with three browser tabs open, there will be three
     * <code>BrowserControl</code> instances, one for each tab.  Also,
     * any pop-up windows that are created during the course of a user
     * session also correspond to a <code>BrowserControl</code>
     * instance.</p>
     */

    public static BrowserControl newBrowserControl() throws InstantiationException, IllegalAccessException, IllegalStateException {
	BrowserControl result = null;
	result = getInstance().newBrowserControl();
	return result;
    }

    /**
     * <p>Delete a {@link BrowserControl} instance created with {@link
     * #newBrowserControl}.  This method must be called when the user no
     * longer needs a <code>BrowserControl</code> instance.  For
     * example, when a browser tab closes.  After returning from this
     * call, any extant references to that <code>BrowserControl</code>
     * are completely useless.</p>
     *
     * @param toDelete the <code>BrowserControl</code> instance to
     * delete.
     */
    
    public static void deleteBrowserControl(BrowserControl toDelete) {
	getInstance().deleteBrowserControl(toDelete);
    }
    
    //
    // helper methods
    // 

    /**
     * <p>This method enables separating the webclient interface from
     * its implementation via the {@link Utilities#getImplFromServices}
     * method.</p>
     */
    
    protected static WebclientFactory getInstance() 
    {
	if (null != instance) {
	    return instance;
	}
	
	instance = (WebclientFactory) 
	    Utilities.getImplFromServices("org.mozilla.webclient.WebclientFactory");
	return instance;
    }
    
} // end of class BrowserControlFactory
