/* 
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

import java.util.ArrayList;
import java.util.List;
import java.util.Iterator;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.BrowserControlCanvas;
import org.mozilla.webclient.EventRegistration2;
import org.mozilla.webclient.impl.WrapperFactory;
import org.mozilla.webclient.DocumentLoadEvent;
import org.mozilla.webclient.DocumentLoadListener;
import org.mozilla.webclient.NewWindowEvent;
import org.mozilla.webclient.NewWindowListener;
import java.awt.event.MouseListener;
import org.mozilla.webclient.WebclientEvent;
import org.mozilla.webclient.WebclientEventListener;
import org.mozilla.webclient.UnimplementedException;

public class EventRegistrationImpl extends ImplObjectNative implements EventRegistration2
{
//
// Constants
//

//
// Class Variables
//

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

    private BrowserControlCanvas browserControlCanvas = null;

    private List documentLoadListeners;

    private BrowserToJavaEventPump eventPump = null;

    private static int instanceCount = 0;

//
// Constructors and Initializers
//

public EventRegistrationImpl(WrapperFactory yourFactory,
                 BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
    
    try {
	browserControlCanvas = (BrowserControlCanvas)
	    yourBrowserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
    }
    catch (ClassNotFoundException e) {
	throw new RuntimeException("EventRegistrationImpl: Can't obtain reference to BrowserControlCanvas");
    }
    
    documentLoadListeners = new ArrayList();
    eventPump = new BrowserToJavaEventPump(instanceCount++);
    eventPump.start();
}

public void delete()
{
    super.delete();
    eventPump.stopRunning();
}

//
// Class methods
//

//
// General Methods
//

//
// Methods from EventRegistration
//

public void addDocumentLoadListener(DocumentLoadListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();
    
    synchronized(documentLoadListeners) {
	documentLoadListeners.add(listener);
    }
}

public void removeDocumentLoadListener(DocumentLoadListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();

    synchronized(documentLoadListeners) {
	documentLoadListeners.remove(listener);
    }
}

public void addMouseListener(MouseListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    ParameterCheck.nonNull(listener);

    // We have to wrap the user provided java.awt.event.MouseListener
    // instance in a custom WebclientEventListener subclass that also
    // implements java.awt.event.MouseListener.  See WCMouseListener for
    // more information.

    WCMouseListenerImpl mouseListenerWrapper =
        new WCMouseListenerImpl(listener);

    if (null == mouseListenerWrapper) {
        throw new NullPointerException("Can't instantiate WCMouseListenerImpl, out of memory.");
    }

    WCEventListenerWrapper listenerWrapper =
        new WCEventListenerWrapper(mouseListenerWrapper,
                                   MouseListener.class.getName());

    if (null == listenerWrapper) {
        throw new NullPointerException("Can't instantiate WCEventListenerWrapper, out of memory.");
    }

    synchronized(getBrowserControl()) {
        // PENDING nativeEventThread.addListener(listenerWrapper);
    }
}

public void removeMouseListener(MouseListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    ParameterCheck.nonNull(listener);

    WCMouseListenerImpl mouseListenerWrapper =
        new WCMouseListenerImpl(listener);

    if (null == mouseListenerWrapper) {
        throw new NullPointerException("Can't instantiate WCMouseListenerImpl, out of memory.");
    }

    WCEventListenerWrapper listenerWrapper =
        new WCEventListenerWrapper(mouseListenerWrapper,
                                   MouseListener.class.getName());

    if (null == listenerWrapper) {
        throw new NullPointerException("Can't instantiate WCEventListenerWrapper, out of memory.");
    }

    synchronized(getBrowserControl()) {
        // PENDING nativeEventThread.removeListener(listenerWrapper);
    }
}

public void addNewWindowListener(NewWindowListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    ParameterCheck.nonNull(listener);

    WCEventListenerWrapper listenerWrapper =
        new WCEventListenerWrapper(listener,
                                   NewWindowListener.class.getName());
    if (null == listenerWrapper) {
        throw new NullPointerException("Can't instantiate WCEventListenerWrapper, out of memory.");
    }

    synchronized(getBrowserControl()) {
        // PENDING nativeEventThread.addListener(listenerWrapper);
    }
}

public void removeNewWindowListener(NewWindowListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());
    ParameterCheck.nonNull(listener);

    WCEventListenerWrapper listenerWrapper =
        new WCEventListenerWrapper(listener,
                                   NewWindowListener.class.getName());
    if (null == listenerWrapper) {
        throw new NullPointerException("Can't instantiate WCEventListenerWrapper, out of memory.");
    }

    synchronized(getBrowserControl()) {
        // PENDING nativeEventThread.removeListener(listenerWrapper);
    }
}

/**

 * This method is called from native code when an event occurrs.  This
 * method relies on the fact that all events types that the client can
 * observe descend from WebclientEventListener.  I use instanceOf to
 * determine what kind of WebclientEvent subclass to create.

 */

