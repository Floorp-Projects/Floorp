/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The original code is RaptorCanvas
 * 
 * The Initial Developer of the Original Code is Kirk Baker <kbaker@eb.com> and * Ian Wilkinson <iw@ennoble.com
 */

package org.mozilla.webclient.test.swing;

/*
 * SwingEmbeddedMozilla.java
 */

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import org.mozilla.webclient.*;

/**
 *

 * This is a test application for using the BrowserControl.

 *
 * @version $Id: SwingEmbeddedMozilla.java,v 1.1 1999/10/20 00:49:24 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlCanvasFactory

 */

public class SwingEmbeddedMozilla extends JFrame
{
    static final int defaultWidth = 640;
    static final int defaultHeight = 480;
    static String binDir = null;

    private TextField		urlField;
	private BrowserControl	browserControl;
    private CommandPanel	controlPanel;

public static void printUsage()
{
    System.out.println("usage: java org.mozilla.webclient.test.SwingEmbeddedMozilla <path> [url]");
    System.out.println("       <path> is the absolute path to the native browser bin directory, ");
    System.out.println("       including the bin.");
}
	
	
    public static void main (String[] arg) {
        if (1 > arg.length) {
            printUsage();
            System.exit(-1);
        }
		String urlArg =(2 == arg.length) ? arg[1] : "http://www.mozilla.org/";

        SwingEmbeddedMozilla.binDir = arg[0];
		
		SwingEmbeddedMozilla gecko = 
			new SwingEmbeddedMozilla("Embedded Mozilla", arg[0], urlArg);
    } // main()
    
public SwingEmbeddedMozilla (String title, String binDir, String url) 
{
    super(title);

    Container contentPane = getContentPane();
    JMenuBar menubar = createMenuBar();

    System.out.println("constructed with " + url);
	
    addWindowListener(new WindowAdapter() {
      public void windowClosing(WindowEvent e) {
          dispose();
          // should close the BrowserControlCanvas
      }
      
      public void windowClosed(WindowEvent e) { 
          System.exit(0);
      }
    });
    
    setSize(defaultWidth, defaultHeight);
	
    // Create the browser
    BrowserControlCanvas browser  = null;
    
    try {
        BrowserControlCanvasFactory.setAppData(binDir);
        browser = BrowserControlCanvasFactory.newBrowserControlCanvas();
    }
    catch(Exception e) {
        System.out.println("Can't create BrowserControlCanvas: " + 
                           e.getMessage());
    }

    browser.setSize(defaultWidth, defaultHeight);

	// Add the control panel and the browser
    contentPane.add(browser,      BorderLayout.CENTER);
    
    
    controlPanel = new CommandPanel(this);
    contentPane.add(controlPanel, BorderLayout.NORTH);
    
    
    getRootPane().setMenuBar(menubar);
    
    pack();
    show();
    toFront();
	
    browserControl = browser.getWebShell();

    //        DocumentLoadListener dll;
    try {
        //            dll = new DocumentLoadListener();
        //            browserControl.getEventRegistration().addDocumentLoadListener(dll);
        contentPane.add(new StatusTextField(browserControl), BorderLayout.SOUTH);
        
    } catch (Exception ex) {
        System.out.println(ex.toString());
    }
    
    
    try {
        browserControl.loadURL(url);
        controlPanel.getURLField().setText(url);
    }
    catch (Exception e) {
        System.out.println(e.toString());
    }
} // SwingEmbeddedMozilla() ctor

private JMenuBar createMenuBar()
{
    JMenuBar menubar = new JMenuBar();
    JMenu menu = new JMenu("File");
    JMenuItem exitItem = new JMenuItem("Exit");
    JMenuItem newItem = new JMenuItem("New Window");
    
    KeyStroke ks = KeyStroke.getKeyStroke(KeyEvent.VK_Q, Event.CTRL_MASK);
    KeyStroke ks_new = KeyStroke.getKeyStroke(KeyEvent.VK_N, Event.CTRL_MASK);
    
    menu.getPopupMenu().setLightWeightPopupEnabled(false);
    newItem.setAccelerator(ks_new);
    newItem.addActionListener(new ActionListener() {
      public void actionPerformed(ActionEvent e) {
          new SwingEmbeddedMozilla("New Window", SwingEmbeddedMozilla.binDir,
                                   controlPanel.getURLField().getText());
      }
    });
    
    exitItem.setAccelerator(ks);
    exitItem.addActionListener( new ActionListener() {
      public void actionPerformed(ActionEvent e) {
          System.exit(0);
      }
    });
    menu.setMnemonic('F');
    exitItem.setMnemonic(KeyEvent.VK_X);
    newItem.setMnemonic(KeyEvent.VK_N);
    menu.add(newItem);
    menu.add(exitItem);
    
    menubar.add(menu);
    return menubar;
}

public void back() 
{
    try {
        if (browserControl.canBack()) {
            browserControl.back();
            int index = browserControl.getHistoryIndex();
            String newURL = browserControl.getURL(index);
            
            System.out.println(newURL);
            
            controlPanel.getURLField().setText(newURL);
        }
    } catch (Exception e) {
        System.out.println( "anns: exception in back   "+ e.toString());
    }
}

public void forward() 
{
    try {
        if (browserControl.canForward()) {
            browserControl.forward();
            int index = browserControl.getHistoryIndex();
            String newURL = browserControl.getURL(index);
            
            System.out.println(newURL);
            
            controlPanel.getURLField().setText(newURL);
        }
    } catch (Exception e) {
        System.out.println( "anns: exception in forward   "+ e.toString());
    }
}

public void stop()
{
    try {
        browserControl.stop();
    } catch (Exception e) {
        System.out.println("anns: exception in stop "+ e.toString());
    }
}

public void refresh() 
{
    try {
        browserControl.refresh();
    } catch (Exception e) {
        System.out.println("anns: exception in refresh" + e.toString());
    }
}

public void loadURL(String url) 
{
    try {
        browserControl.loadURL(url);
    } catch (Exception e) {
        System.out.println("anns: exception in load "+ e.toString());
    }
    
}

}

// EOF
