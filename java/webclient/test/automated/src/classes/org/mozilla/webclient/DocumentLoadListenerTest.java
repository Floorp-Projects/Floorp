/*
 * $Id: DocumentLoadListenerTest.java,v 1.5 2006/03/17 00:26:02 edburns%acm.org Exp $
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

import java.awt.Robot;
import java.awt.event.InputEvent;


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

    static final int IN_X = 20;
    static final int IN_Y = 117;

    static final int OUT_X = 700;
    static final int OUT_Y = 500;

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

    public static final int HAS_START_EVENT_DATA =                 0;
    public static final int HAS_END_EVENT_DATA =                   1;
    public static final int START_EVENT_DATA_IS_MAP =              2;
    public static final int END_EVENT_DATA_IS_MAP =                3;
    public static final int START_HAS_REQUEST_METHOD =             4;
    public static final int END_HAS_REQUEST_METHOD =               5;
    public static final int START_REQUEST_METHOD_IS_CORRECT =      6;
    public static final int END_REQUEST_METHOD_IS_CORRECT =        7;
    public static final int START_URI_IS_CORRECT =                 8;
    public static final int END_URI_IS_CORRECT =                   9;
    public static final int HAS_REQUEST_HEADERS =                 10;
    public static final int HAS_RESPONSE_HEADERS =                12;
    public static final int REQUEST_HEADERS_IS_MAP =              13;
    public static final int RESPONSE_HEADERS_IS_MAP =             14;
    public static final int REQUEST_HEADERS_ARE_CORRECT =         15;
    public static final int RESPONSE_HEADERS_ARE_CORRECT =        16;
    public static final int HAS_POST_BODY =                       17;
    public static final int HAS_RESPONSE_CODE =                   18;
    public static final int RESPONSE_CODE_IS_CORRECT =            19;
    public static final int END_DOCUMENT_URI_IS_CORRECT =         20;

    public void testPageInfoListener() throws Exception {
	BrowserControl firstBrowserControl = null;
	DocumentLoadListener listener = null;
	Selection selection = null;
	final StringBuffer methodToCheck = new StringBuffer();
	final StringBuffer url = new StringBuffer("http://localhost:5243/HttpNavigationTest.txt");
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
	
	eventRegistration.addDocumentLoadListener(listener = new PageInfoListener() {
		public void eventDispatched(WebclientEvent event) {
		    Map map = null;
		    Iterator iter = null;
		    Object requestMethod;
		    Object responseCode;
		    if (event instanceof DocumentLoadEvent) {
			switch ((int) event.getType()) {
			case ((int) DocumentLoadEvent.START_URL_LOAD_EVENT_MASK):
			    // do we have eventData?
			    listenerResult.set(HAS_START_EVENT_DATA,
					       null != event.getEventData());
			    // is it a map?
			    listenerResult.set(START_EVENT_DATA_IS_MAP,
					       event.getEventData() instanceof 
					       Map);
			    map = (Map) event.getEventData();

			    // does it have a method entry
			    requestMethod = map.get("method");
			    listenerResult.set(START_HAS_REQUEST_METHOD,
					       null != requestMethod);

			    // is it the expected value?
			    listenerResult.set(START_REQUEST_METHOD_IS_CORRECT,
					       methodToCheck.toString().equals(requestMethod.toString()));

			    System.out.println("START_URL_LOAD: request method: " + 
					       requestMethod);

			    // does it have a URI entry?
			    listenerResult.set(START_URI_IS_CORRECT,
					       url.toString().equals(map.get("URI")));;
			    
			    // does it have a headers entry?
			    listenerResult.set(HAS_REQUEST_HEADERS,
					       null != map.get("headers"));
			    // is it a map?
			    listenerResult.set(REQUEST_HEADERS_IS_MAP,
					       map.get("headers") instanceof Map);
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
			    listenerResult.set(REQUEST_HEADERS_ARE_CORRECT,
					       hadCorrectHostHeader);

			    break;

			case ((int) DocumentLoadEvent.END_URL_LOAD_EVENT_MASK):
			    // do we have eventData?
			    listenerResult.set(HAS_END_EVENT_DATA,
					       null != event.getEventData());
			    // is it a map?
			    listenerResult.set(END_EVENT_DATA_IS_MAP,
					       event.getEventData() instanceof
					       Map);
			    map = (Map) event.getEventData();

			    // do we have a method entry
			    requestMethod = map.get("method");
			    listenerResult.set(END_HAS_REQUEST_METHOD,
					       null != requestMethod);

			    // is it the expected value
			    listenerResult.set(END_REQUEST_METHOD_IS_CORRECT,
					       methodToCheck.toString().equals(requestMethod.toString()));
			    
			    System.out.println("END_URL_LOAD request method: "
					       + requestMethod);

			    if (requestMethod.equals("POST")) {
				listenerResult.set(0);
				InputStream requestBody = (InputStream)
				    map.get("requestBody");
				listenerResult.set(HAS_POST_BODY,
						   null != requestBody);
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

			    // do we have a URI entry?
			    listenerResult.set(END_URI_IS_CORRECT,
					       url.toString().equals(map.get("URI")));
			    
			    // do we have a status entry
			    responseCode = map.get("status");

			    if (requestMethod.equals("GET")) {
				// do we have a status entry
				listenerResult.set(HAS_RESPONSE_CODE,
						   null != responseCode);
				listenerResult.set(RESPONSE_CODE_IS_CORRECT,
						   responseCode.toString().equals("200 OK"));
			    }

			    // do we have a headers entry?
			    listenerResult.set(HAS_RESPONSE_HEADERS,
					       null != map.get("headers"));
			    listenerResult.set(RESPONSE_HEADERS_IS_MAP,
					       map.get("headers") instanceof Map);
			    iter = (map = (Map) map.get("headers")).keySet().iterator();
			    boolean hadCorrectResponseHeader = false;
			    while (iter.hasNext()) {
				String curName = iter.next().toString();
				if (curName.equals("Server")) {
				    if (-1 != map.get(curName).toString().indexOf("THTTPD")) {
					hadCorrectResponseHeader = true;
				    }
				}
				System.out.println("\t" + curName + 
						   ": " + 
						   map.get(curName));
			    }
			    listenerResult.set(RESPONSE_HEADERS_ARE_CORRECT,
					       hadCorrectResponseHeader);

			    break;
			case ((int) DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK):
			    map = (Map) event.getEventData();
			    // do we have a URI entry?
			    listenerResult.set(END_DOCUMENT_URI_IS_CORRECT,
					       url.toString().equals(map.get("URI")));

                            try {
                                Thread.currentThread().sleep(2000);
                            } catch (InterruptedException ex) {
                                fail();
                            }
			    DocumentLoadListenerTest.keepWaiting = false;
			    break;
			}
		    }
		}
	    });

	Thread.currentThread().sleep(3000);
	
	DocumentLoadListenerTest.keepWaiting = true;
	methodToCheck.replace(0, methodToCheck.length(), "GET");
	System.out.println("++++++++++ Testing GET to " + url);
	listenerResult.clear();
	nav.loadURL(url.toString());

	// keep waiting until the previous load completes
	while (DocumentLoadListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	// START_URL_LOAD tests
	assertTrue(listenerResult.get(HAS_START_EVENT_DATA));
	assertTrue(listenerResult.get(START_EVENT_DATA_IS_MAP));
	assertTrue(listenerResult.get(START_HAS_REQUEST_METHOD));
	assertTrue(listenerResult.get(START_REQUEST_METHOD_IS_CORRECT));
	assertTrue(listenerResult.get(START_URI_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_REQUEST_HEADERS));
	assertTrue(listenerResult.get(REQUEST_HEADERS_IS_MAP));
	assertTrue(listenerResult.get(REQUEST_HEADERS_ARE_CORRECT));

	// END_URL_LOAD tests
	assertTrue(listenerResult.get(HAS_END_EVENT_DATA));
	assertTrue(listenerResult.get(END_EVENT_DATA_IS_MAP));
	assertTrue(listenerResult.get(END_HAS_REQUEST_METHOD));
	assertTrue(listenerResult.get(END_REQUEST_METHOD_IS_CORRECT));
	assertTrue(listenerResult.get(END_URI_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_RESPONSE_CODE));
	assertTrue(listenerResult.get(RESPONSE_CODE_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_RESPONSE_HEADERS));
	assertTrue(listenerResult.get(RESPONSE_HEADERS_IS_MAP));
	assertTrue(listenerResult.get(RESPONSE_HEADERS_ARE_CORRECT));

	// END_DOCUMENT_LOAD tests
	assertTrue(listenerResult.get(END_DOCUMENT_URI_IS_CORRECT));

	Thread.currentThread().sleep(3000);

	DocumentLoadListenerTest.keepWaiting = true;
	url.replace(0, url.length(), 
		    "http://localhost:5243/DocumentLoadListenerTest0.html");
	methodToCheck.replace(0, methodToCheck.length(), "GET");
	listenerResult.clear();
	nav.loadURL(url.toString());

	// keep waiting until the previous load completes
	while (DocumentLoadListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	// START_URL_LOAD tests
	assertTrue(listenerResult.get(HAS_START_EVENT_DATA));
	assertTrue(listenerResult.get(START_EVENT_DATA_IS_MAP));
	assertTrue(listenerResult.get(START_HAS_REQUEST_METHOD));
	assertTrue(listenerResult.get(START_REQUEST_METHOD_IS_CORRECT));
	assertTrue(listenerResult.get(START_URI_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_REQUEST_HEADERS));
	assertTrue(listenerResult.get(REQUEST_HEADERS_IS_MAP));
	assertTrue(listenerResult.get(REQUEST_HEADERS_ARE_CORRECT));

	// END_URL_LOAD tests
	assertTrue(listenerResult.get(HAS_END_EVENT_DATA));
	assertTrue(listenerResult.get(END_EVENT_DATA_IS_MAP));
	assertTrue(listenerResult.get(END_HAS_REQUEST_METHOD));
	assertTrue(listenerResult.get(END_REQUEST_METHOD_IS_CORRECT));
	assertTrue(listenerResult.get(END_URI_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_RESPONSE_CODE));
	assertTrue(listenerResult.get(RESPONSE_CODE_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_RESPONSE_HEADERS));
	assertTrue(listenerResult.get(RESPONSE_HEADERS_IS_MAP));
	assertTrue(listenerResult.get(RESPONSE_HEADERS_ARE_CORRECT));

	// END_DOCUMENT_LOAD tests
	assertTrue(listenerResult.get(END_DOCUMENT_URI_IS_CORRECT));
	
	DocumentLoadListenerTest.keepWaiting = true;

	System.out.println("++++++++++ Testing POST to " + url);

	// press the submit button
	listenerResult.clear();
	url.replace(0, url.length(), 
		    "http://localhost:5243/HistoryTest0.html");
	methodToCheck.replace(0, methodToCheck.length(), "POST");

        /******************
	Robot robot = new Robot();
	robot.mouseMove(IN_X, IN_Y);
	robot.mousePress(InputEvent.BUTTON1_MASK);
	robot.mouseRelease(InputEvent.BUTTON1_MASK);

	// keep waiting until the previous load completes
	while (DocumentLoadListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	// START_URL_LOAD tests
	assertTrue(listenerResult.get(HAS_START_EVENT_DATA));
	assertTrue(listenerResult.get(START_EVENT_DATA_IS_MAP));
	assertTrue(listenerResult.get(START_HAS_REQUEST_METHOD));
	assertTrue(listenerResult.get(START_REQUEST_METHOD_IS_CORRECT));
	assertTrue(listenerResult.get(START_URI_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_REQUEST_HEADERS));
	assertTrue(listenerResult.get(REQUEST_HEADERS_IS_MAP));
	assertTrue(listenerResult.get(REQUEST_HEADERS_ARE_CORRECT));

	// END_URL_LOAD tests
	assertTrue(listenerResult.get(HAS_END_EVENT_DATA));
	assertTrue(listenerResult.get(END_EVENT_DATA_IS_MAP));
	assertTrue(listenerResult.get(END_HAS_REQUEST_METHOD));
	assertTrue(listenerResult.get(END_REQUEST_METHOD_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_POST_BODY));
	assertTrue(listenerResult.get(END_URI_IS_CORRECT));
	// For some reason, there is no response code when the method is POST
	// assertTrue(listenerResult.get(HAS_RESPONSE_CODE));
	// assertTrue(listenerResult.get(RESPONSE_CODE_IS_CORRECT));
	assertTrue(listenerResult.get(HAS_RESPONSE_HEADERS));
	assertTrue(listenerResult.get(RESPONSE_HEADERS_IS_MAP));
	// For some reason, no response headers when the method is POST
	// assertTrue(listenerResult.get(RESPONSE_HEADERS_ARE_CORRECT));

	// END_DOCUMENT_LOAD tests
	assertTrue(listenerResult.get(END_DOCUMENT_URI_IS_CORRECT));
	
*******************/

	eventRegistration.removeDocumentLoadListener(listener);

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

}
