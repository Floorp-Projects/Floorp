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

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.impl.WrapperFactory;

import org.mozilla.webclient.UnimplementedException;

import java.awt.Rectangle;

public class WindowControlImpl extends ImplObjectNative implements WindowControl
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

public WindowControlImpl(WrapperFactory yourFactory, 
			 BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
}

//
// Class methods
//

//
// General Methods
//


//
// Package Methods
//

//
// Methods from WindowControl    
//

public void setBounds(Rectangle newBounds)
{
    ParameterCheck.nonNull(newBounds);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    
    synchronized(getBrowserControl()) {
        nativeSetBounds(getNativeBrowserControl(), newBounds.x, newBounds.y,
                        newBounds.width, newBounds.height);
    }
}

public void createWindow(int nativeWindow, Rectangle bounds)
{
    ParameterCheck.greaterThan(nativeWindow, 0);
    ParameterCheck.nonNull(bounds);
    getWrapperFactory().verifyInitialized();
}

public int getNativeWebShell()
{
    getWrapperFactory().verifyInitialized();

    return getNativeBrowserControl();
}

public void moveWindowTo(int x, int y)
{
    getWrapperFactory().verifyInitialized();
    
    synchronized(getBrowserControl()) {
        nativeMoveWindowTo(getNativeBrowserControl(), x, y);
    }
}

public void removeFocus()
{
    getWrapperFactory().verifyInitialized();
    
    throw new UnimplementedException("\nUnimplementedException -----\n API Function WindowControl::removeFocus has not yet been implemented.\n");

}
    
public void repaint(boolean forceRepaint)
{
    getWrapperFactory().verifyInitialized();
    
    synchronized(getBrowserControl()) {
        nativeRepaint(getNativeBrowserControl(), forceRepaint);
    }
}

public void setVisible(boolean newState)
{
    getWrapperFactory().verifyInitialized();
    
    synchronized(getBrowserControl()) {
        nativeSetVisible(getNativeBrowserControl(), newState);
    }
}

public void setFocus()
{
    getWrapperFactory().verifyInitialized();

    throw new UnimplementedException("\nUnimplementedException -----\n API Function WindowControl::setFocus has not yet been implemented.\n");
}


// 
// Native methods
//

public native void nativeSetBounds(int webShellPtr, int x, int y, 
                                   int w, int h);

public native void nativeMoveWindowTo(int webShellPtr, int x, int y);

public native void nativeRemoveFocus(int webShellPtr);

public native void nativeRepaint(int webShellPtr, boolean forceRepaint);

public native void nativeSetVisible(int webShellPtr, boolean newState);

public native void nativeSetFocus(int webShellPtr);


// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("WindowControlImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: WindowControlImpl.java,v 1.3 2004/04/10 21:50:38 edburns%acm.org Exp $");

    try {
        org.mozilla.webclient.BrowserControlFactory.setAppData(args[0]);
	org.mozilla.webclient.BrowserControl control = 
	    org.mozilla.webclient.BrowserControlFactory.newBrowserControl();
        Assert.assert_it(control != null);
	
	WindowControl wc = (WindowControl)
	    control.queryInterface(org.mozilla.webclient.BrowserControl.WINDOW_CONTROL_NAME);
	Assert.assert_it(wc != null);
    }
    catch (Exception e) {
	System.out.println("got exception: " + e.getMessage());
    }
}

// ----VERTIGO_TEST_END

} // end of class WindowControlImpl