void nativeEventOccurred(String targetClassName, long eventType,
                         Object eventData)
{
    ParameterCheck.nonNull(targetClassName);

    WebclientEvent event = null;

    if (DocumentLoadListener.class.getName().equals(targetClassName)) {
        event = new DocumentLoadEvent(this, eventType, eventData);
    }
    else if (MouseListener.class.getName().equals(targetClassName)) {
        // We create a plain vanilla WebclientEvent, which the
        // WCMouseListenerImpl target knows how to deal with.

        // Also, the source happens to be the browserControlCanvas
        // to satisfy the java.awt.event.MouseEvent's requirement
        // that the source be a java.awt.Component subclass.

        event = new WebclientEvent(browserControlCanvas, eventType, eventData);
    }
    else if (NewWindowListener.class.getName().equals(targetClassName)) {
        event = new NewWindowEvent(this, eventType, eventData);
    }
    // else...

    eventPump.queueEvent(event);
    eventPump.V();
}

public class BrowserToJavaEventPump extends Thread {
    private boolean keepRunning = false;
    
    private List eventsToJava = null;

    private int count = 0;
    
    public BrowserToJavaEventPump(int instanceCount) {
	super("BrowserToJavaEventPump-" + instanceCount);
	eventsToJava = new ArrayList();
	keepRunning = true;
    }

    //
    // semaphore methods
    // 

    public synchronized void P() {
	while (count <= 0) {
	    try { wait(); } catch (InterruptedException ex) {}
	}
	--count;
    }
    
    public synchronized void V() {
	++count;
	notifyAll();
    }
    
    public void queueEvent(WebclientEvent toQueue) {
	synchronized (eventsToJava) {
	    eventsToJava.add(toQueue);
	}
    }

    public void stopRunning() {
	keepRunning = false;
    }
    
    public void run() {
	WebclientEvent curEvent = null;
	WebclientEventListener curListener = null;
	List listeners = null;

	while (keepRunning) {
	    P();

	    synchronized(eventsToJava) {
		if (!eventsToJava.isEmpty()) {
		    curEvent = (WebclientEvent) eventsToJava.remove(0);
		}
	    }
	    
	    if (null == curEvent) {
		continue;
	    }
	    
	    if (curEvent instanceof DocumentLoadEvent) {
		listeners = EventRegistrationImpl.this.documentLoadListeners;
	    }
	    // else...

	    if (null != curEvent && null != listeners) {
		synchronized (listeners) {
		    Iterator iter = listeners.iterator();
		    while (iter.hasNext()) {
			curListener = (WebclientEventListener) iter.next();
			curListener.eventDispatched(curEvent);
		    }
		}
	    }
	}

	System.out.println(this.getName() + " exiting");
    }

} // end of class BrowserToJavaEventPump


} // end of class EventRegistrationImpl
