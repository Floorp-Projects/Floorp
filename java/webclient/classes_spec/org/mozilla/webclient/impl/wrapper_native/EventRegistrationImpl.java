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
import java.util.Map;
import java.util.Iterator;
import java.util.Set;
import java.util.Collection;
import java.util.Properties;
import java.util.EventObject;

import java.awt.event.InputEvent;
import java.awt.event.MouseEvent;
import java.awt.Component;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.BrowserControlCanvas;
import org.mozilla.webclient.EventRegistration2;
import org.mozilla.webclient.impl.WrapperFactory;
import org.mozilla.webclient.DocumentLoadEvent;
import org.mozilla.webclient.DocumentLoadListener;
import org.mozilla.webclient.PageInfoListener;
import org.mozilla.webclient.NewWindowEvent;
import org.mozilla.webclient.NewWindowListener;
import java.awt.event.MouseListener;
import org.mozilla.webclient.WebclientEvent;
import org.mozilla.webclient.WCMouseEvent;
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

    private List mouseListeners;

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
    mouseListeners = new ArrayList();
    eventPump = new BrowserToJavaEventPump(instanceCount++);
    eventPump.start();
}

public void delete()
{
    documentLoadListeners.clear();
    documentLoadListeners = null;
    mouseListeners.clear();
    mouseListeners = null;
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
	if (listener instanceof PageInfoListener) {
	    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable(){
		    public Object run() {
			nativeSetCapturePageInfo(getNativeBrowserControl(),
						 true);
			return null;
		    }
		});
	}
	
	documentLoadListeners.add(listener);
    }
}

public void removeDocumentLoadListener(DocumentLoadListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();
    
    synchronized(documentLoadListeners) {
	documentLoadListeners.remove(listener);
	
	if (0 == documentLoadListeners.size()) {
	    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable(){
		    public Object run() {
			nativeSetCapturePageInfo(getNativeBrowserControl(),
						 false);
			return null;
		    }
		});
	}
	
    }
}

public void addMouseListener(MouseListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();
    
    synchronized(mouseListeners) {
	mouseListeners.add(listener);
    }
}

public void removeMouseListener(MouseListener listener)
{
    ParameterCheck.nonNull(listener);
    getWrapperFactory().verifyInitialized();
    
    synchronized(mouseListeners) {
	mouseListeners.remove(listener);
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

    EventObject event = null;

    if (DocumentLoadListener.class.getName().equals(targetClassName)) {
        event = new DocumentLoadEvent(this, eventType, 
				      // PENDING(edburns: new URIToStringMap((Map)eventData));
				      eventData);
    }
    else if (MouseListener.class.getName().equals(targetClassName)) {
        event = createMouseEvent(eventType, eventData);
    }
    else if (NewWindowListener.class.getName().equals(targetClassName)) {
        event = new NewWindowEvent(this, eventType, eventData);
    }
    // else...

    eventPump.queueEvent(event);
    eventPump.V();
}

private EventObject createMouseEvent(long eventType, Object eventData) {
    WCMouseEvent mouseEvent = null;
    Properties props = (Properties) eventData;
    int modifiers = 0, x = -1, y = -1, clickCount = 0;
    String str;
    boolean bool;
    if (null != props) { 
	if (null != (str = props.getProperty("ClientX"))) {
	    x = Integer.valueOf(str).intValue();
	}
	if (null != (str = props.getProperty("ClientY"))) {
	    y = Integer.valueOf(str).intValue();
	}
	if (null != (str = props.getProperty("ClickCount"))) {
	    clickCount = Integer.valueOf(str).intValue();
	}
	if (null != (str = props.getProperty("Button"))) {
	    int button = Integer.valueOf(str).intValue();
	    if (1 == button) {
		modifiers += InputEvent.BUTTON1_MASK;
	    }
	    if (2 == button) {
		modifiers += InputEvent.BUTTON2_MASK;
	    }
	    if (3 == button) {
		modifiers += InputEvent.BUTTON3_MASK;
	    }
	}
	if (null != (str = props.getProperty("Alt"))) {
	    bool = Boolean.valueOf(str).booleanValue();
	    if (bool) {
		modifiers += InputEvent.ALT_MASK;
	    }
	}
	if (null != (str = props.getProperty("Ctrl"))) {
	    bool = Boolean.valueOf(str).booleanValue();
	    if (bool) {
		modifiers += InputEvent.CTRL_MASK;
	    }
	}
	if (null != (str = props.getProperty("Meta"))) {
	    bool = Boolean.valueOf(str).booleanValue();
	    if (bool) {
		modifiers += InputEvent.META_MASK;
	    }
	}
	if (null != (str = props.getProperty("Shift"))) {
	    bool = Boolean.valueOf(str).booleanValue();
	    if (bool) {
		modifiers += InputEvent.SHIFT_MASK;
	    }
	}
    }
    WebclientEvent event = new WebclientEvent(browserControlCanvas, eventType,
					      eventData);
    switch ((int) eventType) {
    case (int) WCMouseEvent.MOUSE_DOWN_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) browserControlCanvas,
                             MouseEvent.MOUSE_PRESSED, -1,
                             modifiers, x, y, clickCount, false, event);
        break;
    case (int) WCMouseEvent.MOUSE_UP_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) browserControlCanvas,
                             MouseEvent.MOUSE_RELEASED, -1,
                             modifiers, x, y, clickCount, false, event);
        break;
    case (int) WCMouseEvent.MOUSE_CLICK_EVENT_MASK:
    case (int) WCMouseEvent.MOUSE_DOUBLE_CLICK_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) browserControlCanvas,
                             MouseEvent.MOUSE_CLICKED, -1,
                             modifiers, x, y, clickCount, false, event);
        break;
    case (int) WCMouseEvent.MOUSE_OVER_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) browserControlCanvas,
                             MouseEvent.MOUSE_ENTERED, -1,
                             modifiers, x, y, clickCount, false, event);
        break;
    case (int) WCMouseEvent.MOUSE_OUT_EVENT_MASK:
        mouseEvent = 
            new WCMouseEvent((Component) browserControlCanvas,
                             MouseEvent.MOUSE_EXITED, -1,
                             modifiers, x, y, clickCount, false, event);
        break;
    }
    return mouseEvent;
}

