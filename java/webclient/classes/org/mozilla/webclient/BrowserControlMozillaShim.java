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

// MWebShellMozillaShim.java

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.awt.*;

/**
 *

 *  <B>MWebShellMozillaShim</B> is a class with native methods that
 *  provides the glue between MWebShell and nsIWebShell. <P>

 * WAS: instance <P>

 * <B>Lifetime And Scope</B> <P>

 * There is one instance of this class and all of the exposed methods
 * are static.

 * @version $Id: BrowserControlMozillaShim.java,v 1.1 1999/07/30 01:03:05 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlImpl

 * 

 */

public class BrowserControlMozillaShim extends Object
{
//
// Protected Constants
//

//
// Class Variables
//

private static boolean          initialized = false;

private static BrowserControlMozillaShim	instance = null;

private static Object         	lock = null;


//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

//
// Constructors and Initializers    
//

public BrowserControlMozillaShim()
{
    super();
	lock = new Object();
}

//
// Class methods
//

public static void initialize () throws Exception 
{
	if (!initialized) {
		instance = new BrowserControlMozillaShim();
		
		try {
			System.loadLibrary("webclient");
		}
		catch (java.lang.UnsatisfiedLinkError e) {
			throw new Exception("Unable to open native webclient library");
		}
		
		instance.nativeInitialize();
		initialized = true;
	}
}

public static void terminate () throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeTerminate();
			initialized = false;
		}
	}
}

public static void processEvents(int theWebShell) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeProcessEvents(theWebShell);
		}
	}
}

//
// Window events
//    

public static void sendKeyDownEvent (int widgetPtr,
									 char keyChar, int keyCode, 
									 int modifiers, int eventTime)
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeSendKeyDownEvent(widgetPtr,
											keyChar, keyCode, modifiers, 
											eventTime);
		}
	}
}

public static void sendKeyUpEvent (int widgetPtr,
								   char keyChar, int keyCode, 
								   int modifiers, int eventTime)
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeSendKeyUpEvent(widgetPtr, keyChar, 
										  keyCode, modifiers, eventTime);
			
		}
	}
}

public static void sendMouseEvent (int windowPtr, int widgetPtr,
								   int widgetX, int widgetY,
								   int windowX, int windowY,
								   int mouseMessage, int numClicks,
								   int modifiers, int eventTime) 
{
	
	synchronized(lock) {
		if (initialized) {
			instance.nativeSendMouseEvent(windowPtr, widgetPtr,
										  widgetX, widgetY,
										  windowX, windowY,
										  mouseMessage, numClicks,
										  modifiers, eventTime);
		}
	}
}

public static void idleEvent (int windowPtr) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeIdleEvent(windowPtr, 0);
		}
	}
}

public static void updateEvent (int windowPtr) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeUpdateEvent(windowPtr, 0);
		}
	}
}

//
// Widget methods
//

public static int widgetCreate (int windowPtr, 
								Rectangle bounds) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return(instance.nativeWidgetCreate(windowPtr, 
											   bounds.x, bounds.y,
											   bounds.width + 1, bounds.height + 1));
		} else {
			return 0;
		}
	}
}

public static void widgetDelete (int widgetPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWidgetDelete(widgetPtr);
		}
	}
}

public static void widgetResize (int widgetPtr, Rectangle bounds, 
								 boolean repaint) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWidgetResize(widgetPtr, bounds.x, bounds.y, bounds.width + 1, bounds.height + 1, repaint);
		}
	}
}

public static void widgetResize (int widgetPtr, int width, 
								 int height, boolean repaint) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWidgetResize(widgetPtr, 0, 0, width + 1, height + 1, repaint);
		}
	}
}

public static void widgetEnable (int widgetPtr, boolean enable) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWidgetEnable(widgetPtr, enable);
		}
	}
}

public static void widgetShow (int widgetPtr, boolean show) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWidgetShow(widgetPtr, show);
		}
	}
}

public static void widgetInvalidate (int widgetPtr, boolean isSynchronous) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWidgetInvalidate(widgetPtr, isSynchronous);
		}
	}
}

public static void widgetUpdate (int widgetPtr) 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWidgetUpdate(widgetPtr);
		}
	}
}

//
// WebShell methods
//

public static int webShellCreate (int windowPtr, 
								  Rectangle bounds) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return(instance.nativeWebShellCreate(windowPtr, bounds.x, bounds.y, bounds.width + 1, bounds.height + 1));
		}
		else {
			throw new Exception("Error: unable to create native nsIWebShell");
		}
	}
}

public static void webShellDelete (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellDelete(webShellPtr);
		}
	}
}

public static void webShellLoadURL (int webShellPtr, 
									String urlString) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellLoadURL(webShellPtr, urlString);
		}
	}
}

public static void webShellStop (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellStop(webShellPtr);
		}
	}
}

public static void webShellShow (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellShow(webShellPtr);
		}
	}
}

public static void webShellHide (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellHide(webShellPtr);
		}
	}
}

public static void webShellSetBounds (int webShellPtr, 
									  Rectangle bounds) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellSetBounds(webShellPtr, bounds.x, bounds.y, bounds.width + 1, bounds.height + 1);
		}
	}
}

