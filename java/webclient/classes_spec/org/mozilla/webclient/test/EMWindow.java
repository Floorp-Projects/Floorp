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
 * EMWindow.java
 */

import java.awt.*;
import java.awt.event.*;

import javax.swing.tree.TreeModel;
import javax.swing.tree.TreeNode;
import javax.swing.*;

import java.util.Enumeration;
import java.util.Properties;
import java.util.*;

import org.mozilla.webclient.*;
import org.mozilla.util.Assert;

import org.w3c.dom.Document;

/**
 *

 * This is a test application for using the BrowserControl.

 *
 * @version $Id: EMWindow.java,v 1.16 2000/07/07 18:47:25 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlFactory

 */

public class EMWindow extends Frame implements DialogClient, ActionListener, DocumentLoadListener, MouseListener {
    static final int defaultWidth = 640;
    static final int defaultHeight = 480;

  private int winNum;

    private TextField		urlField;
	private BrowserControl	browserControl;
    private BrowserControlCanvas browserCanvas;

    private Navigation navigation = null;
	private CurrentPage	    currentPage;
	private History	        history;
	private Bookmarks	    bookmarks;
    private BookmarksFrame bookmarksFrame = null;
	private TreeModel	    bookmarksTree;
    private DOMViewerFrame  domViewer;
	private Panel			controlPanel;
	private Panel			statusPanel;
	private Panel			buttonsPanel;
    private FindDialog           findDialog = null;
    private MenuBar             menuBar;
    private Label          statusLabel;
    private String currentURL;

  private Document       currentDocument = null;

  private EmbeddedMozilla creator;
  private boolean viewMode = true;

    private Component forwardButton;
    private Component backButton;
    private Component stopButton;
    private Component refreshButton;

  public static void main(String [] arg)
    {
    }


