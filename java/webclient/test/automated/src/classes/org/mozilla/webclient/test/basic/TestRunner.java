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
 * TestRunner.java
 */

import java.awt.FlowLayout;
import java.awt.BorderLayout;
import java.awt.Frame;
import java.awt.Color;
import java.awt.TextField;
import java.awt.TextArea;
import java.awt.Button;
import java.awt.Panel;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;
import org.mozilla.webclient.*;
import org.mozilla.util.Assert;


public class TestRunner
{
    static final public int defaultWidth = 640;
    static final public int defaultHeight = 480;
    protected static TestContext context = null;
    protected TestWindow tw = null;
    protected BrowserControlCanvas browserCanvas = null;
    protected BrowserControl browserControl = null;
    protected TestLoader tl = null;
    public TestRunner() 
    {
        try {
            BrowserControlFactory.setAppData(context.getBrowserBinDir());
			browserControl = BrowserControlFactory.newBrowserControl();
            browserCanvas = (BrowserControlCanvas) 
                browserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
        }
        catch(Exception e) {
            System.out.println("Can't create BrowserControl: " + 
                               e.getMessage());
        }
        Assert.assert(null != browserCanvas);
		browserCanvas.setSize(defaultWidth, defaultHeight);
        context.setBrowserControl(browserControl);
        tw = new TestWindow(browserCanvas,"TestWindow", defaultWidth, defaultHeight, this);
        context.setTestWindow(tw);
        tw.pack();
		tw.show();
		tw.toFront();
        tl = new TestLoader();
        tl.initialize(context,this);
        tl.start();
        try {
            CheckingThread checkingThread = 
                new CheckingThread((new Integer(context.getProperty(TestContext.DELAY_INTERNAL_PROP_NAME))).intValue()*1000,this,context);
            checkingThread.start();	
        } catch (Exception e) {
            System.out.println(TestContext.DELAY_INTERNAL_PROP_NAME+" props isn't correctly set");
        }
    }
    public TestRunner(String modifier) 
    {
        System.out.println("Constructor of TestRunnerMixed");
        System.out.flush();
        try {
            BrowserControlFactory.setAppData(context.getBrowserBinDir());
			browserControl = BrowserControlFactory.newBrowserControl();
            browserCanvas = (BrowserControlCanvas) 
                browserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
        }catch(Exception e) {
            System.out.println("Can't create BrowserControl: " + 
                               e.getMessage());
        }
        Assert.assert(null != browserCanvas);
		browserCanvas.setSize(defaultWidth, defaultHeight);
        context.setBrowserControl(browserControl);
        Panel p = new Panel();
        TextArea descrArea = new TextArea("Test info:\n",5,10,TextArea.SCROLLBARS_VERTICAL_ONLY);
        descrArea.setEditable(false);
        descrArea.setBackground(Color.lightGray);
        context.setDescrArea(descrArea);

        Button passed = new Button("PASSED");
        Button failed = new Button("FAILED");
        Button cont = new Button("CONTINUE");
        
	cont.addActionListener(new ActionListener() {
            public void actionPerformed (ActionEvent evt) {
            context.delete();
            delete();
            }
        });
        passed.addActionListener(new ActionListener() {
            public void actionPerformed (ActionEvent evt) {
                context.registerPASSED("PASSED by TESTER");
                context.delete();
                delete();
            }
        });
        failed.addActionListener(new ActionListener() {
            public void actionPerformed (ActionEvent evt) {
                context.registerFAILED("FAILED by TESTER");
                context.delete();
                delete();
            }
        });
        p.setLayout(new BorderLayout());
        Panel buttonPanel = new Panel();
	buttonPanel.setLayout(new FlowLayout());
	context.setButtonPanel(buttonPanel);
        
        buttonPanel.add(passed);
        buttonPanel.add(failed);
	buttonPanel.add(cont);
        buttonPanel.setVisible(true);
        p.setBackground(Color.lightGray);
        p.add(buttonPanel, BorderLayout.EAST);
        p.add(descrArea,BorderLayout.CENTER);
	p.doLayout();
        p.setVisible(true);
        
        tw = new TestWindow(browserCanvas,p,"MixedTestWindow", defaultWidth, defaultHeight, this);
        context.setTestWindow(tw);
        tw.pack();
		tw.show();
		tw.toFront();
        tl = new TestLoader();
        tl.initialize(context,this);
        tl.start();
    }
    public void delete() {
        browserCanvas.setVisible(false);
        System.out.println("before deleteBrowserControl(browserControl)");
        BrowserControlFactory.deleteBrowserControl(browserControl);
        System.out.println("after deleteBrowserControl(browserControl)");
        browserControl = null;
        context=null;
        tw.delete();
        tw = null;
        browserCanvas = null;
        System.exit(0);
    }
    public static void usage()
    {
        System.out.println("usage: java org.mozilla.webclient.test.basic.TestRunner <initFile>");
        System.out.println("       <path> is the absolute path to the native browser bin directory, ");
        System.out.println("       including the bin.");
    }

 //Main procedure
    public static void main(String [] arg)
    { 
        String initFile = null;
        if ( 2 == arg.length ) {
            System.out.println("Length - 2 : " + arg[0]);
            if ("-m".equals(arg[0])) {
                initFile = arg[1];
                context = new TestContext(initFile);
                TestRunner tr = new TestRunner(arg[0]);
                context.setRunner(tr);
            } else {
                usage();
                System.exit(-1);
            }
        } else {
            if ( 1 == arg.length ) {
                initFile = arg[0];
                context = new TestContext(initFile);
                TestRunner tr = new TestRunner();
                context.setRunner(tr);
            } else {
                usage();
                System.exit(-1);
            }
        }
    }
}

// EOF










