/*
 * $Id: NavigationTest.java,v 1.9 2004/06/10 16:30:00 edburns%acm.org Exp $
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

import junit.framework.TestSuite;
import junit.framework.Test;

import java.util.Enumeration;

import java.awt.Frame;
import java.awt.BorderLayout;

import java.io.File;
import java.io.FileInputStream;

// NavigationTest.java

public class NavigationTest extends WebclientTestCase {

    public NavigationTest(String name) {
	super(name);
    }

    public static Test suite() {
	return (new TestSuite(NavigationTest.class));
    }

    //
    // Constants
    // 

    //
    // Testcases
    // 

    public void testNavigation() throws Exception {
	BrowserControl firstBrowserControl = null;
	Selection selection = null;
	BrowserControlFactory.setAppData(getBrowserBinDir());
	firstBrowserControl = BrowserControlFactory.newBrowserControl();
	assertNotNull(firstBrowserControl);
	BrowserControlCanvas canvas = (BrowserControlCanvas)
	    firstBrowserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
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
	CurrentPage2 currentPage = (CurrentPage2) 
          firstBrowserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	
	assertNotNull(currentPage);

	File testPage = new File(getBrowserBinDir(), 
				 "../../java/webclient/test/automated/src/test/NavigationTest.txt");
	
	//
	// try loading a file: url
	//
	System.out.println("Loading url: " + testPage.toURL().toString());
	nav.loadURL(testPage.toURL().toString());
	Thread.currentThread().sleep(1000);

	currentPage.selectAll();
	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("This test file is for the NavigationTest."));
	System.out.println("Selection is: " + selection.toString());

	//
	// try loading from the dreaded RandomHTMLInputStream
	//
	RandomHTMLInputStream rhis = new RandomHTMLInputStream(10, false);
	
	nav.loadFromStreamBlocking(rhis, "http://randomstream.com/",
				   "text/html", -1, null);
	Thread.currentThread().sleep(15000);

	currentPage.selectAll();
	selection = currentPage.getSelection();
	System.out.println("Selection is: " + selection.toString());
	assertTrue(-1 != selection.toString().indexOf("START Random Data"));
	assertTrue(-1 != selection.toString().indexOf("END Random Data"));

	//
	// try loading from a FileInputStream
	//
	FileInputStream fis = new FileInputStream(testPage);
	nav.loadFromStream(fis, "http://somefile.com/",
			   "text/html", -1, null);
	Thread.currentThread().sleep(1000);
	
	currentPage.selectAll();
	selection = currentPage.getSelection();
	assertTrue(-1 != selection.toString().indexOf("This test file is for the NavigationTest."));
	System.out.println("Selection is: " + selection.toString());

	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
	BrowserControlFactory.appTerminate();
    }

}
