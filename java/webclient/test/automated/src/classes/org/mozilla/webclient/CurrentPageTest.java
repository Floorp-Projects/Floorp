/*
 * $Id: CurrentPageTest.java,v 1.11 2005/03/29 05:03:12 edburns%acm.org Exp $
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

import java.awt.Frame;
import java.awt.BorderLayout;
import java.awt.Toolkit;
import java.awt.datatransfer.Clipboard;
import java.awt.datatransfer.ClipboardOwner;
import java.awt.datatransfer.Transferable;
import java.awt.datatransfer.DataFlavor;

import java.io.File;
import java.io.FileInputStream;
import java.io.BufferedReader;

import org.w3c.dom.Document;
import org.w3c.dom.Node;

// CurrentPageTest.java

public class CurrentPageTest extends WebclientTestCase implements ClipboardOwner {

    public CurrentPageTest(String name) {
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
	result.addTestSuite(CurrentPageTest.class);
	return (result);
    }

    static EventRegistration2 eventRegistration;

    static CurrentPage2 currentPage = null;

    static boolean keepWaiting;

    //
    // Constants
    // 

    // 
    // ClipboardOwner
    //


    public void lostOwnership(Clipboard clipboard,
			      Transferable contents) {
    }

    //
    // Testcases
    // 

    public void testCopyCurrentSelectionToSystemClipboard() throws Exception {
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
	currentPage = (CurrentPage2) 
	  firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    CurrentPageTest.keepWaiting = false;
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	CurrentPageTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/HistoryTest0.html");
	
	// keep waiting until the previous load completes
	while (CurrentPageTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	CurrentPageTest.keepWaiting = true;
	Clipboard clipboard = Toolkit.getDefaultToolkit().getSystemClipboard();
	Transferable contents;

	Thread.currentThread().sleep(3000);
	currentPage.selectAll();
	currentPage.copyCurrentSelectionToSystemClipboard();
	contents = clipboard.getContents(this);
	assertNotNull(contents);

	DataFlavor bestTextFlavor = DataFlavor.getTextPlainUnicodeFlavor();
	BufferedReader clipReader = 
	    new BufferedReader(bestTextFlavor.getReaderForText(contents));
	String contentLine;
	StringBuffer buf = new StringBuffer();
	while (null != (contentLine = clipReader.readLine())) {
	    buf.append(contentLine);
	    System.out.println(contentLine);
	}
	assertEquals("HistoryTest0This is page 0 of the history test.next",
		     buf.toString());

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    public void testGetCurrentURL() throws Exception {
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
	currentPage = (CurrentPage2) 
	  firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    CurrentPageTest.keepWaiting = false;
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	//
	// load four files.
	//
	CurrentPageTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/HistoryTest0.html");
	
	// keep waiting until the previous load completes
	while (CurrentPageTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	String url = currentPage.getCurrentURL();
	assertEquals("http://localhost:5243/HistoryTest0.html",
		     currentPage.getCurrentURL());
	
	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    public void testFind() throws Exception {
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
	currentPage = (CurrentPage2) 
	  firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    CurrentPageTest.keepWaiting = false;
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	//
	// load four files.
	//
	CurrentPageTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/FindTest0.html");
	
	// keep waiting until the previous load completes
	while (CurrentPageTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	currentPage.resetFind();
	assertTrue(currentPage.find("one", true, false));

	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("one"));

	assertTrue(currentPage.findNext());
		   
	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("one"));

	assertTrue(currentPage.findNext());

	assertTrue(!currentPage.findNext());

	assertTrue(currentPage.find("one", false, false));
	assertTrue(currentPage.findNext());

	assertTrue(currentPage.find("three", true, false));
	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("Three"));
	assertTrue(currentPage.findNext());
	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("Three"));
	assertTrue(currentPage.findNext());
	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("three"));

	assertTrue(currentPage.find("Three", false, true));
	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("Three"));
	assertTrue(currentPage.findNext());
	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("Three"));
	
	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    public void testHighlightDomRegion() throws Exception {
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
	currentPage = (CurrentPage2) 
	  firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    CurrentPageTest.keepWaiting = false;
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	CurrentPageTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/DOMSelectionTest.html");
	
	// keep waiting until the previous load completes
	while (CurrentPageTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	Document dom = currentPage.getDOM();
	assertNotNull(dom);

	Node 
	    start = dom.getElementById("p2"),
	    end = dom.getElementById("p4");
	assertNotNull(start);
	assertNotNull(end);
	
	selection = currentPage.getSelection();
	selection.init("", start, end, 0, 0);
	// select Paragraphs 2 - 4 exclusive
	currentPage.highlightSelection(selection);

	Thread.currentThread().sleep(3000); // PENDING remove

	selection = currentPage.getSelection();
	assertTrue(-1 == selection.toString().indexOf("Paragraph 1"));
	assertTrue(-1 != selection.toString().indexOf("Paragraph 2"));
	assertTrue(-1 != selection.toString().indexOf("Paragraph 3"));
	assertTrue(-1 == selection.toString().indexOf("Paragraph 4"));
	assertTrue(-1 == selection.toString().indexOf("Paragraph 5"));	
	
	currentPage.clearAllSelections();
	selection = currentPage.getSelection();
	assertEquals(0, selection.toString().length());

	
	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    public void NoIdeaHowToTestPrintingUsingJunit() throws Exception {
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
	currentPage = (CurrentPage2) 
	  firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    CurrentPageTest.keepWaiting = false;
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	//
	// load four files.
	//
	CurrentPageTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/HistoryTest0.html");
	
	// keep waiting until the previous load completes
	while (CurrentPageTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	currentPage.print();

	Thread.currentThread().sleep(10000);
	
	
	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    public void testPrintPreview() throws Exception {
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
	currentPage = (CurrentPage2) 
	  firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    CurrentPageTest.keepWaiting = false;
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	//
	// load four files.
	//
	CurrentPageTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/HistoryTest0.html");
	
	// keep waiting until the previous load completes
	while (CurrentPageTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	currentPage.printPreview(true);

	Thread.currentThread().sleep(3000);

	currentPage.printPreview(false);
	
	
	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    public void NOTtestGetSource() throws Exception {
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
	currentPage = (CurrentPage2) 
	  firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	eventRegistration.addDocumentLoadListener(listener = new DocumentLoadListenerImpl() {
		public void doEndCheck() {
		    CurrentPageTest.keepWaiting = false;
		}
	    });
	
	Thread.currentThread().sleep(3000);
	

	CurrentPageTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/ViewSourceTest.html");
	
	// keep waiting until the previous load completes
	while (CurrentPageTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	String source = currentPage.getSource();
	assertTrue(-1 != source.indexOf("<HTML>"));
	assertTrue(-1 != source.indexOf("</HTML>"));
		   
	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

}