public static void webShellMoveTo (int webShellPtr, 
								   int x, int y) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellMoveTo(webShellPtr, x, y);
		}
	}
}

public static void webShellSetFocus (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellSetFocus(webShellPtr);
		}
	}
}

public static void webShellRemoveFocus (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellRemoveFocus(webShellPtr);
		}
	}
}

public static void webShellRepaint (int webShellPtr, 
									boolean forceRepaint) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			instance.nativeWebShellRepaint(webShellPtr, forceRepaint);
		}
	}
}

public static boolean webShellCanBack (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellCanBack(webShellPtr);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}

public static boolean webShellCanForward (int webShellPtr) throws Exception
{
	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellCanForward(webShellPtr);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}

public static boolean webShellBack (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellBack(webShellPtr);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}

public static boolean webShellForward (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellForward(webShellPtr);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}

public static boolean webShellGoTo (int webShellPtr, 
									int historyIndex) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellGoTo(webShellPtr, historyIndex);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}

public static int webShellGetHistoryLength (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellGetHistoryLength(webShellPtr);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}

public static int webShellGetHistoryIndex (int webShellPtr) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellGetHistoryIndex(webShellPtr);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}

public static String webShellGetURL(int webShellPtr, 
									int historyIndex) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellGetURL(webShellPtr, historyIndex);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}

//
// Native interfaces
//

private native void nativeInitialize () throws Exception;
private native void nativeTerminate () throws Exception;
    

//
// Event interfaces
//
  
private native void nativeSendKeyDownEvent(int widgetPtr, char keyChar, int keyCode, int modifiers, int eventTime);
private native void nativeSendKeyUpEvent(int widgetPtr, char keyChar, int keyCode, int modifiers, int eventTime);
private native void nativeSendMouseEvent(int windowPtr, int widgetPtr,
										 int widgetX, int widgetY,
										 int windowX, int windowY,
										 int mouseMessage, int numClicks,
										 int modifiers, int eventTime);
private native void nativeProcessEvents(int theWebShell);
private native void nativeIdleEvent (int windowPtr, int eventTime);
private native void nativeUpdateEvent (int windowPtr, int eventTime);


//
// Widget interfaces
//

private native int  nativeWidgetCreate (int windowPtr, int x, int y, int width, int height) throws Exception;
private native void nativeWidgetDelete (int widgetPtr) throws Exception;
private native void nativeWidgetResize (int widgetPtr, int x, int y, int width, int height, boolean repaint);

/*
  private native void nativeWidgetResize (int widgetPtr, int width, int height, boolean repaint);
  */

private native void nativeWidgetEnable (int widgetPtr, boolean enable);
private native void nativeWidgetShow   (int widgetPtr, boolean show);
private native void nativeWidgetInvalidate (int widgetPtr, boolean isSynchronous);
private native void nativeWidgetUpdate (int widgetPtr);



//
// WebShell interface
//

private native int  	nativeWebShellCreate			(int windowPtr,
														 int x, int y, int width, int height) throws Exception;
private native void		nativeWebShellDelete			(int webShellPtr) throws Exception;
private native void		nativeWebShellLoadURL			(int webShellPtr, String urlString) throws Exception;
private native void		nativeWebShellStop				(int webShellPtr) throws Exception;
private native void		nativeWebShellShow				(int webShellPtr) throws Exception;
private native void		nativeWebShellHide				(int webShellPtr) throws Exception;
private native void		nativeWebShellSetBounds			(int webShellPtr, int x, int y, int width, int height) throws Exception;
private native void		nativeWebShellMoveTo			(int webShellPtr, int x, int y) throws Exception;
private native void		nativeWebShellSetFocus			(int webShellPtr) throws Exception;
private native void		nativeWebShellRemoveFocus		(int webShellPtr) throws Exception;
private native void		nativeWebShellRepaint 			(int webShellPtr, boolean forceRepaint) throws Exception;
private native boolean	nativeWebShellCanBack 			(int webShellPtr) throws Exception;
private native boolean	nativeWebShellCanForward		(int webShellPtr) throws Exception;
private native boolean	nativeWebShellBack				(int webShellPtr) throws Exception;
private native boolean	nativeWebShellForward			(int webShellPtr) throws Exception;
private native boolean	nativeWebShellGoTo				(int webShellPtr, int aHistoryIndex) throws Exception;
private native int		nativeWebShellGetHistoryLength	(int webShellPtr) throws Exception;
private native int		nativeWebShellGetHistoryIndex	(int webShellPtr) throws Exception;
private native String	nativeWebShellGetURL			(int webShellPtr, int aHistoryIndex) throws Exception;

//
// General Methods
//

// ----UNIT_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);
    BrowserControlMozillaShim me = new BrowserControlMozillaShim();
    Log.setApplicationName("BrowserControlMozillaShim");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: BrowserControlMozillaShim.java,v 1.1 1999/07/30 01:03:05 edburns%acm.org Exp $");
    
}

// ----UNIT_TEST_END

} // end of class BrowserControlMozillaShim
