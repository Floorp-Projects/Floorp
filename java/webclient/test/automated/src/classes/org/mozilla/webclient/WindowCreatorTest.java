/*
 * $Id: WindowCreatorTest.java,v 1.1 2005/01/11 07:02:12 edburns%acm.org Exp $
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

import junit.framework.TestSuite;
import junit.framework.TestResult;
import junit.framework.Test;

import java.util.Enumeration;
import java.util.Map;
import java.util.BitSet;

import java.awt.Frame;
import java.awt.Robot;
import java.awt.event.MouseListener;
import java.awt.event.MouseEvent;
import java.awt.event.InputEvent;
import java.awt.BorderLayout;

import java.io.File;
import java.io.FileInputStream;

// WindowCreatorTest.java

public class WindowCreatorTest extends WebclientTestCase {

    public WindowCreatorTest(String name) {
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
	result.addTestSuite(WindowCreatorTest.class);
	return (result);
    }

    static final int IN_X = 20;
    static final int IN_Y = 117;

    static final int OUT_X = 700;
    static final int OUT_Y = 500;

    static EventRegistration2 eventRegistration;

    static CurrentPage2 currentPage = null;

    static boolean keepWaiting;

    //
    // Constants
    // 

    //
    // Testcases
    // 

    public void testNewWindow() throws Exception {
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
		    WindowCreatorTest.keepWaiting = false;
		}
	    });

	final BitSet bitSet = new BitSet();

	eventRegistration.addNewWindowListener(new NewWindowListener() {
		public void eventDispatched(WebclientEvent event) {
		    bitSet.set(0);
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	//
	// load four files.
	//
	WindowCreatorTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/WindowCreatorTest0.html");
	
	// keep waiting until the previous load completes
	while (WindowCreatorTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	Robot robot = new Robot();
	
	robot.mouseMove(IN_X, IN_Y);
	robot.mousePress(InputEvent.BUTTON1_MASK);
	robot.mouseRelease(InputEvent.BUTTON1_MASK);

	Thread.currentThread().sleep(3000);

	assertTrue(!bitSet.isEmpty());

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    

}
