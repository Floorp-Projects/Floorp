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

package org.mozilla.webclient.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.WrapperFactory;

public class WrapperFactoryImpl extends WrapperFactory
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

boolean initialized = false;

// Relationship Instance Variables

//
// Constructors and Initializers    
//

public WrapperFactoryImpl()
{
    super();
}


//
// Class methods
//

//
// Methods from webclient.WrapperFactory
//

/**

 * @param interfaceName IMPORTANT!!!! This method assumes that
 * interfaceName is one of the actual strings from the interface
 * definition for BrowserControl.  That is, this method is only called
 * from BrowserControlImpl, and is only invoked like this

<CODE><PRE>

// BrowserControlImpl code...

    if (WINDOW_CONTROL_NAME.equals(interfaceName)) {
        if (null == windowControl) {
            windowControl = 
                (WindowControl) wrapperFactory.newImpl(WINDOW_CONTROL_NAME,
                                                       this);
        }
        return windowControl;
    }

</PRE></CODE>

 * <P>
 * This is done to avoid a costly string compare.  This shortcut is
 * justified since WrapperFactoryImpl is only called from
 * BrowserControlImpl <B>and</B> the only values for interfaceName that
 * make sense are those defined in BrowserControl class variables ending
 * with _NAME.
 * </P>

 * @see org.mozilla.webclient.BrowserControl.BROWSER_CONTROL_CANVAS_NAME

 */

public Object newImpl(String interfaceName,
                      BrowserControl browserControl) throws ClassNotFoundException
{
    throwExceptionIfNotInitialized();

    Object result = null;

    synchronized(this) {
        if (!nativeDoesImplement(interfaceName)) {
            throw new ClassNotFoundException("Can't instantiate " + 
                                             interfaceName + 
                                             ": not implemented.");
        }
        System.out.println("native library does implement " +
                           interfaceName);
        if (BrowserControl.WINDOW_CONTROL_NAME == interfaceName) {
            result = new WindowControlImpl(this, browserControl);
            return result;
        }
        if (BrowserControl.NAVIGATION_NAME == interfaceName) {
            result = new NavigationImpl(this, browserControl);
            return result;
        }
        if (BrowserControl.HISTORY_NAME == interfaceName) {
            result = new HistoryImpl(this, browserControl);
            return result;
        }
        if (BrowserControl.CURRENT_PAGE_NAME == interfaceName) {
            result = new CurrentPageImpl(this, browserControl);
            return result;
        }
        if (BrowserControl.EVENT_REGISTRATION_NAME == interfaceName) {
            result = new EventRegistrationImpl(this, browserControl);
            return result;
        }
        if (BrowserControl.BOOKMARKS_NAME == interfaceName) {
            result = new BookmarksImpl(this, browserControl);
            return result;
        }
    }

    return result;
}

public void initialize(String verifiedBinDirAbsolutePath) throws Exception
{
    synchronized(this) {
        if (!hasBeenInitialized()) {
            nativeAppInitialize(verifiedBinDirAbsolutePath);
            initialized = true;
        }
    }
}

public void terminate() throws Exception
{
    throwExceptionIfNotInitialized();
    
    synchronized(this) {
        nativeTerminate();
        initialized = false;
    }
}

public boolean hasBeenInitialized()
{
    return initialized;
}

// 
// Native methods
//

/**

 * This is called only once, at the very beginning of program execution.
 * This method allows the native code to handle ONE TIME initialization
 * tasks.  Per-window initialization tasks are handled in the
 * WindowControlImpl native methods.  <P>

 * For mozilla, this means initializing XPCOM.

 */

private native void nativeAppInitialize (String verifiedBinDirAbsolutePath) throws Exception;

/**

 * Called only once, at program termination.  Results are undefined if
 * the number of browser windows currently open is greater than zero.

 */

private native void nativeTerminate () throws Exception;

/**

 * This is called whenever java calls newImpl on an instance of this
 * class.  This method allows the native code to declare which of the
 * "webclient.*" interfaces it supports.  See the comments for the
 * newImpl method for the format of valid values for interfaceName.

 * @see org.mozilla.webclient.wrapper_native.WrapperFactoryImpl.newImpl

 */

private native boolean nativeDoesImplement(String interfaceName);


// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    WrapperFactory me = new WrapperFactoryImpl();
    Log.setApplicationName("WrapperFactoryImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: WrapperFactoryImpl.java,v 1.3 2000/11/02 23:33:13 edburns%acm.org Exp $");

    
}

// ----VERTIGO_TEST_END

} // end of class WrapperFactoryImpl
