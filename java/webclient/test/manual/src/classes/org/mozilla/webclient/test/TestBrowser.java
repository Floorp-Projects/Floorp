/*
 * Copyright (C) 2004 Sun Microsystems, Inc. All rights reserved. Use is
 * subject to license terms.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */ 

package org.mozilla.webclient.test;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.border.*;
import javax.swing.SwingConstants;
import java.io.File;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.Map;
import java.util.Iterator;

import org.mozilla.webclient.*;

/**
 * <p>webclient API demo main class.</p>

 * <p><code>TestBrowser</code> is a GUI application demonstrating the
 * usage of the package <code>org.mozilla.webclient</code> </p>

 * <p>This is based on the TestBrowser class in the <a
 * href="https://jdic.dev.java.net/">JDIC project</a>. </p>
 */

public class TestBrowser extends JPanel {

    public void addNotify() {
	super.addNotify();
	browserControlCanvas.setVisible(true);
    }

    public static ImageIcon browseIcon = new ImageIcon(
        TestBrowser.class.getResource("images/Right.gif"));

    BorderLayout borderLayout1 = new BorderLayout();

    JToolBar jBrowserToolBar = new JToolBar();
    JButton jStopButton = new JButton("Stop",
            new ImageIcon(getClass().getResource("images/Stop.png")));

    JButton jRefreshButton = new JButton("Refresh",
            new ImageIcon(getClass().getResource("images/Reload.png")));
    JButton jForwardButton = new JButton("Forward",
            new ImageIcon(getClass().getResource("images/Forward.png")));
    JButton jBackButton = new JButton("Back",
            new ImageIcon(getClass().getResource("images/Back.png")));
    JButton jCopyButton = new JButton("Copy");

    JPanel jAddressPanel = new JPanel();
    JLabel jAddressLabel = new JLabel();
    JTextField jAddressTextField = new JTextField();
    JButton jGoButton = new JButton();
    JPanel jAddrToolBarPanel = new JPanel();
    MyStatusBar statusBar = new MyStatusBar();
    JPanel jBrowserPanel = new JPanel();

    BrowserControl webBrowser;
    Navigation navigation;
    History history;
    EventRegistration eventRegistration;
    CurrentPage currentPage;
    Canvas browserControlCanvas;

    public TestBrowser() {
        try {
            jbInit();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void main(String[] args) {
        try {
            UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
	    BrowserControlFactory.setAppData(System.getProperty("BROWSER_BIN_DIR"));
        } catch (Exception e) {
	    System.out.println("Can't init test browser, exception: " + e + " "
			       + e.getMessage());
	}

        JFrame frame = new JFrame("Webclient API Demo - Browser");

        Container contentPane = frame.getContentPane();

        contentPane.setLayout(new GridLayout(1, 1));
        contentPane.add(new TestBrowser());

        frame.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent e) {
                System.exit(0);
            }
        });

