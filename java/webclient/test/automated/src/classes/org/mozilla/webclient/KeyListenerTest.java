/*
 * $Id: KeyListenerTest.java,v 1.3 2005/08/20 19:25:52 edburns%acm.org Exp $
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
import junit.framework.Test;
import java.util.BitSet;

import java.awt.Frame;
import java.awt.Robot;
import java.awt.event.KeyListener;
import java.awt.event.KeyEvent;
import java.awt.event.InputEvent;
import java.awt.BorderLayout;
import org.w3c.dom.Element;
import org.w3c.dom.Node;


// KeyListenerTest.java

public class KeyListenerTest extends WebclientTestCase {

    public KeyListenerTest(String name) {
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
	result.addTestSuite(KeyListenerTest.class);
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
		    KeyListenerTest.keepWaiting = false;
		}
	    });
	final BitSet bitSet = new BitSet();

	KeyListener manualExitKeyListener = new KeyListener() {
		public void keyPressed(KeyEvent e) {
		}
		public void keyReleased(KeyEvent e) {
		}
		public void keyTyped(KeyEvent e) {
		    if ('z' == e.getKeyChar()) {
			KeyListenerTest.keepWaiting = false;
		    }
                    
		}
	    };
	canvas.addKeyListener(manualExitKeyListener);
	
	KeyListener keyListener = new KeyListener() {
		public void keyPressed(KeyEvent e) {
		    System.out.println("Key Pressed");
		    bitSet.set(0);
		}
		public void keyReleased(KeyEvent e) {
		    System.out.println("Key Released");
		    bitSet.set(1);
		}
		public void keyTyped(KeyEvent e) {
		    System.out.println("Key Pressed");
                    assertTrue(e instanceof WCKeyEvent);
                    WCKeyEvent wcKeyEvent = (WCKeyEvent) e;
                    WebclientEvent wcEvent = wcKeyEvent.getWebclientEvent();
                    assertNotNull(wcEvent);
                    Node domNode = (Node) wcEvent.getSource();
                    assertNotNull(domNode);
                    assertTrue(domNode instanceof Element);
                    Element element = (Element) domNode;
                    String 
                        id = element.getAttribute("id"),
                        name = element.getAttribute("name"),
                        nodeName = domNode.getNodeName(),
                        value = domNode.getNodeValue();
                    assertEquals("field1", id);
                    assertEquals("field1", name);
                    assertEquals("INPUT", nodeName);
                    assertEquals("", value);
                    assertEquals('a', e.getKeyChar());
                                        
		    System.out.println("Key Typed");
		    bitSet.set(2);
		}
	    };
	
	canvas.addKeyListener(keyListener);
	
	Thread.currentThread().sleep(3000);
	

	//
	// load four files.
	//
	KeyListenerTest.keepWaiting = true;

	nav.loadURL("http://localhost:5243/KeyListenerTest1.html");
	
	// keep waiting until the previous load completes
	while (KeyListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}

	Robot robot = new Robot();
	
	robot.mouseMove(IN_X, IN_Y);
	robot.mousePress(InputEvent.BUTTON1_MASK);
	robot.mouseRelease(InputEvent.BUTTON1_MASK);


	// uncomment to enable manual testing
	/*************
	KeyListenerTest.keepWaiting = true;
	
	// keep waiting until the previous load completes
	while (KeyListenerTest.keepWaiting) {
	    Thread.currentThread().sleep(1000);
	}
	******************/

	robot.keyPress(KeyEvent.VK_A);
	robot.keyRelease(KeyEvent.VK_A);

	Thread.currentThread().sleep(3000);

	bitSet.flip(0, bitSet.size());
	assertTrue(!bitSet.isEmpty());

	canvas.removeKeyListener(keyListener);

	keyListener = new KeyListener() {
		public void keyPressed(KeyEvent e) {
		    if (e.getKeyCode() == KeyEvent.VK_BACK_SPACE) {
			bitSet.set(0);
		    }
		}
		public void keyReleased(KeyEvent e) {
		    System.out.println("Key Released");
		    if (e.getKeyCode() == KeyEvent.VK_BACK_SPACE) {
			bitSet.set(1);
		    }
		}
		public void keyTyped(KeyEvent e) {
		    System.out.println("Key Typed");
		    bitSet.set(2);
		}
	    };
	
	canvas.addKeyListener(keyListener);
	bitSet.clear(0);
	bitSet.clear(1);
	bitSet.clear(2);

	robot.keyPress(KeyEvent.VK_BACK_SPACE);
	robot.keyRelease(KeyEvent.VK_BACK_SPACE);

	Thread.currentThread().sleep(3000);

	assertTrue(bitSet.get(0));
	assertTrue(bitSet.get(1));
	assertTrue(!bitSet.get(2));


	frame.setVisible(false);
	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
    }

    

}
