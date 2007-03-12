/* 
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
import org.mozilla.util.ReturnRunnable;

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

public void setBounds(Rectangle rect)
{
    ParameterCheck.nonNull(rect);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    final Rectangle newBounds = rect;
    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		nativeSetBounds(getNativeBrowserControl(), 
				newBounds.x, newBounds.y,
				newBounds.width, newBounds.height);
		return null;
	    }
            public String toString() {
                return "WCRunnable.nativeSetBounds";
            }

	});
}

public void createWindow(int nativeWindow, Rectangle rect)
{
    ParameterCheck.greaterThan(nativeWindow, 0);
    ParameterCheck.nonNull(rect);
    getWrapperFactory().verifyInitialized();
    final int nativeWin = nativeWindow;
    final int nativeBc = getNativeBrowserControl();
    final BrowserControl bc = getBrowserControl();
    final int finalX = rect.x;
    final int finalY = rect.y;
    final int finalWidth = rect.width;
    final int finalHeight = rect.height;

    NativeEventThread.instance.pushBlockingReturnRunnable(new NativeRealizeWCRunnable(nativeWin,
            nativeBc, rect, bc));
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
    final boolean finalBool = newState;
    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		nativeSetVisible(getNativeBrowserControl(), finalBool);
		return null;
	    }
            public String toString() {
                return "WCRunnable.nativeSetVisible";
            }

	});
}

public void setFocus()
{
    getWrapperFactory().verifyInitialized();

    throw new UnimplementedException("\nUnimplementedException -----\n API Function WindowControl::setFocus has not yet been implemented.\n");
}

public class NativeRealizeWCRunnable extends ReturnRunnable {
    
    final private int nativeWin;
    final private int nativeBc;
    final private Rectangle rect;
    final private BrowserControl bc;
    
    NativeRealizeWCRunnable(int nativeWin, int nativeBc, Rectangle rect, 
            BrowserControl bc) {
        this.nativeWin = nativeWin;
        this.nativeBc = nativeBc;
        this.rect = rect;
        this.bc = bc;
    }
    
    public Object run() {

        WindowControlImpl.this.nativeRealize(this.nativeWin, this.nativeBc, 
                this.rect.x, this.rect.y, this.rect.width, this.rect.height, 
                this.bc);
        return null;
    }
    public String toString() {
        return "WCRunnable.nativeRealize";
    }

}


// 
// Native methods
//

public native void nativeRealize(int nativeWindow, 
				 int nativeBrowserControl,
				 int x, int y, int width, int height, 
				 BrowserControl myBrowserControlImpl);

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
    Log.setApplicationVersionDate("$Id: WindowControlImpl.java,v 1.8 2007/03/12 20:39:22 edburns%acm.org Exp $");

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
