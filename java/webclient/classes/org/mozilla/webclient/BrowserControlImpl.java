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
 * The original code is RaptorCanvas
 * 
 * The Initial Developer of the Original Code is Kirk Baker <kbaker@eb.com> and * Ian Wilkinson <iw@ennoble.com
 */

package org.mozilla.webclient;

// BrowserControlImpl.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.awt.Rectangle;

/**
 *
 *  <B>BrowserControlImpl</B> provides the implementation for BrowserControl
 *
 * <B>Lifetime And Scope</B> <P>
 *
 * @version $Id: BrowserControlImpl.java,v 1.1 1999/07/30 01:03:05 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControl
 *
 */

public class BrowserControlImpl extends Object implements BrowserControl
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

//
// Constructors and Initializers    
//

public BrowserControlImpl(int windowPtr, Rectangle bounds) throws Exception 
{
	nativeWebShell = BrowserControlMozillaShim.webShellCreate(windowPtr, bounds);
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

public void loadURL(String urlString) throws Exception 
{
    BrowserControlMozillaShim.webShellLoadURL(nativeWebShell, urlString);
}

public void stop() throws Exception 
{
    BrowserControlMozillaShim.webShellStop(nativeWebShell);
}

public void show () throws Exception 
{
    BrowserControlMozillaShim.webShellShow(nativeWebShell);
}

public void hide () throws Exception 
{
    BrowserControlMozillaShim.webShellHide(nativeWebShell);
}

public void setBounds (Rectangle bounds) throws Exception 
{
    BrowserControlMozillaShim.webShellSetBounds(nativeWebShell, bounds);
}

public void moveTo (int x, int y) throws Exception 
{
    BrowserControlMozillaShim.webShellMoveTo(nativeWebShell, x, y);
}

/**
 *
 */
public void setFocus () throws Exception 
{
    BrowserControlMozillaShim.webShellSetFocus(nativeWebShell);
}

/**
 *
 */
public void removeFocus () throws Exception 
{
    BrowserControlMozillaShim.webShellRemoveFocus(nativeWebShell);
}

/**
 *
 */
public void repaint (boolean forceRepaint) throws Exception 
{
    BrowserControlMozillaShim.webShellRepaint(nativeWebShell, forceRepaint);
}

/**
 *
 */
public boolean canBack () throws Exception 
{
    return BrowserControlMozillaShim.webShellCanBack(nativeWebShell);
}

/**
 *
 */
public boolean canForward () throws Exception 
{
    return BrowserControlMozillaShim.webShellCanForward(nativeWebShell);
}

/**
 *
 */
public boolean back () throws Exception 
{
    return BrowserControlMozillaShim.webShellBack(nativeWebShell);
}

/**
 *
 */
public boolean forward () throws Exception 
{
    return BrowserControlMozillaShim.webShellForward(nativeWebShell);
}

/**
 *
 */
public boolean goTo (int historyIndex) throws Exception 
{
    return BrowserControlMozillaShim.webShellGoTo(nativeWebShell, historyIndex);
}

/**
 *
 */
public int getHistoryLength () throws Exception 
{
    return BrowserControlMozillaShim.webShellGetHistoryLength(nativeWebShell);
}

/**
 *
 */
public int getHistoryIndex () throws Exception 
{
    return BrowserControlMozillaShim.webShellGetHistoryIndex(nativeWebShell);
}

/**
 *
 */
public String getURL (int historyIndex) throws Exception 
{
    return BrowserControlMozillaShim.webShellGetURL(nativeWebShell, historyIndex);
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
        BrowserControlMozillaShim.webShellDelete(nativeWebShell);
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
    Log.setApplicationVersionDate("$Id: BrowserControlImpl.java,v 1.1 1999/07/30 01:03:05 edburns%acm.org Exp $");
    
}

// ----UNIT_TEST_END

} // end of class BrowserControlImpl
