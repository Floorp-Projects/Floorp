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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ashutosh Kulkarni <ashuk@eng.sun.com>
 *                  Ed Burns <edburns@acm.org>
 *
 */



package org.mozilla.webclient.test_nonnative;

/*
 * EMWindow.java
 */

import java.awt.*;
import java.awt.event.*;


import javax.swing.*;
import java.util.Enumeration;
import java.util.Properties;
import java.util.*;

import org.mozilla.webclient.*;
import org.mozilla.util.Assert;

import org.w3c.dom.Document;

import java.io.File;
import java.io.FileInputStream;

import ice.storm.*;
import java.beans.*;


/**
 *

 * This is a test application for using the BrowserControl.

 *
 * @version $Id: EMWindow.java,v 1.1 2001/07/27 21:02:15 ashuk%eng.sun.com Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlFactory

 */

public class EMWindow extends Frame implements ActionListener, WindowListener, PropertyChangeListener {

  private int winNum;

    private TextField		urlField;
	private BrowserControl	browserControl;

    private Navigation navigation = null;
	private CurrentPage	    currentPage;
	private History	        history;
	private WindowControl windowControl;

	private Panel			controlPanel;
	private Panel			statusPanel;
	private Panel			buttonsPanel;
	
	private Panel			panel;

    private Label          statusLabel;
    private Label          urlStatusLabel;
    private String currentURL = null;

    private Component forwardButton;
    private Component backButton;
    private Component stopButton;
    private Component refreshButton;

// Just to have a frame of reference for dialog boxes
	static Frame theFrame=null;

	private StormBase base;
	private String viewportId;
    private Thread statusUpdater;

    private StormBase ICEbase;

  public static void main(String [] arg)
    {
	// Create a StormBase Object
	StormBase iceBase = new StormBase();

	// Init the browser for toolkit
	try {
	    Callback myCallBack = new Callback();
	    StormCallback cb = (StormCallback) myCallBack;
	    iceBase.setCallback(cb);
	}
	catch (Exception e) {
		    System.out.println(e.toString());
		}
	
	iceBase.renderContent("http://sunweb.central.sun.com",null, "Webclient and ICE");
    }	


    public EMWindow (StormBase b, Viewport viewport) 
    {
    super(viewport.getName());
    this.base=b;
    this.viewportId=viewport.getId();
    
    addWindowListener(this);
    base.addPropertyChangeListener(this,viewportId);

    // Create the URL field
    urlField = new TextField("", 30);
    urlField.addActionListener(this);        	
    currentURL = "http://sunweb.central.sun.com/";
//    urlField.setText();

    // Create the buttons sub panel
    buttonsPanel = new Panel();
    buttonsPanel.setLayout(new GridBagLayout());

    // Add the buttons
    backButton = makeItem(buttonsPanel, "Back",    0, 0, 1, 1, 0.0, 0.0);
    backButton.setEnabled(true);
    forwardButton = makeItem(buttonsPanel, "Forward", 1, 0, 1, 1, 0.0, 0.0);
    forwardButton.setEnabled(true);
    stopButton = makeItem(buttonsPanel, "Stop",    2, 0, 1, 1, 0.0, 0.0);
    stopButton.setEnabled(true);
    refreshButton = makeItem(buttonsPanel, "Refresh", 3, 0, 1, 1, 0.0, 0.0);
    refreshButton.setEnabled(true);

    // Create the control panel
    controlPanel = new Panel();
    controlPanel.setLayout(new BorderLayout());
        
    // Add the URL field, and the buttons panel
    Panel centerPanel = new Panel();
    centerPanel.setLayout(new BorderLayout());
    centerPanel.add(urlField, BorderLayout.NORTH);

    //		controlPanel.add(urlField,     BorderLayout.CENTER);
    controlPanel.add(centerPanel,     BorderLayout.CENTER);
    controlPanel.add(buttonsPanel, BorderLayout.WEST);

    // create the status panel
    statusPanel = new Panel();
    statusPanel.setLayout(new BorderLayout());

    // create and add the statusLabel and the urlStatusLabel
    statusLabel = new Label("", Label.LEFT);
    statusLabel.setBackground(Color.lightGray);
    urlStatusLabel = new Label("", Label.LEFT);
    urlStatusLabel.setBackground(Color.lightGray);

    Panel tPanel = new Panel();
    tPanel.setLayout(new BorderLayout());
    tPanel.add(statusLabel, BorderLayout.NORTH);
    tPanel.add(urlStatusLabel, BorderLayout.SOUTH);
        
    statusPanel.add(tPanel, BorderLayout.CENTER);
    add(controlPanel, BorderLayout.NORTH);
    add(statusPanel,      BorderLayout.SOUTH);

    // Create the Webclient objects
    try {
        BrowserControlFactory.setAppData(BrowserControl.BROWSER_TYPE_NON_NATIVE, null);
        browserControl = BrowserControlFactory.newBrowserControl();
	BrowserType browserType = (BrowserType)
            browserControl.queryInterface(BrowserControl.BROWSER_TYPE_NAME);
	browserType.setICEProps((Object) base, (Object) viewport);
    }
    catch(Exception e) {
        System.out.println("test_nonnative: Can't create BrowserControl: " + 
                               e.getMessage());
    }

    panel = new Panel();
    panel.setLayout(new BorderLayout());
    add(panel, BorderLayout.CENTER);
    setSize(600,600);

    try {
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
            currentPage = (CurrentPage)
                browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
            history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);

	    System.setProperty("http.proxyHost", "webcache-cup.eng.sun.com");
	    System.setProperty("http.proxyPort", "8080");

        }
	catch (Exception e) {
		    System.out.println(e.toString());
		    System.out.println("\n\n Something went wrong when getting the Interfaces \n");
		}
    } // EMWindow() ctor


