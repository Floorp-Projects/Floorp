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
 * The Original Code is The Waterfall Java Plugin Module
 * 
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 *
 * $Id: Aptest.java,v 1.1 2001/05/09 17:30:09 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

import java.util.*;
import java.awt.*;
import java.awt.event.*;
import java.applet.*;
import java.text.*;
import java.net.*;

public class Aptest extends Applet implements Runnable {
    Thread timer;                // The thread that displays clock
    int lastxs, lastys, lastxm,
        lastym, lastxh, lastyh;  // Dimensions used to draw hands 
    SimpleDateFormat formatter;  // Formats the date displayed
    String lastdate;             // String to hold date displayed
    Font clockFaceFont;          // Font for number display on clock
    Date currentDate;            // Used to get date to display
    Color handColor;             // Color of main hands and dial
    Color numberColor;           // Color of second hand and numbers

    public void init() {
	Button b1 = new Button("Show status");
	b1.setBounds(0, 0, 40, 40);
	b1.addActionListener(new ActionListener()
	    {	    
		public void actionPerformed(ActionEvent e)
		{
		    AppletContext ctx = getAppletContext();
		    ctx.showStatus("DONE SHOW STATUS");
		}
	    });
	Button b2 = new Button("Show document");
	b2.setBounds(0, 0, 40, 40);
	b2.addActionListener(new ActionListener()
	    {	    
		public void actionPerformed(ActionEvent e)
		{
		    AppletContext ctx = getAppletContext();
		    try {
		    ctx.showDocument(new URL("http://www.sun.com"));
		    } catch (Exception ex) {}
		}
	    });
	setLayout(new FlowLayout(FlowLayout.CENTER));
	
	add(b1);
	add(b2);
        resize(300,300);              // Set clock window size
    }

    public void start() {
        timer = new Thread(this);
        timer.start();
    }

    public void stop() {
        timer = null;
    }

    public void run() {
        Thread me = Thread.currentThread();
        while (timer == me) {
            try {
                Thread.currentThread().sleep(100);
            } catch (InterruptedException e) {
            }
        }
    }

    public void update(Graphics g) {
        paint(g);
    }

    public String getAppletInfo() {
        return "Title: A Clock \nAuthor: Rachel Gollub, 1995 \nAn analog clock.";
    }
  
    public String[][] getParameterInfo() {
        String[][] info = {
            {"bgcolor", "hexadecimal RGB number", "The background color. Default is the color of your browser."},
            {"fgcolor1", "hexadecimal RGB number", "The color of the hands and dial. Default is blue."},
            {"fgcolor2", "hexadecimal RGB number", "The color of the seconds hand and numbers. Default is dark gray."}
        };
        return info;
    }
}