private native void nativeSetCapturePageInfo(int webShellPtr, 
					     boolean newState);

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
    
    public void queueEvent(EventObject toQueue) {
	synchronized (eventsToJava) {
	    eventsToJava.add(toQueue);
	}
    }

    public void stopRunning() {
	keepRunning = false;
    }
    
    public void run() {
	EventObject curEvent = null;
	WebclientEventListener curListener = null;
	Object cur = null;
	List listeners = null;

	while (keepRunning) {
	    P();

	    synchronized(eventsToJava) {
		if (!eventsToJava.isEmpty()) {
		    curEvent = (EventObject) eventsToJava.remove(0);
		}
	    }
	    
	    if (null == curEvent) {
		continue;
	    }
	    
	    if (curEvent instanceof DocumentLoadEvent) {
		listeners = EventRegistrationImpl.this.documentLoadListeners;
	    }
	    else if (curEvent instanceof MouseEvent) {
		listeners = EventRegistrationImpl.this.mouseListeners;
	    }
	    // else...

	    if (null != curEvent && null != listeners) {
		synchronized (listeners) {
		    Iterator iter = listeners.iterator();
		    while (iter.hasNext()) {
			cur = iter.next();
			try {
			    if (cur instanceof WebclientEventListener) {
				curListener = (WebclientEventListener) cur;
				curListener.eventDispatched((WebclientEvent) curEvent);
			    }
			    else if (cur instanceof MouseListener) {
				dispatchMouseEvent((MouseListener) cur, 
						   (WCMouseEvent) curEvent);
			    }
			    // else ...
			}
			catch (Throwable e) {
			    System.out.println("Caught Execption calling listener: " + curListener + ". Exception: " + e + " " + e.getMessage());
			    e.printStackTrace();
			}
		    }
		}
	    }
	}

	System.out.println(this.getName() + " exiting");
    }

    private void dispatchMouseEvent(MouseListener listener, 
				    WCMouseEvent event) {
	switch ((int) event.getWebclientEvent().getType()) {
	case (int) WCMouseEvent.MOUSE_DOWN_EVENT_MASK:
	    listener.mousePressed(event);
	    break;
	case (int) WCMouseEvent.MOUSE_UP_EVENT_MASK:
	    listener.mouseReleased(event);
	    break;
	case (int) WCMouseEvent.MOUSE_CLICK_EVENT_MASK:
	case (int) WCMouseEvent.MOUSE_DOUBLE_CLICK_EVENT_MASK:
	    listener.mouseClicked(event);
	    break;
	case (int) WCMouseEvent.MOUSE_OVER_EVENT_MASK:
	    listener.mouseEntered(event);
	    break;
	case (int) WCMouseEvent.MOUSE_OUT_EVENT_MASK:
	    listener.mouseExited(event);
	    break;
	}
    }

} // end of class BrowserToJavaEventPump

class URIToStringMap extends Object implements Map {
    private Map map = null;

    URIToStringMap(Map yourMap) {
	map = yourMap;
    }

    public String toString() {
	Object result = null;
	if (null == map || null == (result = map.get("URI"))) {
	    result = "";
	}
	return result.toString();
    }
	    
    public void clear() {
	throw new UnsupportedOperationException();
    }

    public boolean containsKey(Object key) {
	return map.containsKey(key);
    }

    public boolean containsValue(Object value) {
	return map.containsValue(value);
    }

    public Set entrySet() {
	return map.entrySet();
    }
    
    public boolean equals(Object o) {
	return map.equals(o);
    }

    public Object get(Object key) {
	return map.get(key);
    }

    public int hashCode() {
	return map.hashCode();
    }

    public boolean isEmpty() {
	return map.isEmpty();
    }

    public Set keySet() {
	return map.keySet();
    }

    public Object put(Object key, Object value) {
	return map.put(key, value);
    }
    
    public void putAll(Map t) {
	map.putAll(t);
    }

    public Object remove(Object key) {
	return map.remove(key);
    }

    public int size() {
	return map.size();
    }
    
    public Collection values() {
	return map.values();
    }
    
}


} // end of class EventRegistrationImpl