        frame.pack();
        frame.setVisible(true);
    }

    private void jbInit() throws Exception {
        this.setLayout(borderLayout1);

        Dimension screenSize = Toolkit.getDefaultToolkit().getScreenSize();

        this.setPreferredSize(new Dimension(screenSize.width * 9 / 10,
                screenSize.height * 8 / 10));

        ToolTipManager.sharedInstance().setLightWeightPopupEnabled(false);

        jAddressLabel.setBorder(BorderFactory.createEmptyBorder(0, 4, 0, 4));
        jAddressLabel.setToolTipText("");
        jAddressLabel.setText(" URL: ");

        jGoButton.setBorder(BorderFactory.createCompoundBorder(new EmptyBorder(0,
                2, 0, 2),
                new EtchedBorder()));
        jGoButton.setMaximumSize(new Dimension(60, 25));
        jGoButton.setMinimumSize(new Dimension(60, 25));
        jGoButton.setPreferredSize(new Dimension(60, 25));
        jGoButton.setToolTipText("Load the given URL");
        jGoButton.setIcon(browseIcon);
        jGoButton.setText("GO");
        jGoButton.addActionListener(new TestBrowser_jGoButton_actionAdapter(this));
        jAddressPanel.setLayout(new BorderLayout());

        jAddressTextField.addActionListener(new TestBrowser_jAddressTextField_actionAdapter(this));
        jBackButton.setToolTipText("Go back one page");
        jBackButton.setHorizontalTextPosition(SwingConstants.TRAILING);
        jBackButton.setEnabled(false);
        jBackButton.setMaximumSize(new Dimension(75, 27));
        jBackButton.setPreferredSize(new Dimension(75, 27));
        jBackButton.addActionListener(new TestBrowser_jBackButton_actionAdapter(this));

        jCopyButton.setToolTipText("Copy current selection");
        jCopyButton.setHorizontalTextPosition(SwingConstants.TRAILING);
        jCopyButton.setMaximumSize(new Dimension(75, 27));
        jCopyButton.setPreferredSize(new Dimension(75, 27));
        jCopyButton.addActionListener(new TestBrowser_jCopyButton_actionAdapter(this));

        jForwardButton.setToolTipText("Go forward one page");
        jForwardButton.setEnabled(false);
        jForwardButton.addActionListener(new TestBrowser_jForwardButton_actionAdapter(this));
        jRefreshButton.setToolTipText("Reload current page");
        jRefreshButton.setEnabled(true);
        jRefreshButton.setMaximumSize(new Dimension(75, 27));
        jRefreshButton.setMinimumSize(new Dimension(75, 27));
        jRefreshButton.setPreferredSize(new Dimension(75, 27));
        jRefreshButton.addActionListener(new TestBrowser_jRefreshButton_actionAdapter(this));
        jStopButton.setToolTipText("Stop loading this page");
        jStopButton.setVerifyInputWhenFocusTarget(true);
        jStopButton.setText("Stop");
        jStopButton.setEnabled(true);
        jStopButton.setMaximumSize(new Dimension(75, 27));
        jStopButton.setMinimumSize(new Dimension(75, 27));
        jStopButton.setPreferredSize(new Dimension(75, 27));
        jStopButton.addActionListener(new TestBrowser_jStopButton_actionAdapter(this));
        jAddressPanel.add(jAddressLabel, BorderLayout.WEST);
        jAddressPanel.add(jAddressTextField, BorderLayout.CENTER);
        jAddressPanel.add(jGoButton, BorderLayout.EAST);
        jAddressPanel.setBorder(BorderFactory.createCompoundBorder(
            BorderFactory.createEtchedBorder(),
            BorderFactory.createEmptyBorder(2, 0, 2, 0)));

        jBrowserToolBar.setFloatable(false);
        jBrowserToolBar.add(jBackButton, null);
        jBrowserToolBar.add(jForwardButton, null);
        jBrowserToolBar.addSeparator();
        jBrowserToolBar.add(jRefreshButton, null);
        jBrowserToolBar.add(jStopButton, null);
	//        jBrowserToolBar.add(jCopyButton, null);
        jBrowserToolBar.setBorder(BorderFactory.createCompoundBorder(
            BorderFactory.createEtchedBorder(),
            BorderFactory.createEmptyBorder(2, 2, 2, 0)));

        jAddrToolBarPanel.setLayout(new BorderLayout());
        jAddrToolBarPanel.add(jAddressPanel, BorderLayout.CENTER);
        jAddrToolBarPanel.add(jBrowserToolBar, BorderLayout.WEST);
        jAddrToolBarPanel.setBorder(BorderFactory.createEmptyBorder(0, 0, 2, 0));

        statusBar.setBorder(BorderFactory.createEmptyBorder(2, 0, 0, 0));
        statusBar.lblDesc.setText("Webclient API Demo - Browser");
	
        try {
            webBrowser = BrowserControlFactory.newBrowserControl();
	    navigation = (Navigation)
		webBrowser.queryInterface(BrowserControl.NAVIGATION_NAME);
	    history = (History) 
		webBrowser.queryInterface(BrowserControl.HISTORY_NAME);
	    eventRegistration = (EventRegistration) 
		webBrowser.queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
	    currentPage = (CurrentPage)
		webBrowser.queryInterface(BrowserControl.CURRENT_PAGE_NAME);
	    browserControlCanvas  = (Canvas)
		webBrowser.queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
            // Print out debug messages in the command line.
            //myWebBrowser.setDebug(true);
        } catch (Exception e) {
            System.out.println(e.getMessage());
            return;
        }
	
        eventRegistration.addDocumentLoadListener(new PageInfoListener() {
		public void eventDispatched(WebclientEvent event) {
		    Map map = (Map) event.getEventData();
		    if (event instanceof DocumentLoadEvent) {
			switch ((int) event.getType()) {
			case ((int) DocumentLoadEvent.START_URL_LOAD_EVENT_MASK):
			    if (map.get("headers") instanceof Map) {
				Iterator iter = (map = (Map) map.get("headers")).keySet().iterator();
				while (iter.hasNext()) {
				    String curName = iter.next().toString();
				    System.out.println("\t" + curName + 
						       ": " + 
						       map.get(curName));
				}
			    }

			    break;
			case ((int) DocumentLoadEvent.START_DOCUMENT_LOAD_EVENT_MASK):
			    updateStatusInfo("Loading started.");
			    break;
			case ((int) DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK):
			    jBackButton.setEnabled(history.canBack());
			    jForwardButton.setEnabled(history.canForward());
			    updateStatusInfo("Loading completed.");
			    
			    if (event.getEventData() != null) {
				jAddressTextField.setText(map.get("URI").toString());
			    }
			    break;
			case ((int) DocumentLoadEvent.END_URL_LOAD_EVENT_MASK):
			    if (map.get("headers") instanceof Map) {
				Iterator iter = (map = (Map) map.get("headers")).keySet().iterator();
				while (iter.hasNext()) {
				    String curName = iter.next().toString();
				    System.out.println("\t" + curName + 
						       ": " + 
						       map.get(curName));
				}
			    }
			    break;
			case ((int) DocumentLoadEvent.PROGRESS_URL_LOAD_EVENT_MASK):
			    updateStatusInfo(map.get("message").toString());
			    break;
			case ((int) DocumentLoadEvent.FETCH_INTERRUPT_EVENT_MASK):
			    updateStatusInfo("Loading error.");
			    break;
			}
		    }
		}
	    });	
	
        jBrowserPanel.setLayout(new BorderLayout());
        jBrowserPanel.add(browserControlCanvas, BorderLayout.CENTER);
	
        this.add(jAddrToolBarPanel, BorderLayout.NORTH);
        this.add(statusBar, BorderLayout.SOUTH);
        this.add(jBrowserPanel, BorderLayout.CENTER);
    }

    void updateStatusInfo(String statusMessage) {
        statusBar.lblStatus.setText(statusMessage);
    }

    /**
     * Check the current input URL string in the address text field, load it,
     * and update the status info and toolbar info.
     */
    void loadURL() {
        String inputValue = jAddressTextField.getText();

        if (inputValue == null) {
            JOptionPane.showMessageDialog(this, "The given URL is NULL:",
                    "Warning", JOptionPane.WARNING_MESSAGE);
        } else {
            // Check if the text value is a URL string.
            URL curUrl = null;

            try {
                // Check if the input string is a local path by checking if it starts
                // with a driver name(on Windows) or root path(on Unix).
                File[] roots = File.listRoots();

                for (int i = 0; i < roots.length; i++) {
                    if (inputValue.toLowerCase().startsWith(roots[i].toString().toLowerCase())) {
                        File curLocalFile = new File(inputValue);

                        curUrl = curLocalFile.toURL();
                        break;
                    }
                }

                if (curUrl == null) {
                    // Check if the text value starts with known protocols.
                    if (inputValue.toLowerCase().startsWith("http://")
                            || inputValue.toLowerCase().startsWith("ftp://")
                            || inputValue.toLowerCase().startsWith("gopher://")) {
                        curUrl = new URL(inputValue);
                    } else {
                        if (inputValue.toLowerCase().startsWith("ftp.")) {
                            curUrl = new URL("ftp://" + inputValue);
                        } else if (inputValue.toLowerCase().startsWith("gopher.")) {
                            curUrl = new URL("gopher://" + inputValue);
                        } else {
                            curUrl = new URL("http://" + inputValue);
                        }
                    }
                }

                navigation.loadURL(curUrl.toString());

                // Update the address text field, statusbar, and toolbar info.
                updateStatusInfo("Loading " + curUrl.toString() + " ......");
            } catch (MalformedURLException mue) {
                JOptionPane.showMessageDialog(this,
                        "The given URL is not valid:" + inputValue, "Warning",
                        JOptionPane.WARNING_MESSAGE);
            }
        }
    }

    void jGoButton_actionPerformed(ActionEvent e) {
        loadURL();
    }

    void jAddressTextField_actionPerformed(ActionEvent e) {
        loadURL();
    }

    void jBackButton_actionPerformed(ActionEvent e) {
        history.back();
    }

    void jCopyButton_actionPerformed(ActionEvent e) {
	currentPage.copyCurrentSelectionToSystemClipboard();
    }


    void jForwardButton_actionPerformed(ActionEvent e) {
        history.forward();
    }

    void jRefreshButton_actionPerformed(ActionEvent e) {
        navigation.refresh(Navigation.LOAD_FORCE_RELOAD);
    }

    void jStopButton_actionPerformed(ActionEvent e) {
        navigation.stop();
    }
}


