/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Client QA Team, St. Petersburg, Russia
 */


 
package org.mozilla.webclient.test.basic;

/*
 * TestWindow.java
 */


import java.awt.BorderLayout;
import java.awt.Frame;
import java.awt.TextField;
import java.awt.Panel;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;
import org.mozilla.webclient.*;


public class TestWindow extends Frame {
    private TestRunner      runner;
    private Panel           controlPanel;

    //Default constructor for automated tests.
    public TestWindow (BrowserControlCanvas browserCanvas,String title, int width, int height, TestRunner runner) 
    {
        super(title);
        try {
            throw(new Exception());
        }catch(Exception e) {
            e.printStackTrace();
        }
        System.out.println("Constructor of (AUTOMATED)TestWindow: "+ title);
        this.runner = runner;
        this.add(browserCanvas,BorderLayout.CENTER);
        this.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent e) {
                System.out.println("Got windowClosing");
                System.out.println("destroying the BrowserControl");
                // should close the BrowserControlCanvas
            }
		    
            public void windowClosed(WindowEvent e) { 
                System.out.println("Got windowClosed");
            }
        });
        
    }
    //Constructor for mixed tests
    public TestWindow (BrowserControlCanvas browserCanvas, Panel controlPanel, String title, int width, int height, TestRunner runner) 
    {
        super(title);
        System.out.println("Constructor of TestWindow:  " + title);
        this.runner = runner;
        this.controlPanel = controlPanel;
        this.setLayout(new BorderLayout());
        this.add(browserCanvas,BorderLayout.CENTER);
        this.add(controlPanel,BorderLayout.SOUTH);
        //pack();
        //show();
        this.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent e) {
                System.out.println("Got windowClosing");
                System.out.println("destroying the BrowserControl");
                // should close the BrowserControlCanvas
            }
		    
            public void windowClosed(WindowEvent e) { 
                System.out.println("Got windowClosed");
            }
        });
        
    }
    public void delete()
    {
        this.hide();
        this.dispose();
        controlPanel = null;
    }

}//End of class test window
