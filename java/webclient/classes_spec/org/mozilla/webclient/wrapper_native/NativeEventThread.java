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
 *                  Ashutosh Kulkarni <ashuk@eng.sun.com>
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
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
import java.awt.event.MouseListener;
import org.mozilla.webclient.WebclientEvent;
import org.mozilla.webclient.WebclientEventListener;
import org.mozilla.webclient.UnimplementedException;

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
    synchronized(this) {
        // this has to be inside the synchronized block!
        browserControlCanvas = null;
        synchronized (this) {
            try {
                wait();
            }
            catch (Exception e) {
                System.out.println("NativeEventThread.delete: interrupted while waiting\n\t for NativeEventThread to notify() after destruction of initContext: " + e + 
                                   " " + e.getMessage());
            }
        }
    }
    // PENDING(ashuk): do any necessary cleanup.
    listenersToAdd = null;
    doRemoveListeners();
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
    //   this.setPriority(Thread.MIN_PRIORITY);
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

    while (true) {
        synchronized (this) {

            // this has to be inside the synchronized block!
            if (null == this.browserControlCanvas) {
                // if we get here, this Thread is terminating, destroy
                // the initContext and notify the WindowControl
                ((WindowControlImpl)windowControl).nativeDestroyInitContext(nativeWebShell);
                try {
                    notify();
                }
                catch (Exception e) {
                    System.out.println("NativeEventThread.run: Exception: trying to send notify() to this during delete: " + e + " " + e.getMessage());
                }
                return;
            }

            synchronized (this.browserControlCanvas.getTreeLock()) {
                nativeProcessEvents(nativeWebShell);
            }
            
            if (null != listenersToAdd && !listenersToAdd.isEmpty()) {
                tempEnum = listenersToAdd.elements();

                while (tempEnum.hasMoreElements()) {
                    WCEventListenerWrapper tempListener = 
                        (WCEventListenerWrapper) tempEnum.nextElement();
                    nativeAddListener(nativeWebShell,tempListener.listener, 
                                      tempListener.listenerClassName);
                }
                // use removeAllElements instead of clear for jdk1.1.x
                // compatibility.
                listenersToAdd.removeAllElements();
            }
            doRemoveListeners();
            
        }
    }
}

//
// private methods
//

/**

 *  this was broken out into a separate method due to the complexity of
 *  handling the case where we are to remove all listeners.

 */

private void doRemoveListeners()
{
    if (null != listenersToRemove && !listenersToRemove.isEmpty()) {
        tempEnum = listenersToRemove.elements();
        while (tempEnum.hasMoreElements()) {
            Object listenerObj = tempEnum.nextElement();
            String listenerString;
            if (listenerObj instanceof String) {
                listenerString = (String) listenerObj;
                if (listenerString.equals("all")) {
                    nativeRemoveAllListeners(nativeWebShell);
                    return;
                }
                else {
                    throw new UnimplementedException("Webclient doesn't understand how to remove " + ((String)listenerObj) + ".");
                }
            }
            else {
                WCEventListenerWrapper tempListener = 
                    (WCEventListenerWrapper) listenerObj;
                nativeRemoveListener(nativeWebShell, 
                                     tempListener.listener,
                                     tempListener.listenerClassName);
                
            }
        }
        // use removeAllElements instead of clear for jdk1.1.x
        // compatibility.
        listenersToRemove.removeAllElements();
    }
}

//
// Package methods
//

/**

 * Takes the abstract WebclientEventListener instance and adds it to a
 * Vector of listeners to be added.  This vector is scanned each time
 * around the event loop in run(). <P>

 * The vector is a vector of WCEventListenerWrapper instances.  In run()
 * these are unpacked and sent to nativeAddListener like this:
 * nativeAddListener(nativeWebShell,tempListener.listener, 
 * tempListener.listenerClassName); <P>

 * @see run

 */

void addListener(WCEventListenerWrapper newListener)
{
    Assert.assert(-1 != nativeWebShell);
    Assert.assert(null != windowControl);

    synchronized (this) {
        if (null == listenersToAdd) {
            listenersToAdd = new Vector();
        }
        // use addElement instead of add for jdk1.1.x
        // compatibility.
        listenersToAdd.addElement(newListener);
    }
}

/**

 * remove a listener

 * @param newListener if null, removes all listeners

 */

void removeListener(WCEventListenerWrapper newListener)
{
    Assert.assert(-1 != nativeWebShell);
    Assert.assert(null != windowControl);

    synchronized (this) {
        if (null == listenersToRemove) {
            listenersToRemove = new Vector();
        }
        if (null == newListener) {
            String all = "all";
            // use addElement instead of add for jdk1.1.x
            // compatibility.
            listenersToRemove.addElement(all);
        }
        else {
            // use addElement instead of add for jdk1.1.x
            // compatibility.
            listenersToRemove.addElement(newListener);
        }
    }
    
}

/**

 * This method is called from native code when an event occurrs.  This
 * method relies on the fact that all events types that the client can
 * observe descend from WebclientEventListener.  I use instanceOf to
 * determine what kind of WebclientEvent subclass to create.

 */

void nativeEventOccurred(WebclientEventListener target, 
                         String targetClassName, long eventType, 
                         Object eventData)
{
    ParameterCheck.nonNull(target);
    ParameterCheck.nonNull(targetClassName);

    Assert.assert(-1 != nativeWebShell);
    Assert.assert(null != windowControl);
    
    WebclientEvent event = null;
    
    if (DocumentLoadListener.class.getName().equals(targetClassName)) {
        event = new DocumentLoadEvent(this, eventType, eventData);
    }
    else if (MouseListener.class.getName().equals(targetClassName)) {
        Assert.assert(target instanceof WCMouseListenerImpl);
        
        // We create a plain vanilla WebclientEvent, which the
        // WCMouseListenerImpl target knows how to deal with.
        
        // Also, the source happens to be the browserControlCanvas
        // to satisfy the java.awt.event.MouseEvent's requirement
        // that the source be a java.awt.Component subclass.
        
        event = new WebclientEvent(browserControlCanvas, eventType, eventData);
    }
    // else...
    
    // PENDING(edburns): maybe we need to put this in some sort of
    // event queue?
    target.eventDispatched(event);
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
                                     WebclientEventListener typedListener,
                                     String listenerName);

/**

 * Called from Java to allow the native code to remove a listener.

 */ 

public native void nativeRemoveListener(int webShellPtr, 
                                        WebclientEventListener typedListener,
                                        String listenerName);

/**

 * Called from Java to allow the native code to remove all listeners.

 */ 

public native void nativeRemoveAllListeners(int webShellPrt);

} // end of class NativeEventThread
