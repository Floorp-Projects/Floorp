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
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.test;

/*
 * EmbeddedMozilla.java
 */

import java.awt.*;
import java.awt.event.*;

import javax.swing.tree.TreeModel;
import javax.swing.tree.TreeNode;

import java.util.Enumeration;
import java.util.Properties;

import org.mozilla.webclient.*;
import org.mozilla.util.Assert;

/**
 *

 * This is a test application for using the BrowserControl.

 *
 * @version $Id: EmbeddedMozilla.java,v 1.1 2000/03/04 01:10:54 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlFactory

 */

public class EmbeddedMozilla extends Frame implements ActionListener, DocumentLoadListener {
    static final int defaultWidth = 640;
    static final int defaultHeight = 480;

    private TextField		urlField;
	private BrowserControl	browserControl;
	private CurrentPage	    currentPage;
	private Bookmarks	    bookmarks;
	private TreeModel	    bookmarksTree;
	private Panel			controlPanel;
	private Panel			buttonsPanel;

public static void printUsage()
{
    System.out.println("usage: java org.mozilla.webclient.test.EmbeddedMozilla <path> [url]");
    System.out.println("       <path> is the absolute path to the native browser bin directory, ");
    System.out.println("       including the bin.");
}
	
	
    public static void main (String[] arg) {
        if (1 > arg.length) {
            printUsage();
            System.exit(-1);
        }
		String urlArg =(2 == arg.length) ? arg[1] : "file:///E|/Projects/tmp/5105.html";
		
		EmbeddedMozilla gecko = 
			new EmbeddedMozilla("Embedded Mozilla", arg[0], urlArg);
    } // main()
    
    public EmbeddedMozilla (String title, String binDir, String url) 
    {
		super(title);
        System.out.println("constructed with binDir: " + binDir + " url: " + 
                           url);
	
		setSize(defaultWidth, defaultHeight);
	
		// Create the URL field
		urlField = new TextField("", 30);
        urlField.addActionListener(this);        	

		// Create the buttons sub panel
		buttonsPanel = new Panel();
        buttonsPanel.setLayout(new GridBagLayout());

		// Add the buttons
		makeItem(buttonsPanel, "Back",    0, 0, 1, 1, 0.0, 0.0);
		makeItem(buttonsPanel, "Forward", 1, 0, 1, 1, 0.0, 0.0);
		makeItem(buttonsPanel, "Stop",    2, 0, 1, 1, 0.0, 0.0);
		makeItem(buttonsPanel, "Refresh", 3, 0, 1, 1, 0.0, 0.0);
		makeItem(buttonsPanel, "Bookmarks",    4, 0, 1, 1, 0.0, 0.0);

		// Create the control panel
		controlPanel = new Panel();
        controlPanel.setLayout(new BorderLayout());
        
        // Add the URL field, and the buttons panel
		controlPanel.add(urlField,     BorderLayout.CENTER);
		controlPanel.add(buttonsPanel, BorderLayout.WEST);

		// Create the browser
        BrowserControlCanvas browserCanvas = null;

        try {
            BrowserControlFactory.setAppData(binDir);
			browserControl = BrowserControlFactory.newBrowserControl();
            browserCanvas = 
                (BrowserControlCanvas) 
                browserControl.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);

        }
        catch(Exception e) {
            System.out.println("Can't create BrowserControl: " + 
                               e.getMessage());
        }
        Assert.assert(null != browserCanvas);
		browserCanvas.setSize(defaultWidth, defaultHeight);
	
		// Add the control panel and the browserCanvas
		add(controlPanel, BorderLayout.NORTH);
		add(browserCanvas,      BorderLayout.CENTER);

		addWindowListener(new WindowAdapter() {
		    public void windowClosing(WindowEvent e) {
				dispose();
                System.out.println("Got windowClosing");
				// should close the BrowserControlCanvas
		    }
		    
		    public void windowClosed(WindowEvent e) { 
                System.out.println("Got windowClosed");
				System.exit(0);
		    }
		});
	 
		pack();
		show();
		toFront();

        Navigation navigation = null;
	
