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

// BrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.awt.*;

/**
 *
 * BrowserControlCanvas is the principal class for embedding
 * the Mozilla WebShell into the Java framework.
 * This component represents the proxy for the native
 * WebShell class as provided by the nsIWebShell
 * interface.
 *
 * @see org.mozilla.webclient.BrowserControl
 *
 * @author Kirk Baker
 * @author Ian Wilkinson

 * <P> 

 * <B>Lifetime And Scope</B> <P>

 * See concrete subclasses for scope info.

 * @version $Id: BrowserControlCanvas.java,v 1.1 1999/07/30 01:03:04 edburns%acm.org Exp $

 * @see	org.mozilla.webclient.Win32BrowserControlCanvas

 */

import java.awt.*;
import java.awt.Canvas;
import java.awt.event.*;
import sun.awt.*;

public abstract class BrowserControlCanvas extends Canvas 
{

//
// Class Variables
//

private static int		webShellCount = 0;

//
// Instance Variables
//

// Attribute Instance Variables

private boolean		initializeOK;
private boolean		boundsValid;
private boolean		hasFocus;

// Relationship Instance Variables


private BrowserControl	webShell;
private int			nativeWindow;
private Rectangle		windowRelativeBounds;

// PENDING(edburns): Is this needed: // private BrowserControlIdleThread	idleThread;

//
// Constructors and Initializers    
//

/**
 * Initialize the BrowserControlMozillaShim. For now,
 * this initializes the Mozilla registry.
 */
public BrowserControlCanvas () 
{
	nativeWindow = 0;
	webShell = null;
	initializeOK = false;
	boundsValid = false;
	hasFocus = false;
	
	try {
		BrowserControlMozillaShim.initialize();
	} catch (Exception e) {
		System.out.println(e.toString());
	}
} // BrowserControlCanvas() ctor
	
	
/**
  * Obtain the native window handle for this component's
  * peer.
  */

abstract protected int getWindow(DrawingSurfaceInfo dsi);

//
// Methods from Canvas
//


/**
  * Instantiate the Mozilla WebShell container.
  */
public void addNotify () 
{
	super.addNotify();
	
	DrawingSurface ds = (DrawingSurface)this.getPeer();
	DrawingSurfaceInfo dsi = ds.getDrawingSurfaceInfo();
	
	windowRelativeBounds = new Rectangle();
	
	// We must lock() the DrawingSurfaceInfo before
	// accessing its native window handle.
	dsi.lock();
	nativeWindow = getWindow(dsi);
	
	try {
		Rectangle r = new Rectangle(getBoundsRelativeToWindow());
		webShell = new BrowserControlImpl(nativeWindow, r);
	} catch (Exception e) {
		dsi.unlock();
		System.out.println(e.toString());
		return;
	}
	
	dsi.unlock();
	
	initializeOK = true;
	webShellCount++;
	
	/*
	  requestFocus();
	  */
} // addNotify()

public BrowserControl getWebShell () 
{
	return webShell;
} // getWebShell()

protected Point getEventCoordsLocalToWindow(MouseEvent evt) 
{
	Rectangle    localBounds = getBoundsRelativeToWindow();
	int            windowLocalX = evt.getX() + localBounds.x;
	int            windowLocalY = evt.getY() + localBounds.y;
	
	return new Point(windowLocalX, windowLocalY);
} // getEventCoordsLocalToWindow()

protected Rectangle getWindowBounds () 
{// Throw an Exception?
	Container    parent = getParent();
	
	if (parent != null) {
		do {
			// if the parent is a window, then return its bounds
			if (parent instanceof Window == true) {
				return parent.getBounds();
			}
			parent = parent.getParent();            
		} while (parent != null);
	}
	
	return new Rectangle();
} // getWindowBounds()

protected Rectangle getBoundsRelativeToWindow ()
{
	if (boundsValid) {
		return windowRelativeBounds;
	}
	
	Container    parent = getParent();
	Point        ourLoc = getLocation();
	Rectangle    ourBounds = getBounds();
	
	if (parent != null) {
		do {
			// if the parent is a window, then don't adjust to its offset
			//  and look no further
			if (parent instanceof Window == true) {
				break;
			}
			
			Point parentLoc = parent.getLocation();
			ourLoc.translate(-parentLoc.x, -parentLoc.y);
			parent = parent.getParent();            
		} while (parent != null);
	}
	
	windowRelativeBounds.setBounds(-ourLoc.x, -ourLoc.y, ourBounds.width, ourBounds.height);
	boundsValid = true;
	
	return windowRelativeBounds;
} // getBoundsRelativeToWindow()

public void setBounds(int x, int y, int w, int h) 
{
	super.setBounds(x, y, w, h);
	if (webShell != null) {
		System.out.println("in BrowserControlCanvas setBounds: x = " + x + " y = " + y + " w = " + w + " h = " + h);
		try {
			webShell.setBounds(new Rectangle(0, 0, w - 1, h - 1));
		}
		catch(Exception ex) {
		}
	}
}

public void setBounds(Rectangle rect) 
{
	super.setBounds(rect);
}

} // class BrowserControlCanvas


// EOF