class TestBrowser_jAddressTextField_actionAdapter implements java.awt.event.ActionListener {
    TestBrowser adaptee;

    TestBrowser_jAddressTextField_actionAdapter(TestBrowser adaptee) {
        this.adaptee = adaptee;
    }

    public void actionPerformed(ActionEvent e) {
        adaptee.jAddressTextField_actionPerformed(e);
    }
}


class TestBrowser_jBackButton_actionAdapter implements java.awt.event.ActionListener {
    TestBrowser adaptee;

    TestBrowser_jBackButton_actionAdapter(TestBrowser adaptee) {
        this.adaptee = adaptee;
    }

    public void actionPerformed(ActionEvent e) {
        adaptee.jBackButton_actionPerformed(e);
    }
}

class TestBrowser_jCopyButton_actionAdapter implements java.awt.event.ActionListener {
    TestBrowser adaptee;

    TestBrowser_jCopyButton_actionAdapter(TestBrowser adaptee) {
        this.adaptee = adaptee;
    }

    public void actionPerformed(ActionEvent e) {
        adaptee.jCopyButton_actionPerformed(e);
    }
}


class TestBrowser_jForwardButton_actionAdapter implements java.awt.event.ActionListener {
    TestBrowser adaptee;

    TestBrowser_jForwardButton_actionAdapter(TestBrowser adaptee) {
        this.adaptee = adaptee;
    }

    public void actionPerformed(ActionEvent e) {
        adaptee.jForwardButton_actionPerformed(e);
    }
}


class TestBrowser_jRefreshButton_actionAdapter implements java.awt.event.ActionListener {
    TestBrowser adaptee;

    TestBrowser_jRefreshButton_actionAdapter(TestBrowser adaptee) {
        this.adaptee = adaptee;
    }

    public void actionPerformed(ActionEvent e) {
        adaptee.jRefreshButton_actionPerformed(e);
    }
}


class TestBrowser_jStopButton_actionAdapter implements java.awt.event.ActionListener {
    TestBrowser adaptee;

    TestBrowser_jStopButton_actionAdapter(TestBrowser adaptee) {
        this.adaptee = adaptee;
    }

    public void actionPerformed(ActionEvent e) {
        adaptee.jStopButton_actionPerformed(e);
    }
}


class TestBrowser_jGoButton_actionAdapter implements java.awt.event.ActionListener {
    TestBrowser adaptee;

    TestBrowser_jGoButton_actionAdapter(TestBrowser adaptee) {
        this.adaptee = adaptee;
    }

    public void actionPerformed(ActionEvent e) {
        adaptee.jGoButton_actionPerformed(e);
    }
}
