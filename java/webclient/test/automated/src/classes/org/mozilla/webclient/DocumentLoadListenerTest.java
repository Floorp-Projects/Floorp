/*
 * $Id: DocumentLoadListenerTest.java,v 1.1 2004/09/09 20:17:17 edburns%acm.org Exp $
 */

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns &lt;edburns@acm.org&gt;
 */

package org.mozilla.webclient;

import junit.framework.Test;
import junit.framework.TestSuite;

import java.util.Iterator;
import java.util.Map;

import java.awt.Frame;
import java.awt.BorderLayout;

import java.io.File;
import java.io.FileInputStream;

// DocumentLoadListenerTest.java

public class DocumentLoadListenerTest extends WebclientTestCase {

    public DocumentLoadListenerTest(String name) {
 	super(name);
	try {
	    BrowserControlFactory.setAppData(getBrowserBinDir());
	}
	catch (Exception e) {
	    fail();
	}
    }

    public static Test suite() {
	TestSuite result = createServerTestSuite();
	result.addTestSuite(DocumentLoadListenerTest.class);
	return (result);
    }

    static EventRegistration2 eventRegistration;

    static boolean keepWaiting;

    //
    // Constants
    // 

    //
    // Testcases
    // 

    public void testDocumentLoadListener() throws Exception {
	BrowserControl firstBrowserControl = null;
	DocumentLoadListener listener = null;
	Selection selection = null;
	firstBrowserControl = BrowserControlFactory.newBrowserControl();
	assertNotNull(firstBrowserControl);
	BrowserControlCanvas canvas = (BrowserControlCanvas)
	    firstBrowserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
	eventRegistration = (EventRegistration2)
	    firstBrowserControl.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);

	assertNotNull(canvas);
	Frame frame = new Frame();
	frame.setUndecorated(true);
	frame.setBounds(0, 0, 640, 480);
	frame.add(canvas, BorderLayout.CENTER);
	frame.setVisible(true);
	canvas.setVisible(true);
	
	Navigation2 nav = (Navigation2) 
	    firstBrowserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
	assertNotNull(nav);

	//
	// try loading a file over HTTP
	//
	
	DocumentLoadListenerTest.keepWaiting = true;
	final String url = "http://localhost:5243/HttpNavigationTest.txt";

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListener() {
		public void eventDispatched(WebclientEvent event) {
		    if (event instanceof DocumentLoadEvent) {
			switch ((int) event.getType()) {
			case ((int) DocumentLoadEvent.END_URL_LOAD_EVENT_MASK):
			    assertNotNull(event.getEventData());
			    // PENDING(edburns): assertEquals(url, event.getEventData().toString());
			    assertTrue(event.getEventData() instanceof Map);
			    Map map = (Map) event.getEventData();
			    assertNull(map.get("headers"));
			    assertEquals(url, map.get("URI"));
			    break;
			}
		    }
		    DocumentLoadListenerTest.keepWaiting = false;
		}
	    });

	Thread.currentThread().sleep(3000);
	
	nav.loadURL(url);
	
	// keep waiting until the previous load completes
	while (DocumentLoadListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}
	eventRegistration.removeDocumentLoadListener(listener);

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    public void testPageInfoListener() throws Exception {
	BrowserControl firstBrowserControl = null;
	DocumentLoadListener listener = null;
	Selection selection = null;
	firstBrowserControl = BrowserControlFactory.newBrowserControl();
	assertNotNull(firstBrowserControl);
	BrowserControlCanvas canvas = (BrowserControlCanvas)
	    firstBrowserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
	eventRegistration = (EventRegistration2)
	    firstBrowserControl.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);

	assertNotNull(canvas);
	Frame frame = new Frame();
	frame.setUndecorated(true);
	frame.setBounds(0, 0, 640, 480);
	frame.add(canvas, BorderLayout.CENTER);
	frame.setVisible(true);
	canvas.setVisible(true);
	
	Navigation2 nav = (Navigation2) 
	    firstBrowserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
	assertNotNull(nav);

	//
	// try loading a file over HTTP
	//
	
	DocumentLoadListenerTest.keepWaiting = true;
	final String url = "http://localhost:5243/HttpNavigationTest.txt";

	eventRegistration.addDocumentLoadListener(listener = new PageInfoListener() {
		public void eventDispatched(WebclientEvent event) {
		    if (event instanceof DocumentLoadEvent) {
			switch ((int) event.getType()) {
			case ((int) DocumentLoadEvent.END_URL_LOAD_EVENT_MASK):
			    assertNotNull(event.getEventData());
			    assertTrue(event.getEventData() instanceof Map);
			    Map map = (Map) event.getEventData();
			    assertEquals(url, map.get("URI"));
			    assertNotNull(map.get("headers"));
			    assertTrue(map.get("headers") instanceof Map);
			    Iterator iter = (map = (Map) map.get("headers")).keySet().iterator();
			    boolean hadCorrectServerHeader = false;
			    while (iter.hasNext()) {
				String curName = iter.next().toString();
				if (curName.equals("Server")) {
				    hadCorrectServerHeader = true;
				}
				System.out.println("\t" + curName + 
						   ": " + 
						   map.get(curName));
			    }
			    assertTrue(hadCorrectServerHeader);
			    break;
			}
		    }
		    DocumentLoadListenerTest.keepWaiting = false;
		}
	    });

	Thread.currentThread().sleep(3000);
	
	nav.loadURL(url);
	
	// keep waiting until the previous load completes
	while (DocumentLoadListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}
	eventRegistration.removeDocumentLoadListener(listener);

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

}
