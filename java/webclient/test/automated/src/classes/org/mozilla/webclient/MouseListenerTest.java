/*
 * $Id: MouseListenerTest.java,v 1.2 2004/10/28 13:57:59 edburns%acm.org Exp $
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

// MouseListenerTest.java

public class MouseListenerTest extends WebclientTestCase {

    public MouseListenerTest(String name) {
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
	result.addTestSuite(MouseListenerTest.class);
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

    public void testListenerAddedToEventRegistration() throws Exception {
	doTest(false);
    }

    public void testListenerAddedToCanvas() throws Exception {
	doTest(true);
    }
    public void doTest(boolean addToCanvas) throws Exception {
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
		    MouseListenerTest.keepWaiting = false;
		}
	    });
	final BitSet bitSet = new BitSet();
	
	// PENDING(edburns): flesh this out with more content
	MouseListener mouseListener = new MouseListener() {
		public void mouseEntered(MouseEvent e) {
		    assertEquals(IN_X, e.getX());
		    assertEquals(IN_Y, e.getY());

		    assertTrue(e instanceof WCMouseEvent);
		    WCMouseEvent wcMouseEvent = (WCMouseEvent) e;
		    Map eventMap =
			(Map) wcMouseEvent.getWebclientEvent().getEventData();
		    assertNotNull(eventMap);

		    String href = (String) eventMap.get("href");
		    assertNotNull(href);
		    assertEquals(href, "HistoryTest1.html");
		    bitSet.set(0);
		}
		public void mouseExited(MouseEvent e) {
		    System.out.println("debug: edburns: exited: " + 
				       e.getX() + ", " + e.getY());
		    bitSet.set(1);
		}
		public void mouseClicked(MouseEvent e) {
		    System.out.println("debug: edburns: clicked: " + 
				       e.getX() + ", " + e.getY());
		    bitSet.set(2);
		}
		public void mousePressed(MouseEvent e) {
		    System.out.println("debug: edburns: pressed: " + 
				       e.getX() + ", " + e.getY());
		    bitSet.set(3);
		}
		public void mouseReleased(MouseEvent e) {
		    System.out.println("debug: edburns: released: " + 
				       e.getX() + ", " + e.getY());
		    bitSet.set(4);
		}
	    };
	
	if (addToCanvas) {
	    canvas.addMouseListener(mouseListener);
	}
	else {
	    eventRegistration.addMouseListener(mouseListener);
	}
	
	Thread.currentThread().sleep(3000);
	

	//
	// load four files.
	//
	MouseListenerTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/HistoryTest0.html");
	
	// keep waiting until the previous load completes
	while (MouseListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	Robot robot = new Robot();
	
	robot.mouseMove(IN_X, IN_Y);
	robot.mousePress(InputEvent.BUTTON1_MASK);
	robot.mouseRelease(InputEvent.BUTTON1_MASK);


	MouseListenerTest.keepWaiting = true;
	while (MouseListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	robot.mouseMove(OUT_X, OUT_Y);

	Thread.currentThread().sleep(3000);

	bitSet.flip(0, bitSet.size());
	assertTrue(!bitSet.isEmpty());

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    

}
