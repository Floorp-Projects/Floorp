/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: SecureApplet.java,v 1.2 2001/07/12 19:58:29 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

import java.util.*;
import java.awt.*;
import java.awt.event.*;
import java.applet.*;
import java.text.*;
import java.io.*;
import java.net.*;

public class SecureApplet extends Applet implements ActionListener
{
    Button    b1, b2, clear;
    TextArea  content;

    public void init() {
	b1 = new Button("Secure");
	b2 = new Button("Nonsecure");
	clear = new Button("Clear");
	content = new TextArea(15, 35);
	setFont(new Font("Monospaced", Font.PLAIN, 10));
	add(content);
	add(b1); add(b2); add(clear);
	b1.addActionListener(this);
	b2.addActionListener(this);
	clear.addActionListener(this);	
    }    

    public void actionPerformed(ActionEvent evt) {
          Button source = (Button)evt.getSource();
          if (source == b1) 
	      {
		  doSecureConnect("lyola");
		  return;
	      }
	  if (source == b2) 
	      {
		  doNonsecureConnect("lyola");
		  return;
	      }
	  if (source == clear) 
	      {
		  content.setText("");
	      }
    }


    public void paint(Graphics g) {
	
    }

    public void start() {
    }

    public void stop() {
    }

    public String getAppletInfo() {
        return "Title: Secure Connection test";
    }
    
    void readStream(URL u, InputStream is) {
	byte[] buf = new byte[256];
	int r;
	StringBuffer sb = new StringBuffer();
	content.setText("reading....");
	sb.append("Following read from url "+u+":\n");
	try {
	    while ( (r = is.read(buf)) != -1)
		{
		    sb.append(new String(buf, 0, r));
		}
	} catch (IOException e) {
	    e.printStackTrace();
	}
	sb.append("_________________________________________");
	content.setText(new String(sb));
    }

    void doSecureConnect(String host) {
	InputStream is;
	URL u;
	try {
	    URL u1 = getDocumentBase();
	    u = new URL("https", 
			u1.getHost(), 
			443,
			"/applets/https/data1.dat");
	    is = u.openStream();
	} catch (Exception e) {
	    e.printStackTrace();
	    return;
	}
	readStream(u, is);
    }
    
    void doNonsecureConnect(String host) {
	InputStream is;
	URL u;
	try {
	    URL u1 = getDocumentBase();
	    u = new URL("http", 
			u1.getHost(),
			80, 
			"/applets/https/data2.dat");	
    	    is = u.openStream();
	} catch (Exception e) {
	    e.printStackTrace();
	    return;
	}
	readStream(u, is);
    }
    
}
