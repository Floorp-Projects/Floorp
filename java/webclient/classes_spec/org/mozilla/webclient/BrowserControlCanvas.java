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
 */

package org.mozilla.webclient;

// BrowserControlCanvas.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.awt.*;
import java.awt.event.*;

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

 * @version $Id: BrowserControlCanvas.java,v 1.8 2004/10/27 01:33:56 edburns%acm.org Exp $

 * @see	org.mozilla.webclient.win32.Win32BrowserControlCanvas

 * @see	org.mozilla.webclient.gtk.GtkBrowserControlCanvas

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

 * just initialize all the ivars.  the initialize() method must be called
 * before an instance of this class can be used.  Instances are created
 * in BrowserControlFactory.newBrowserControl().

 */
protected BrowserControlCanvas () 
{
	nativeWindow = 0;
	webShell = null;
	initializeOK = false;
	boundsValid = false;
	hasFocus = false;
	
} // BrowserControlCanvas() ctor

// PENDING(edburns): make this protected again
public void initialize(BrowserControl controlImpl)
{
    ParameterCheck.nonNull(controlImpl);
    webShell = controlImpl;
}

/**
  * Create the Native gtk window and get it's handle
  */

abstract protected int getWindow();

//
// Methods from Canvas
//


/**
  * Instantiate the Mozilla WebShell container.
  */
public void addNotify () 
{
	super.addNotify();

	windowRelativeBounds = new Rectangle();

    //Create the Native gtkWindow and it's container and
    //get a handle to this widget
   	nativeWindow = getWindow();

	try {
		Rectangle r = new Rectangle(getBoundsRelativeToWindow());
        Assert.assert_it(null != webShell);

        WindowControl wc = (WindowControl)
            webShell.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        //This createWindow call sets in motion the creation of the
        //nativeInitContext and the creation of the Mozilla embedded
        //webBrowser
        wc.createWindow(nativeWindow, r);
	} catch (Exception e) {
		System.out.println(e.toString());
		return;
	}

	initializeOK = true;
	webShellCount++;
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
    if (!initializeOK) {
        throw new IllegalStateException("Can't resize canvas before adding it to parent");
    }
	super.setBounds(x, y, w, h);
    Rectangle boundsRect = new Rectangle(0, 0, w - 1, h - 1);
	if (webShell != null) {
		if (boundsRect.width < 1) {
			boundsRect.width = 1;
        }
		if (boundsRect.height < 1) {
			boundsRect.height = 1;
		}
		System.out.println("in BrowserControlCanvas setBounds: x = " + x + " y = " + y + " w = " + w + " h = " + h);
		try {
            WindowControl wc = (WindowControl)
                webShell.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
            wc.setBounds(boundsRect);
		}
		catch(Exception ex) {
            System.out.println("Can't setBounds(" + boundsRect + ") " + 
                               ex.getMessage());
            
		}
	}
}

public void setBounds(Rectangle rect) 
{
	super.setBounds(rect);
}

public void setVisible(boolean b) {
    try {
        WindowControl wc = (WindowControl)
            webShell.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        wc.setVisible(b);
    }
    catch(Exception ex) {
        System.out.println("Can't setVisible(" + b + ") " + 
                           ex.getMessage());
        
    }
    super.setVisible(b);
}

public void addMouseListener(MouseListener listener) {
    try {
        EventRegistration er = (EventRegistration)
            webShell.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
        er.addMouseListener(listener);
    }
    catch(Exception ex) {
        System.out.println("Can't addMouseListener(" + listener + ") " + 
                           ex.getMessage());
        
    }
}

public void removeMouseListener(MouseListener listener) {
    try {
        EventRegistration er = (EventRegistration)
            webShell.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
        er.removeMouseListener(listener);
    }
    catch(Exception ex) {
        System.out.println("Can't removeMouseListener(" + listener + ") " + 
                           ex.getMessage());
        
    }
}

        

} // class BrowserControlCanvas


// EOF
