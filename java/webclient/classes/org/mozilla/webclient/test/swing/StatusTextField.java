package org.mozilla.webclient.test.swing;
/*
 * StatusPanel.java
 */

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import org.mozilla.webclient.*;

public class StatusTextField extends JTextField implements DocumentLoadListener {

    public StatusTextField(BrowserControl bc) {
	super();
	setEditable(false);
	try {
           bc.getEventRegistration().addDocumentLoadListener(this);
        } catch (Exception ex) {
            System.out.println(ex.toString());
        }

    }
    public void docEventPerformed(String event) {
	if (event.equals("startdocumentload")) {
	    this.setText("Loading document...");
	} else if (event.equals("enddocumentload")) {
	    this.setText("Finished loading document.");
	}

  }
    
}
