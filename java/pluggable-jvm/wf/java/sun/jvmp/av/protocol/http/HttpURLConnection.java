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
 * $Id: HttpURLConnection.java,v 1.1 2001/07/12 20:26:11 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.av.protocol.http;

import java.io.*;
import java.util.*;
import java.security.*;
import sun.net.*;
import java.net.*;
import sun.net.www.*;
import sun.jvmp.applet.*;
import sun.jvmp.av.*;

class HttpURLConnection extends sun.net.www.protocol.http.HttpURLConnection
{
    protected String proxy = null;
    protected int proxyPort = -1;

    public HttpURLConnection(URL u, Handler handler)
	throws IOException 
    {
        super(u, handler);
	ProxyHandler p = WFProxyHandler.getDefaultProxyHandler();
        ProxyInfo    pi = (p != null) ? p.getProxyInfo(u) : null;
        if (pi != null) 
	    {
		this.proxy = pi.getProxy();
		this.proxyPort = pi.getPort();
		//System.err.println("for url="+url+" proxy="+proxy);
	    }
    }

    public synchronized void connect() 
	throws IOException 
    {
        if (connected) return;
	try {
            AccessController.doPrivileged(new PrivilegedExceptionAction()
		{
		    public Object run() throws IOException {
			http = new sun.net.www.http.HttpClient(url, proxy, proxyPort);
			ps = (PrintStream)http.getOutputStream(); 
			return null;
                }
		});
        } catch (PrivilegedActionException e) {
            IOException ioe = (IOException)e.getException();
	    System.err.println("rethrown security exception");
            throw ioe;
        }
	connected = true;
    }
    
    public synchronized InputStream getInputStream() 
	throws IOException
    {
        if (!connected) connect();
	return super.getInputStream();
    }

    public Permission getPermission() throws IOException
    {
	if (proxy != null)
	    {
		return  new java.net.SocketPermission(proxy+":"+proxyPort,
						      "connect,accept,resolve");
	    }
	return null;
    }
}









