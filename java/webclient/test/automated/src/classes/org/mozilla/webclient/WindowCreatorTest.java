/*
 * $Id: WindowCreatorTest.java,v 1.4 2007/01/22 12:35:14 edburns%acm.org Exp $
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

    static boolean keepWaiting;

    //
    // Constants
    // 

    //
    // Testcases
    // 

    public void testNewWindow() throws Exception {
	BrowserControl firstBrowserControl = null;
	final DocumentLoadListenerImpl listener = 
	    new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    WindowCreatorTest.keepWaiting = false;
		}
	    };
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

	eventRegistration.addDocumentLoadListener(listener);

	final BitSet bitSet = new BitSet();
        final WebclientWrapper secondBrowser = new WebclientWrapper();

	eventRegistration.setNewWindowListener(new NewWindowListener() {
		public void eventDispatched(WebclientEvent wcEvent) {
		    bitSet.set(0);
		    NewWindowEvent event = (NewWindowEvent) wcEvent;
		    BrowserControl secondBrowserControl = null;
		    BrowserControlCanvas secondCanvas = null;
		    EventRegistration2 secondEventRegistration = null;
                    CurrentPage2 secondCurrentPage = null;
		    
		    try {
			secondBrowserControl = 
			    BrowserControlFactory.newBrowserControl();
			secondCanvas = (BrowserControlCanvas)
			    secondBrowserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
			secondEventRegistration = 
			    (EventRegistration2)
			    secondBrowserControl.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
			secondCurrentPage = (CurrentPage2) 
			    secondBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
			
			assertNotNull(secondCurrentPage);

		    } catch (Throwable e) {
			System.out.println(e.getMessage());
			fail();
		    }
		    secondEventRegistration.addDocumentLoadListener(new DocumentLoadListenerImpl() {
                        public void doEndCheck() {
                            secondBrowser.setKeepWaiting(false);
                        }
                    });
		    event.setBrowserControl(secondBrowserControl);
                    
                    // Pass the content of the new window back to the 
                    // main Thread.
                    secondBrowser.setBrowserControl(secondBrowserControl);
                    secondBrowser.setCanvas(secondCanvas);
                    secondBrowser.setCurrentPage(secondCurrentPage);
                    secondBrowser.setEventRegistration(secondEventRegistration);
                    secondBrowser.setKeepWaiting(false);


		}
	    });
	
	//
	// load a file that pops up a new window on link click
	//
	WindowCreatorTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/WindowCreatorTest0.html");

	// keep waiting until the previous load completes
	while (WindowCreatorTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}
        
        secondBrowser.setKeepWaiting(true);

	Robot robot = new Robot();
	
	robot.mouseMove(IN_X, IN_Y);
	robot.mousePress(InputEvent.BUTTON1_MASK);
	robot.mouseRelease(InputEvent.BUTTON1_MASK);
        
	// keep waiting until the second window is ready to make visible
	while (secondBrowser.isKeepWaiting()) {
	    Thread.currentThread().sleep(1000);
	}
        
        Frame newFrame = new Frame();
        newFrame.setUndecorated(true);
        newFrame.setBounds(100, 100, 540, 380);
        newFrame.add(secondBrowser.getCanvas(), BorderLayout.CENTER);
        newFrame.setVisible(true);
        secondBrowser.getCanvas().setVisible(true);

        assertTrue(!bitSet.isEmpty());
        
	assertNotNull(secondBrowser.getCurrentPage());
	secondBrowser.getCurrentPage().selectAll();
	selection = secondBrowser.getCurrentPage().getSelection();
	assertTrue(-1 !=selection.toString().indexOf("This is page 1 of the WindowCreatorTest."));
        
        newFrame.setVisible(false);
        BrowserControlFactory.deleteBrowserControl(secondBrowser.getBrowserControl());

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    

}

class WebclientWrapper {
    private boolean keepWaiting = true;
    private BrowserControl browserControl = null;
    private BrowserControlCanvas canvas = null;
    private CurrentPage2 currentPage = null;
    private EventRegistration2 eventRegistration = null;

    public BrowserControl getBrowserControl() {
        return browserControl;
    }

    public void setBrowserControl(BrowserControl browserControl) {
        this.browserControl = browserControl;
    }

    public BrowserControlCanvas getCanvas() {
        return canvas;
    }

    public void setCanvas(BrowserControlCanvas canvas) {
        this.canvas = canvas;
    }

    public CurrentPage2 getCurrentPage() {
        return currentPage;
    }

    public void setCurrentPage(CurrentPage2 currentPage) {
        this.currentPage = currentPage;
    }

    public EventRegistration2 getEventRegistration() {
        return eventRegistration;
    }

    public void setEventRegistration(EventRegistration2 eventRegistration) {
        this.eventRegistration = eventRegistration;
    }

    public boolean isKeepWaiting() {
        return keepWaiting;
    }

    public void setKeepWaiting(boolean keepWaiting) {
        this.keepWaiting = keepWaiting;
    }

}

