/*
 * $Id: DocumentLoadListenerTest.java,v 1.3 2005/02/28 17:15:45 edburns%acm.org Exp $
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
import java.util.BitSet;

import java.awt.Frame;
import java.awt.BorderLayout;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;

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
	final StringBuffer methodToCheck = new StringBuffer();
	final BitSet listenerResult = new BitSet();
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
		    Map map = null;
		    Iterator iter = null;
		    Object requestMethod;
		    if (event instanceof DocumentLoadEvent) {
			switch ((int) event.getType()) {
			case ((int) DocumentLoadEvent.START_URL_LOAD_EVENT_MASK):
			    // do we have eventData?
			    assertNotNull(event.getEventData());
			    // is it a map?
			    assertTrue(event.getEventData() instanceof Map);
			    map = (Map) event.getEventData();
			    // does it have a URI entry?
			    assertEquals(url, map.get("URI"));
			    // does it have a method entry
			    requestMethod = map.get("method");
			    assertNotNull(requestMethod);
			    // is it the expected value?
			    assertEquals(methodToCheck.toString(), 
					 requestMethod.toString());
			    
			    // does it have a headers entry?
			    assertNotNull(map.get("headers"));
			    // is it a map?
			    assertTrue(map.get("headers") instanceof Map);
			    iter = (map = (Map) map.get("headers")).keySet().iterator();
			    boolean hadCorrectHostHeader = false;
			    while (iter.hasNext()) {
				String curName = iter.next().toString();
				if (curName.equals("Host")) {
				    if (-1 != map.get(curName).toString().indexOf("localhost")) {
					hadCorrectHostHeader = true;
				    }
				}
			    }
			    // does it have the correct entry?
			    assertTrue(hadCorrectHostHeader);
			    break;

			case ((int) DocumentLoadEvent.END_URL_LOAD_EVENT_MASK):
			    // do we have eventData?
			    assertNotNull(event.getEventData());
			    // is it a map?
			    assertTrue(event.getEventData() instanceof Map);
			    map = (Map) event.getEventData();
			    // do we have a URI entry?
			    assertEquals(url, map.get("URI"));
			    
			    // do we have a method entry
			    requestMethod = map.get("method");
			    assertNotNull(requestMethod);
			    // is it the expected value
			    assertEquals(methodToCheck.toString(), 
					 requestMethod.toString());

			    if (requestMethod.equals("GET")) {
				// do we have a status entry
				Object responseCode = map.get("status");
				assertNotNull(responseCode);
				assertEquals("200 OK",responseCode.toString());
			    }

			    // do we have a headers entry?
			    assertNotNull(map.get("headers"));
			    assertTrue(map.get("headers") instanceof Map);
			    iter = (map = (Map) map.get("headers")).keySet().iterator();
			    boolean hadCorrectServerHeader = false;
			    while (iter.hasNext()) {
				String curName = iter.next().toString();
				if (curName.equals("Server")) {
				    if (-1 != map.get(curName).toString().indexOf("THTTPD")) {
					hadCorrectServerHeader = true;
				    }
				}
				System.out.println("\t" + curName + 
						   ": " + 
						   map.get(curName));
			    }
			    assertTrue(hadCorrectServerHeader);
			    break;
			case ((int) DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK):
			    // do we have eventData?
			    assertNotNull(event.getEventData());
			    // is it a map?
			    assertTrue(event.getEventData() instanceof Map);
			    map = (Map) event.getEventData();
			    // do we have a URI entry?
			    assertEquals(url, map.get("URI"));
			    
			    // do we have a method entry
			    requestMethod = map.get("method");
			    assertNotNull(requestMethod);
			    // is it the expected value
			    assertEquals(methodToCheck.toString(), 
					 requestMethod.toString());

			    // do we have a status entry
			    Object responseCode = map.get("status");
			    assertNotNull(responseCode);
			    assertEquals("200 OK",responseCode.toString());

			    /**
			    if (requestMethod.equals("POST")) {
				listenerResult.set(0);
				InputStream requestBody = (InputStream)
				    map.get("requestBody");
				assertNotNull(requestBody);
				int avail;
				byte bytes[];
				try {
				    while (0 < (avail = requestBody.available())) {
					bytes = new byte[avail];
					requestBody.read(bytes);
					System.out.write(bytes);
				    }
				}
				catch (java.io.IOException e) {
				    fail();
				}
			    }
			    ***/
			    break;

			}
		    }
		    DocumentLoadListenerTest.keepWaiting = false;
		}
	    });

	Thread.currentThread().sleep(3000);
	
	methodToCheck.replace(0, methodToCheck.length(), "GET");
	System.out.println("++++++++++ Testing GET to " + url);
	nav.loadURL(url);
	
	// keep waiting until the previous load completes
	while (DocumentLoadListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	Thread.currentThread().sleep(3000);

	/**********
	methodToCheck.replace(0, methodToCheck.length(), "POST");
	System.out.println("++++++++++ Testing POST to " + url);
	nav.post(url, null, "PostData\r\n", "X-WakaWaka: true\r\n\r\n");
	
	// keep waiting until the previous load completes
	while (NavigationTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}
	assertTrue(listenerResult.get(0));
	**********/

	eventRegistration.removeDocumentLoadListener(listener);

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

}
