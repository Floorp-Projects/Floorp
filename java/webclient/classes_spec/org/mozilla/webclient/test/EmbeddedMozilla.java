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

import org.mozilla.webclient.*;
import org.mozilla.util.Assert;

/**
 *

 * This is a test application for using the BrowserControl.

 *
 * @version $Id: EmbeddedMozilla.java,v 1.2 2000/03/13 18:42:02 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlFactory

 */

public class EmbeddedMozilla extends Frame implements ActionListener
{

public static String binDir;
public static String url;
public static int winCount = 0;
public static String NEW_BUTTON_LABEL = "New Window";

public static int width = 200;
public static int height = 100;

private Panel buttonPanel; 

public static void printUsage()
{
    System.out.println("usage: java org.mozilla.webclient.test.EMWindow <path> [url]");
    System.out.println("       <path> is the absolute path to the native browser bin directory, ");
    System.out.println("       including the bin.");
}
	
public EmbeddedMozilla() 
{
    super("EmbeddedMozilla Launcher");
    setSize(200, 100);
    buttonPanel = new Panel();
    buttonPanel.setLayout(new GridBagLayout());
    
    // Add the buttons
    makeItem(buttonPanel, NEW_BUTTON_LABEL,    0, 0, 1, 1, 0.0, 0.0);
    add(buttonPanel, BorderLayout.CENTER);

    addWindowListener(new WindowAdapter() {
public void windowClosing(WindowEvent e) {
    System.out.println("Got windowClosing");
    System.out.println("bye");
    System.exit(0);
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
}

public void actionPerformed (ActionEvent evt) 
{
    String command = evt.getActionCommand();
    EMWindow newWindow;
    
    if (command.equals(NEW_BUTTON_LABEL)) {
        System.out.println("Creating new EmbeddedMozilla window");
        newWindow = new EMWindow("EmbeddedMozila#" + winCount++,
                                 binDir, url);
    }
} // actionPerformed()

	
private void makeItem (Panel p, Object arg, int x, int y, int w, 
                       int h, double weightx, double weighty) 
{
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


public static void main(String [] arg)
{
    if (1 > arg.length) {
        printUsage();
        System.exit(-1);
    }
    String urlArg =(2 == arg.length) ? arg[1] : "file:///E|/Projects/tmp/5105.html";

    // set class vars used in EmbeddedMozilla ctor 
    binDir = arg[0];
    url = urlArg;
    EmbeddedMozilla em = new EmbeddedMozilla();
}

}

// EOF
