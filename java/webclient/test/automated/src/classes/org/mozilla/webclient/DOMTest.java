/*
 * $Id: DOMTest.java,v 1.1 2005/02/04 15:43:47 edburns%acm.org Exp $
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

import java.util.Enumeration;

import java.awt.Frame;
import java.awt.BorderLayout;
import java.awt.Robot;

import java.io.File;
import java.io.FileInputStream;

import org.w3c.dom.Document;
import org.w3c.dom.Node;

// DOMTest.java

public class DOMTest extends WebclientTestCase {

    public DOMTest(String name) {
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
	result.addTestSuite(DOMTest.class);
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

    public void testHttpLoad() throws Exception {
	BrowserControl firstBrowserControl = null;
	DocumentLoadListenerImpl listener = null;
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
	final CurrentPage2 currentPage = (CurrentPage2) 
          firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	//
	// try loading a file over HTTP
	//
	
	DOMTest.keepWaiting = true;
	

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    DOMTest.keepWaiting = false;
		}
	    });

	String url = "http://localhost:5243/HistoryTest0.html";

	Thread.currentThread().sleep(3000);
	
	nav.loadURL(url);
	
	// keep waiting until the previous load completes
	while (DOMTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	Document dom = currentPage.getDOM();
	assertNotNull(dom);

	Node node = dom.getElementById("HistoryTest1.html");
	assertNotNull(node);
	assertEquals(1, node.getNodeType());
	node = node.getFirstChild();
	assertEquals("next", node.getNodeValue());

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

}
