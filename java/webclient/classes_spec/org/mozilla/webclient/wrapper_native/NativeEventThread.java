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

import java.util.Vector;
import java.util.Enumeration;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.BrowserControlCanvas;
import org.mozilla.webclient.WindowControl;
import org.mozilla.webclient.DocumentLoadEvent;
import org.mozilla.webclient.DocumentLoadListener;
import org.mozilla.webclient.WebclientEvent;
import org.mozilla.webclient.WebclientEventListener;

public class NativeEventThread extends Thread 
{

//
// Attribute ivars
//

//
// Relationship ivars
//

/**

 * Vector of listener objects to add.  

 */

private Vector listenersToAdd;

/**

 * Vector of listener objects to remove.  

 */

private Vector listenersToRemove;

/** 
      
 * a handle to the actual mozilla webShell, obtained in constructor
   
 */
  
private int nativeWebShell = -1;

/**

 * a reference to the WindowControl that created us.
 
 */
    
private WindowControl windowControl;

private BrowserControl browserControl;

private BrowserControlCanvas browserControlCanvas;


/**

 * Used in run()

 */

private Enumeration tempEnum;

//
// Attribute ivars
//

//
// Constructors
//

public NativeEventThread(String threadName, BrowserControl yourBrowserControl)
{
    super(threadName);
    ParameterCheck.nonNull(yourBrowserControl);

    browserControl = yourBrowserControl;

    try {
        windowControl = (WindowControl) 
            browserControl.queryInterface(BrowserControl.WINDOW_CONTROL_NAME);
        nativeWebShell = windowControl.getNativeWebShell();

        browserControlCanvas = (BrowserControlCanvas)
            browserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
    }
    catch (Exception e) {
        System.out.println("NativeEventThread constructor: Exception: " + e + 
                           " " + e.getMessage());
    }
}

/**

 * This is a very delicate method, and possibly subject to race
 * condition problems.  To combat this, our first step is to set our
 * browserControlCanvas to null, within a synchronized block which
 * synchronizes on the same object used in the run() method's event
 * loop.  By setting the browserControlCanvas ivar to null, we cause the
 * run method to return.

 */

public void delete()
{
    // setting this to null causes the run thread to exit
    synchronized(this.browserControlCanvas.getTreeLock()) {
        browserControlCanvas = null;
    }
    // PENDING(ashuk): do any necessary cleanup.
    listenersToAdd = null;
    listenersToRemove = null;
    nativeWebShell = -1;
    windowControl = null;
    browserControl = null;
    tempEnum = null;
}

//
// Methods from Thread
//

/**

 * This method is the heart of webclient.  It should only be called when
 * WindowControlImpl.createWindow() is called.  It calls
 * nativeInitialize, which does the per-window initialization, including
 * creating the native event queue which corresponds to this instance,
 * then enters into an infinite loop where processes native events, then
 * checks to see if there are any listeners to add, and adds them if
 * necessary.

 * @see nativeInitialize
 
 * @see nativeProcessEvents

 * @see nativeAddListener

 */

public void run() 
{
    Assert.assert(-1 != nativeWebShell);
    Assert.assert(null != windowControl);

    nativeInitialize(nativeWebShell);

    // IMPORTANT: tell the windowControl, who is waiting for this
    // message, that we have initialized successfully.
    synchronized(windowControl) {
        try {
            windowControl.notify();
        }
        catch (Exception e) {
            System.out.println("NativeEventThread.run: Exception: trying to send notify() to windowControl: " + e + " " + e.getMessage());
        }
    }

    while (null != this.browserControlCanvas) {
        synchronized (this.browserControlCanvas.getTreeLock()) {
            nativeProcessEvents(nativeWebShell);

            if (null != listenersToAdd && !listenersToAdd.isEmpty()) {
                tempEnum = listenersToAdd.elements();
                while (tempEnum.hasMoreElements()) {
                    nativeAddListener(nativeWebShell,
                                      (WebclientEventListener)
                                      tempEnum.nextElement());
                }
                listenersToAdd.clear();
            }
        }
    }
}

//
// Package methods
//

/**

 * Takes the abstract WebclientEventListener instance and adds it to a
 * Vector of listeners to be added.  This vector is scanned each time
 * around the event loop in run().

 * @see run

 */

void addListener(WebclientEventListener newListener)
{
    Assert.assert(-1 != nativeWebShell);
    Assert.assert(null != windowControl);

    synchronized (this.browserControlCanvas.getTreeLock()) {
        if (null == listenersToAdd) {
            listenersToAdd = new Vector();
        }
        listenersToAdd.add(newListener);
    }
}

/**

 * This method is called from native code when an event occurrs.  This
 * method relies on the fact that all events types that the client can
 * observe descend from WebclientEventListener.  I use instanceOf to
 * determine what kind of WebclientEvent subclass to create.

 */

void nativeEventOccurred(WebclientEventListener target, long eventType)
{
    ParameterCheck.nonNull(target);

    Assert.assert(-1 != nativeWebShell);
    Assert.assert(null != windowControl);
    
    synchronized(this.browserControlCanvas.getTreeLock()) {
        WebclientEvent event = null;
        
        if (target instanceof DocumentLoadListener) {
            System.out.println("debug: edburns: creating DocumentLoadEvent");
            event = new DocumentLoadEvent(this, eventType);
        }
        // else...
        
        // PENDING(edburns): maybe we need to put this in some sort of
        // event queue?

        System.out.println("About to call eventDispatched on listener");
        target.eventDispatched(event);
    }
}

//
// local methods
//




//
// Native methods
//

/**

 * Takes the int from WindowControlImpl.nativeCreateInitContext, the
 * meaning of which is left up to the implementation, and does any
 * per-window creation and initialization tasks. <P>

 * For mozilla, this means creating the nsIWebShell instance, attaching
 * to the native event queue, creating the nsISessionHistory instance, etc.

 */

public native void nativeInitialize(int webShellPtr);

/**

 * Called from java to allow the native code to process any pending
 * events, such as: painting the UI, processing mouse and key events,
 * etc.

 */

public native void nativeProcessEvents(int webShellPtr);

/**

 * Called from Java to allow the native code to add a "listener" to the
 * underlying browser implementation.  The native code should do what's
 * necessary to add the appropriate listener type.  When a listener
 * event occurrs, the native code should call the nativeEventOccurred
 * method of this instance, passing the typedListener argument received
 * from nativeAddListener.  See the comments in the native
 * implementation.

 * @see nativeEventOccurred

 */

public native void nativeAddListener(int webShellPtr,
                                     WebclientEventListener typedListener);

} // end of class NativeEventThread
