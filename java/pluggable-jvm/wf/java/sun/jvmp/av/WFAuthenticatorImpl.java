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
 * $Id: WFAuthenticatorImpl.java,v 1.1 2001/07/12 20:26:07 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.av;

import java.net.*;
import  java.security.*;
import java.awt.*;
import java.awt.event.*;

public class WFAuthenticatorImpl extends Authenticator
{

    protected synchronized PasswordAuthentication getPasswordAuthentication()
    {
	PasswordAuthentication pass = null;
        try
	    {
		InetAddress host = getRequestingSite();
		String hostName = "<<unknown>>";
		if (host != null) hostName = host.toString();
		pass = askUser(hostName, 
			       getRequestingPrompt(), 
			       getRequestingScheme());
	    }
	catch (Exception e)
	    {
		// do nothing here
		e.printStackTrace();
	    }
	return pass;
    }

    protected PasswordAuthentication askUser(String host,
					     String prompt,
					     String scheme)
    {
	AuthDialog dlg = new AuthDialog(host, prompt, scheme);
	return dlg.askUser();	
    }
}


class AuthDialog extends Frame
{
    TextField                 user;
    TextField                 passwd;
    PasswordAuthentication    auth;

    AuthDialog(String host, 
	       String prompt, 
	       String scheme)
    {
	super("Authentication dialog");
	//System.err.println("h="+host+" p="+prompt+" s="+scheme);
	setLayout(new BorderLayout());
	setResizable(true);
	Panel p1 = new Panel();
	p1.setLayout(new BorderLayout());	
	user = new TextField(15);
	passwd = new TextField(15);
	passwd.setEchoChar('*');
	p1.add(new Label("Auth request for "+host), 
	       BorderLayout.NORTH);
	p1.add(new Label(prompt), BorderLayout.SOUTH);
	
	Panel p2 = new Panel();
	p2.setLayout(new BorderLayout());
	Panel p21 = new Panel();
	p21.setLayout(new FlowLayout(FlowLayout.LEFT));
	p21.add(new Label("Username: "));
	p21.add(user);
	Panel p22 = new Panel();
	p22.setLayout(new FlowLayout(FlowLayout.LEFT));
	p22.add(new Label("Password: "));
	p22.add(passwd);
	p2.add(p21, BorderLayout.NORTH);
	p2.add(p22, BorderLayout.SOUTH);
	
	Panel p3 = new Panel();
	p3.setLayout(new FlowLayout(FlowLayout.CENTER));
	Button ok = new Button("OK");
	ok.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e)  {
		    synchronized (user) {
			auth = new PasswordAuthentication(user.getText(), 
							  passwd.getText().toCharArray());
			user.notify();
		    }
		}
	    });
	p3.add(ok);
	Button cancel = new Button("Cancel");
	cancel.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e)  {
		    auth = null;
		    synchronized (user) {
			//			System.err.println("u="+user.getText()+" p="+passwd.getText());
			user.notify();
		    }		    
		}
	    });
	p3.add(cancel);	
			
	add(p1, BorderLayout.NORTH);
	add(p3, BorderLayout.SOUTH);
	add(p2, BorderLayout.CENTER);
	setSize(320,180);		
    } 

    PasswordAuthentication askUser()
    {
	show();
	try {
	    synchronized (user) {
		user.wait();
	    }
	} catch (InterruptedException e) {}       
	hide();	
	return auth;
    }
}