    public EMWindow (String title, String binDir, String url, int winnum, EmbeddedMozilla Creator) 
    {
	super(title);
	creator = Creator;
    currentURL = url;
	winNum = winnum;
        System.out.println("constructed with binDir: " + binDir + " url: " + 
                           url);
		setSize(defaultWidth, defaultHeight);
	
		// Create the Menu Bar
		menuBar = new MenuBar();
		this.setMenuBar(menuBar);
		Menu fileMenu = new Menu("File");
		Menu viewMenu = new Menu("View");
		Menu searchMenu = new Menu("Search");
		Menu editMenu = new Menu("Edit");
		MenuItem newItem = new MenuItem("New Window");
		MenuItem closeItem = new MenuItem("Close");
		MenuItem findItem = new MenuItem("Find");
		MenuItem findNextItem = new MenuItem("Find Next");
		MenuItem sourceItem = new MenuItem("View Page Source");
		MenuItem pageInfoItem = new MenuItem("View Page Info");
		MenuItem selectAllItem = new MenuItem("Select All");
        MenuItem copyItem = new MenuItem("Copy");
		menuBar.add(fileMenu);
		menuBar.add(viewMenu);
		menuBar.add(searchMenu);
		menuBar.add(editMenu);
		fileMenu.add(newItem);
		newItem.addActionListener(this);
		fileMenu.add(closeItem);
		closeItem.addActionListener(this);
		searchMenu.add(findItem);
		findItem.addActionListener(this);
		searchMenu.add(findNextItem);
		findNextItem.addActionListener(this);
		viewMenu.add(sourceItem);
		sourceItem.addActionListener(this);
		viewMenu.add(pageInfoItem);
		pageInfoItem.addActionListener(this);
		editMenu.add(selectAllItem);
		selectAllItem.addActionListener(this);
        editMenu.add(copyItem);
        copyItem.addActionListener(this);

		// Create the URL field
		urlField = new TextField("", 30);
        urlField.addActionListener(this);        	
        urlField.setText(url);


		// Create the buttons sub panel
		buttonsPanel = new Panel();
        buttonsPanel.setLayout(new GridBagLayout());

		// Add the buttons
		backButton = makeItem(buttonsPanel, "Back",    0, 0, 1, 1, 0.0, 0.0);
        backButton.setEnabled(false);
		forwardButton = makeItem(buttonsPanel, "Forward", 1, 0, 1, 1, 0.0, 0.0);
        forwardButton.setEnabled(false);
		stopButton = makeItem(buttonsPanel, "Stop",    2, 0, 1, 1, 0.0, 0.0);
        stopButton.setEnabled(false);
		refreshButton = makeItem(buttonsPanel, "Refresh", 3, 0, 1, 1, 0.0, 0.0);
        refreshButton.setEnabled(false);
		makeItem(buttonsPanel, "Bookmarks",    4, 0, 1, 1, 0.0, 0.0);
		makeItem(buttonsPanel, "DOMViewer",    5, 0, 1, 1, 0.0, 0.0);

		// Create the control panel
		controlPanel = new Panel();
        controlPanel.setLayout(new BorderLayout());
        
        // Add the URL field, and the buttons panel
		controlPanel.add(urlField,     BorderLayout.CENTER);
		controlPanel.add(buttonsPanel, BorderLayout.WEST);

        // create the status panel
        statusPanel = new Panel();
        statusPanel.setLayout(new BorderLayout());

        // create and add the statusLabel
        statusLabel = new Label("", Label.LEFT);
        statusLabel.setBackground(Color.lightGray);
        statusPanel.add(statusLabel, BorderLayout.CENTER);

		// Create the browser
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
		add(statusPanel,      BorderLayout.SOUTH);

		addWindowListener(new WindowAdapter() {
		    public void windowClosing(WindowEvent e) {
                System.out.println("Got windowClosing");
		System.out.println("destroying the BrowserControl");
		EMWindow.this.delete();
				// should close the BrowserControlCanvas
		creator.DestroyEMWindow(winNum);
		    }
		    
		    public void windowClosed(WindowEvent e) { 
                System.out.println("Got windowClosed");
		    }
		});
	 
		pack();
		show();
		toFront();

		try {
            navigation = (Navigation)
                browserControl.queryInterface(BrowserControl.NAVIGATION_NAME);
            currentPage = (CurrentPage)
                browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
            history = (History)
                browserControl.queryInterface(BrowserControl.HISTORY_NAME);
            
        }
		catch (Exception e) {
		    System.out.println(e.toString());
		}
        
        try {
            EventRegistration eventRegistration = 
                (EventRegistration)
                browserControl.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
            eventRegistration.addDocumentLoadListener(this);
            eventRegistration.addMouseListener(this);

            // PENDING(edburns): test code, replace with production code
            bookmarks = 
                (Bookmarks)
                browserControl.queryInterface(BrowserControl.BOOKMARKS_NAME);
            System.out.println("debug: edburns: got Bookmarks instance");

            bookmarksTree = bookmarks.getBookmarks();
            /*********
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
            *****/

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
    } // EMWindow() ctor

public void delete()
{
    browserCanvas.setVisible(false);
    if (null != bookmarksFrame) {
	bookmarksFrame.setVisible(false);
	bookmarksFrame.dispose();
	bookmarksFrame = null;
    }
    if (null != domViewer) {
	domViewer.setVisible(false);
	domViewer.dispose();
	domViewer = null;
    }
    BrowserControlFactory.deleteBrowserControl(browserControl);
    browserControl = null;
    this.hide();
    this.dispose();
    urlField = null;
    browserCanvas = null;
    currentPage = null;
    bookmarks = null;
    bookmarksTree = null;
    controlPanel = null;
    buttonsPanel = null;
    currentDocument = null;
}


public void actionPerformed (ActionEvent evt) 
{
    String command = evt.getActionCommand();
    
    try {
        
        // deal with the menu item commands
        if (evt.getSource() instanceof MenuItem) {
            if (command.equals("New Window")) {
                creator.CreateEMWindow();
            }
            else if (command.equals("Close")) {
                System.out.println("Got windowClosing");
                System.out.println("destroying the BrowserControl");
                EMWindow.this.delete();
				// should close the BrowserControlCanvas
                creator.DestroyEMWindow(winNum);
            }
            else if (command.equals("Find")) {
				if (null == findDialog) {
                    Frame f = new Frame();
                    f.setSize(350,150);
                    findDialog = new FindDialog(this, this, 
                                                "Find in Page", "Find  ", 
                                                "", 20, false);
                    findDialog.setModal(false);
	       		}
				findDialog.setVisible(true);
                //		currentPage.findInPage("Sun", true, true);
            }
            else if (command.equals("Find Next")) {
                currentPage.findNextInPage(false);
            }
            else if (command.equals("View Page Source")) {
                currentPage.getSourceBytes(viewMode);
                viewMode = !viewMode;
            }
            else if (command.equals("View Page Info")) {
                currentPage.getPageInfo();
            }
            else if (command.equals("Select All")) {
                currentPage.selectAll();
            }
            else if (command.equals("Copy")) {
                currentPage.copyCurrentSelectionToSystemClipboard();
            }
	    }
        // deal with the button bar commands
	    else if(command.equals("Stop")) {
            navigation.stop();
        }
        else if (command.equals("Refresh")) {
            navigation.refresh(Navigation.LOAD_NORMAL);
        }
        else if (command.equals("Bookmarks")) {
            if (null == bookmarksTree) {
                bookmarksTree = bookmarks.getBookmarks();
            }
            
            if (null == bookmarksFrame) {
                bookmarksFrame = new BookmarksFrame(bookmarksTree, 
                                                    browserControl);
                bookmarksFrame.setSize(new Dimension(320,480));
                bookmarksFrame.setLocation(defaultWidth + 5, 0);
            }
            bookmarksFrame.setVisible(true);
        }
        else if (command.equals("DOMViewer")) {
            if (null == domViewer) {
                domViewer = new DOMViewerFrame("DOM Viewer", creator);
                domViewer.setSize(new Dimension(300, 600));
                domViewer.setLocation(defaultWidth + 5, 0);
            }
            if (null != currentDocument) {
                domViewer.setDocument(currentDocument);
                domViewer.setVisible(true);
            }
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


public void dialogDismissed(Dialog d) {
  if(findDialog.wasClosed()) {
    System.out.println("Find Dialog Closed");
	try {
	CurrentPage currentPage = (CurrentPage)
	  browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	currentPage.resetFind();
	}
	catch (Exception e) {
	System.out.println(e.getMessage());
      }
	}
  else {
    String searchString = findDialog.getTextField().getText();
    if(searchString == null) {
      System.out.println("Java ERROR - SearchString not received from Dialog Box" + 
			 searchString);
    }
    else if(searchString.equals("")) {
      System.out.println("Clear button selected");
    }
    else {
      System.out.println("Tring to Find String   -  " + searchString);
      System.out.println("Parameters are    - Backwrads = " + findDialog.backwards + " and Matchcase = " + findDialog.matchcase);
      try {
	CurrentPage currentPage = (CurrentPage)
	  browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	currentPage.findInPage(searchString, !findDialog.backwards, findDialog.matchcase);
      }
      catch (Exception e) {
	System.out.println(e.getMessage());
      }
    }
  }
}

public void dialogCancelled(Dialog d) {
  System.out.println("Find Dialog Closed");
	try {
	CurrentPage currentPage = (CurrentPage)
	  browserControl.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	currentPage.resetFind();
	}
	catch (Exception e) {
	System.out.println(e.getMessage());
    }
}
      



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
     }
     return comp;
} // makeItem()


//
// From WebclientEventListener sub-interfaces
//

/**

 * Important: do not call any webclient methods during this callback.
 * It may caus your app to deadlock.

 */

public void eventDispatched(WebclientEvent event)
{
    boolean enabledState;

    if (event instanceof DocumentLoadEvent) {
        switch ((int) event.getType()) {
        case ((int) DocumentLoadEvent.START_DOCUMENT_LOAD_EVENT_MASK):
            stopButton.setEnabled(true);
            refreshButton.setEnabled(true);
            currentURL = (String) event.getEventData();
            System.out.println("debug: edburns: Currently Viewing: " + 
                               currentURL);
            statusLabel.setText("Starting to load " + currentURL);
            urlField.setText(currentURL);
            currentDocument = null;
            break;
        case ((int) DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK):
            stopButton.setEnabled(false);
            backButton.setEnabled(history.canBack());
            forwardButton.setEnabled(history.canForward());
            statusLabel.setText("Done.");
            currentDocument = currentPage.getDOM();
            // add the new document to the domViewer
            if (null != domViewer) {
                domViewer.setDocument(currentDocument);
            }

            break;
        }
    }
}

//
// From MouseListener
// 

public void mouseClicked(java.awt.event.MouseEvent e)
{
    int modifiers = e.getModifiers();
     if (0 != (modifiers & InputEvent.BUTTON1_MASK)) {
        System.out.println("Button1 ");
    }
    if (0 != (modifiers & InputEvent.BUTTON2_MASK)) {
        System.out.println("Button2 ");
    }
    if (0 != (modifiers & InputEvent.BUTTON3_MASK)) {
        System.out.println("Button3 ");
    }
}

public void mouseEntered(java.awt.event.MouseEvent e)
{
    if (e instanceof WCMouseEvent) {
        WCMouseEvent wcMouseEvent = (WCMouseEvent) e;
        Properties eventProps = 
            (Properties) wcMouseEvent.getWebclientEvent().getEventData();
        if (null == eventProps) {
            return;
        }
        if (e.isAltDown()) {
            System.out.println("Alt ");
        }
        if (e.isControlDown()) {
            System.out.println("Ctrl ");
        }
        if (e.isShiftDown()) {
            System.out.println("Shift ");
        }
        if (e.isMetaDown()) {
            System.out.println("Meta ");
        }
        String href = eventProps.getProperty("href");
        if (null != href) {
            // if it's a relative URL
            if (null != currentURL && -1 == href.indexOf("://")) {
                int lastSlashIndex = currentURL.lastIndexOf('/');
                if (-1 == lastSlashIndex) {
                    href = currentURL + "/" + href;
                }
                else {
                    href = currentURL.substring(0, lastSlashIndex) + "/"+ href;
                }
            }
            statusLabel.setText(href);
        }
    }
}

public void mouseExited(java.awt.event.MouseEvent e)
{
    statusLabel.setText("");
}

public void mousePressed(java.awt.event.MouseEvent e)
{
}

public void mouseReleased(java.awt.event.MouseEvent e)
{
}


//
// Package methods
//

Navigation getNavigation()
{
    return navigation;
}

}

// EOF