public Container getPanel()
{
    return panel;
}


public void actionPerformed (ActionEvent evt) 
{
    String command = evt.getActionCommand();
    
    try {
        if(command.equals("Stop")) {
            navigation.stop();
        }
        else if (command.equals("Refresh")) {
            navigation.refresh(Navigation.LOAD_NORMAL);
        }
        else if (command.equals("Back")) {
	    System.out.println(" \n At 1 \n");
            if (history.canBack()) {
		System.out.println(" \n At 2 \n");
                history.back();
		System.out.println(" \n At 3 \n");
            }
        }
        else if (command.equals("Forward")) {
            if (history.canForward()) {
                history.forward();
            }
        }
        else if (command.equals(" ")) {
        }
        else {
            navigation.loadURL(urlField.getText());
        }
    }
    catch (Exception e) {
        System.out.println(e.getMessage());
    }
} // actionPerformed()





private Component makeItem (Panel p, Object arg, int x, int y, int w, int h, double weightx, double weighty)
{
     GridBagLayout gbl = (GridBagLayout) p.getLayout();
     GridBagConstraints c = new GridBagConstraints();
     Component comp = null;
     
     c.fill = GridBagConstraints.BOTH;
     c.gridx = x;
     c.gridy = y;
     c.gridwidth = w;
     c.gridheight = h;
     c.weightx = weightx;
     c.weighty = weighty;
     if (arg instanceof String) {
         Button b;
         
         comp = b = new Button((String) arg);
         b.addActionListener(this);
         
         p.add(comp);
         gbl.setConstraints(comp, c);

         if (((String)arg).equals(" ")) {
             b.setEnabled(false);
         }
     }
     return comp;
} // makeItem()


// PropertyChangeListener implementation
public void propertyChange(PropertyChangeEvent e) {
	Viewport v = (Viewport)e.getSource();
	boolean isMyView=v.getId().equals(viewportId);		
	String name = e.getPropertyName();
	String val = "";
	String url = base.getHistoryManager().getCurrentLocation(v.getId());
	urlStatusLabel.setText(url);

	if (e.getNewValue() instanceof String) {
	   val = (String) e.getNewValue();
	}
	if (name.equals("statusLine")) {
	   statusLabel.setText(val);
	}	
	else if (name.equals("outstandingImages")) {
		if (val.equals("0")) {
		   statusLabel.setText("Loading images: done");
		}
		else {
		   statusLabel.setText("loading images: "+val+" left");
		}
	}
	else if (name.equals("contentLoading")) {
		if (val.equals("error")) {
		   ContentLoader cl = (ContentLoader)e.getOldValue();
		   if (cl != null) {
			statusLabel.setText(v.getName()+": "+cl.getException());
		   }
		   else {
			statusLabel.setText(v.getName()+": loading error");
		   }
		}
	}	
}

// WindowListener implementation (remove this???)
public void windowClosing(WindowEvent e) {
	base.closeViewport(viewportId);
}

public void windowClosed(WindowEvent ev) {}
 public void windowOpened(WindowEvent ev) {}
public void windowIconified(WindowEvent ev) {}
public void windowDeiconified(WindowEvent ev) {}
public void windowActivated(WindowEvent ev) {}
public void windowDeactivated(WindowEvent ev) {}


//
// Package methods
//

Navigation getNavigation()
{
    return navigation;
}

}

// EOF
