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
 *               Mark Goddard
 *               Ann Sunhachawee
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

 * @version $Id: BrowserControlNativeShim.java,v 1.2 2000/02/18 19:16:26 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlImpl

 * 

 */

public class BrowserControlNativeShim extends Object
{
//
// Protected Constants
//

//
// Class Variables
//

private static boolean          initialized = false;

private static BrowserControlNativeShim	instance = null;

private static Object         	lock = null;

  //private static int              myGtkwinptr;


//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

//
// Constructors and Initializers    
//

public BrowserControlNativeShim()
{
    super();
	lock = new Object();
}

//
// Class methods
//

    /** 

     * it is safe to call this method multiple times, it is guaranteed
     * to only actually do something once.

     */

public static void initialize(String verifiedBinDirAbsolutePath) throws Exception 
{
	if (!initialized) {
		instance = new BrowserControlNativeShim();

		instance.nativeInitialize(verifiedBinDirAbsolutePath);
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

//
// WebShell methods
//

public static int webShellCreate (int windowPtr, 
                                  Rectangle bounds, BrowserControlImpl abrowsercontrolimpl) throws Exception 
{
	synchronized(lock) {
		if (initialized) {
       			return(instance.nativeWebShellCreate(windowPtr, 
                                                 bounds.x, bounds.y, 
                                                 bounds.width + 1, 
                                                 bounds.height + 1,
						 abrowsercontrolimpl));
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

public static boolean webShellAddDocListener(int webShellPtr, DocumentLoadListener dl) throws Exception 
{
    synchronized(lock) {
        if (initialized) {
            return instance.nativeWebShellAddDocListener(webShellPtr, dl);
        } else {
            throw new Exception ("instance is not initialized");
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

/**
 * Added by Mark Goddard OTMP 9/2/1999
 */

public static boolean webShellRefresh(int webShellPtr) throws Exception
{
 	synchronized(lock) {
		if (initialized) {
			return instance.nativeWebShellRefresh(webShellPtr);
		}
		else {
			throw new Exception("instance is not initialized.");
		}
	}
}   
//
// Native interfaces
//

private native void nativeInitialize (String verifiedBinDirAbsolutePath) throws Exception;
private native void nativeTerminate () throws Exception;
    
//
// WebShell interface
//

private native void      nativeDummy (BrowserControlNativeShim testShim);

private native int  	nativeWebShellCreate			(int windowPtr,
														 int x, int y, int width, int height, BrowserControlImpl abrowsercontrolimpl) throws Exception;
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

/**
 * Added by Mark Goddard OTMP 9/2/1999
 */
private native boolean  nativeWebShellRefresh           (int webShellPtr) throws Exception;
private native boolean nativeWebShellAddDocListener(int webShellPtr, DocumentLoadListener dl);

public static native void nativeDebugBreak(String fileName, int lineNumber);

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
    BrowserControlNativeShim me = new BrowserControlNativeShim();
    Log.setApplicationName("BrowserControlNativeShim");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: BrowserControlNativeShim.java,v 1.2 2000/02/18 19:16:26 edburns%acm.org Exp $");
    
}

// ----UNIT_TEST_END

} // end of class BrowserControlNativeShim