		try {
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
            currentPage = (CurrentPage)
                browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
            
        }
		catch (Exception e) {
		    System.out.println(e.toString());
		}
        
        try {
            EventRegistration eventRegistration = 
                (EventRegistration)
                browserControl.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
            System.out.println("debug: edburns: adding DocumentLoadListener");
            eventRegistration.addDocumentLoadListener(this);


            // PENDING(edburns): test code, replace with production code
            bookmarks = 
                (Bookmarks)
                browserControl.queryInterface(BrowserControl.BOOKMARKS_NAME);
            System.out.println("debug: edburns: got Bookmarks instance");

            bookmarksTree = bookmarks.getBookmarks();
            TreeNode bookmarksRoot = (TreeNode) bookmarksTree.getRoot();

            System.out.println("debug: edburns: testing the Enumeration");
            int childCount = bookmarksRoot.getChildCount();
            System.out.println("debug: edburns: root has " + childCount + 
                               " children.");

            Enumeration rootChildren = bookmarksRoot.children();
            TreeNode currentChild;

            int i = 0, childIndex;
            while (rootChildren.hasMoreElements()) {
                currentChild = (TreeNode) rootChildren.nextElement();
                System.out.println("debug: edburns: bookmarks root has children! child: " + i + " name: " + currentChild.toString());
                i++;
            }

            System.out.println("debug: edburns: testing getChildAt(" + --i + "): ");
            currentChild = bookmarksRoot.getChildAt(i);
            System.out.println("debug: edburns: testing getIndex(Child " + 
                               i + "): index should be " + i + ".");
            childIndex = bookmarksRoot.getIndex(currentChild);
            System.out.println("debug: edburns: index is: " + childIndex);

            /**********
            BookmarkEntry folder = bookmarks.newBookmarkFolder("newFolder");

            bookmarks.addBookmark(null, folder);

            BookmarkEntry entry = bookmarks.newBookmarkEntry("http://yoyo.com");
            System.out.println("debug: edburns: got new entry");

            Properties entryProps = entry.getProperties();

            System.out.println("debug: edburns: entry url: " + 
                               entryProps.getProperty(BookmarkEntry.URL));
            bookmarks.addBookmark(folder, entry);
            **********/
        }
		catch (Exception e) {
		    System.out.println(e.toString());
		}
        
        if (null != navigation) {
            navigation.loadURL(url);
        }
    } // EmbeddedMozilla() ctor


    public void actionPerformed (ActionEvent evt) {
    	String command = evt.getActionCommand();


        try {
            Navigation navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
            CurrentPage currentPage = (CurrentPage)
               browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
            History history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);
            if (command.equals("Stop")) {
	    		navigation.stop();
	    	}
            else if (command.equals("Refresh")) {
                navigation.refresh(Navigation.LOAD_NORMAL);
            }
            else if (command.equals("Bookmarks")) {
                if (null == bookmarksTree) {
                    bookmarksTree = bookmarks.getBookmarks();
                }
                BookmarksFrame frame = new BookmarksFrame(bookmarksTree);
                frame.setSize(new Dimension(320,480));
                frame.setVisible(true);
            }
	    	else if (command.equals("Back")) {
	    		if (history.canBack()) {
		    		history.back();
		    	}
        	}
	    	else if (command.equals("Forward")) {
	    		if (history.canForward()) {
		    		history.forward();
		    	}
            }
	    	else {
		        navigation.loadURL(urlField.getText());
		    }
        }
        catch (Exception e) {
            System.out.println(e.getMessage());
        }
    } // actionPerformed()


    private void makeItem (Panel p, Object arg, int x, int y, int w, int h, double weightx, double weighty) {
        GridBagLayout gbl = (GridBagLayout) p.getLayout();
        GridBagConstraints c = new GridBagConstraints();
        Component comp;

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
        }
    } // makeItem()


//
// From DocumentLoadListener
//

public void eventDispatched(WebclientEvent event)
{
    if (event instanceof DocumentLoadEvent) {
        String currentURL = currentPage.getCurrentURL();
        System.out.println("debug: edburns: Currently Viewing: " + 
                           currentURL);
        urlField.setText(currentURL);
    }
}

}

// EOF
