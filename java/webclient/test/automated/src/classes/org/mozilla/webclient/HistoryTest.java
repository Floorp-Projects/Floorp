/*
 * $Id: HistoryTest.java,v 1.1 2004/06/23 19:21:06 edburns%acm.org Exp $
 */

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

import org.mozilla.util.THTTPD;

import junit.framework.TestSuite;
import junit.framework.TestResult;
import junit.framework.Test;

import java.util.Enumeration;

import java.awt.Frame;
import java.awt.BorderLayout;

import java.io.File;
import java.io.FileInputStream;

// HistoryTest.java

public class HistoryTest extends WebclientTestCase {

    public HistoryTest(String name) {
 	super(name);
	try {
	    BrowserControlFactory.setAppData(getBrowserBinDir());
	}
	catch (Exception e) {
	    fail();
	}
    }

    public static Test suite() {
	TestSuite result = new TestSuite() {
		public void run(TestResult result) {
		    serverThread = 
			new THTTPD.ServerThread("LocalHTTPD",
						new File (getBrowserBinDir() +
							  "/../../java/webclient/build.test"), -1);
		    serverThread.start();
		    serverThread.P();
		    super.run(result);
		    try {
			BrowserControlFactory.appTerminate();
		    }
		    catch (Exception e) {
			fail();
		    }
		    serverThread.stopRunning();
		}
	    };
	result.addTestSuite(HistoryTest.class);
	return (result);
    }

    static THTTPD.ServerThread serverThread;

    static EventRegistration2 eventRegistration;

    static CurrentPage2 currentPage = null;

    static boolean keepWaiting;

    //
    // Constants
    // 

    //
    // Testcases
    // 

    public void testHistory() throws Exception {
	BrowserControl firstBrowserControl = null;
	DocumentLoadListenerImpl listener = null;
	Selection selection = null;
	firstBrowserControl = BrowserControlFactory.newBrowserControl();
	assertNotNull(firstBrowserControl);
	History history = (History) 
	    firstBrowserControl.queryInterface(BrowserControl.HISTORY_NAME);
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
	currentPage = (CurrentPage2) 
	  firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    HistoryTest.keepWaiting = false;
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	//
	// load four files.
	//
	HistoryTest.keepWaiting = true;
	nav.loadURL("http://localhost:5243/HistoryTest0.html");
	
	// keep waiting until the previous load completes
	while (HistoryTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	HistoryTest.keepWaiting = true;
	nav.loadURL("http://localhost:5243/HistoryTest1.html");
	
	// keep waiting until the previous load completes
	while (HistoryTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	HistoryTest.keepWaiting = true;
	nav.loadURL("http://localhost:5243/HistoryTest2.html");
	
	// keep waiting until the previous load completes
	while (HistoryTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	HistoryTest.keepWaiting = true;
	nav.loadURL("http://localhost:5243/HistoryTest3.html");
	
	// keep waiting until the previous load completes
	while (HistoryTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	eventRegistration.removeDocumentLoadListener(listener);

	//
	// new DocumentLoadListenerImpl that 

	//
	// Step back through each and verify selection
	//
	HistoryVerify historyListener = new HistoryVerify();

	eventRegistration.addDocumentLoadListener(historyListener);
	
	HistoryTest.keepWaiting = true;
	historyListener.setStringToVerify("This is page 2 of the history test.");
	history.back();

	// keep waiting until the previous load completes
	while (HistoryTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	HistoryTest.keepWaiting = true;
	historyListener.setStringToVerify("This is page 1 of the history test.");
	history.back();

	// keep waiting until the previous load completes
	while (HistoryTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	HistoryTest.keepWaiting = true;
	historyListener.setStringToVerify("This is page 0 of the history test.");
	history.back();

	// keep waiting until the previous load completes
	while (HistoryTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	eventRegistration.removeDocumentLoadListener(historyListener);


	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    public static class HistoryVerify extends DocumentLoadListenerImpl {

	protected String stringToVerify = "";
	public String getStringToVerify() {
	    return stringToVerify;
	}
	
	public void setStringToVerify(String newStringToVerify) {
	    stringToVerify = newStringToVerify;
	}

	public void doEndCheck() {
	    currentPage.selectAll();
	    Selection selection = currentPage.getSelection();
	    assertTrue(-1 !=selection.toString().indexOf(getStringToVerify()));
	    System.out.println("Selection is: " + 
			       selection.toString());
	    HistoryTest.keepWaiting = false;
	}
    }
}
