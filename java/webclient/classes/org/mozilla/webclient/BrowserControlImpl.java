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
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ann Sunhachawee
 */

package org.mozilla.webclient;

// BrowserControlImpl.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.awt.Rectangle;
import java.awt.Canvas;

/**
 *
 *  <B>BrowserControlImpl</B> provides the implementation for BrowserControl
 *
 * <B>Lifetime And Scope</B> <P>
 *
 * @version $Id: BrowserControlImpl.java,v 1.9 2000/02/18 19:32:22 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControl
 *
 */

public class BrowserControlImpl extends Object implements BrowserControl, EventRegistration
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

	/** 

	 * a handle to the actual mozilla webShell

	 */

	private int nativeWebShell;
    private Canvas myCanvas;

//
// Constructors and Initializers    
//

protected BrowserControlImpl(Canvas yourCanvas)
{
    ParameterCheck.nonNull(yourCanvas);
    myCanvas = yourCanvas;
}

//
// Class methods
//

//
// General Methods
//

// 
// Methods from BrowserControl
// 

public void createWindow(int windowPtr, Rectangle bounds) throws Exception 
{
	nativeWebShell = BrowserControlNativeShim.webShellCreate(windowPtr, bounds, this);
}

public Canvas getCanvas()
{
    return myCanvas;
}

public void loadURL(String urlString) throws Exception 
{
    BrowserControlNativeShim.webShellLoadURL(nativeWebShell, urlString);
}

public void stop() throws Exception 
{
    BrowserControlNativeShim.webShellStop(nativeWebShell);
}

public void show () throws Exception 
{
    BrowserControlNativeShim.webShellShow(nativeWebShell);
}

public void hide () throws Exception 
{
    BrowserControlNativeShim.webShellHide(nativeWebShell);
}

public void setBounds (Rectangle bounds) throws Exception 
{
    BrowserControlNativeShim.webShellSetBounds(nativeWebShell, bounds);
}

public void moveTo (int x, int y) throws Exception 
{
    BrowserControlNativeShim.webShellMoveTo(nativeWebShell, x, y);
}

/**
 *
 */
public void setFocus () throws Exception 
{
    BrowserControlNativeShim.webShellSetFocus(nativeWebShell);
}

/**
 *
 */
public void removeFocus () throws Exception 
{
    BrowserControlNativeShim.webShellRemoveFocus(nativeWebShell);
}

/**
 *
 */
public void repaint (boolean forceRepaint) throws Exception 
{
    BrowserControlNativeShim.webShellRepaint(nativeWebShell, forceRepaint);
}

/**
 *
 */
public boolean canBack () throws Exception 
{
    return BrowserControlNativeShim.webShellCanBack(nativeWebShell);
}

/**
 *
 */
public boolean canForward () throws Exception 
{
    return BrowserControlNativeShim.webShellCanForward(nativeWebShell);
}

/**
 *
 */
public boolean back () throws Exception 
{
    return BrowserControlNativeShim.webShellBack(nativeWebShell);
}

/**
 *
 */
public boolean forward () throws Exception 
{
    return BrowserControlNativeShim.webShellForward(nativeWebShell);
}

/**
 *
 */
public boolean goTo (int historyIndex) throws Exception 
{
    return BrowserControlNativeShim.webShellGoTo(nativeWebShell, historyIndex);
}

/**
 *
 */
public int getHistoryLength () throws Exception 
{
    return BrowserControlNativeShim.webShellGetHistoryLength(nativeWebShell);
}

/**
 *
 */
public int getHistoryIndex () throws Exception 
{
    return BrowserControlNativeShim.webShellGetHistoryIndex(nativeWebShell);
}

/**
 *
 */
public String getURL (int historyIndex) throws Exception 
{
    return BrowserControlNativeShim.webShellGetURL(nativeWebShell, historyIndex);
}

/**
 * Added by Mark Goddard OTMP 9/2/1999
 */
public boolean refresh() throws Exception
{
    return BrowserControlNativeShim.webShellRefresh(nativeWebShell);
}

/**
 * get EventRegistration object
 */

public EventRegistration getEventRegistration() {
    return this;
}

/**
 * add document load event listener
 */
public boolean addDocumentLoadListener(DocumentLoadListener dll) throws Exception {
    return BrowserControlNativeShim.webShellAddDocListener(nativeWebShell, dll);
}

/**
 *
 */
public int getNativeWebShell () 
{
    return nativeWebShell;
}
	
/**
 *
 */
public void finalize () 
{
    try {
        BrowserControlNativeShim.webShellDelete(nativeWebShell);
    }
    catch (Exception ex) {
        System.out.println(ex.toString());
    }
    nativeWebShell = 0;
}

// ----UNIT_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    //    BrowserControlImpl me = new BrowserControlImpl();
    Log.setApplicationName("BrowserControlImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: BrowserControlImpl.java,v 1.9 2000/02/18 19:32:22 edburns%acm.org Exp $");
    
}

// ----UNIT_TEST_END

} // end of class BrowserControlImpl
