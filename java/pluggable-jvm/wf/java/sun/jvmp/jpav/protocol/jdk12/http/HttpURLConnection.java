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
 * $Id: HttpURLConnection.java,v 1.1 2001/05/10 18:12:30 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav.protocol.jdk12.http;

import java.net.URL;
import java.net.ProtocolException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.io.ByteArrayOutputStream;
import java.io.*;
import java.util.*;
import java.security.AccessController;
import java.security.PrivilegedExceptionAction;
import java.security.PrivilegedActionException;
import sun.net.*;
import sun.net.www.*;
import sun.jvmp.applet.*;
import sun.jvmp.jpav.*;

public class HttpURLConnection extends
 sun.net.www.protocol.http.HttpURLConnection {

    protected String proxy = null;
    protected int proxyPort = -1;

    // This is to keep trace of the failedOnce value in the super class. Since failedOnce
    // in the super one are declared private, we do some hack to get this value.
    boolean failedOnce = false;

    public HttpURLConnection(URL u, Handler handler)
    throws IOException {
	super(u, handler);
    }

    /** this constructor is used by other protocol handlers such as ftp
        that want to use http to fetch urls on their behalf. */
    public HttpURLConnection(URL u, String host, int port) 
    throws IOException {
	super(u, host, port);
        this.proxy = host;
        this.proxyPort = port;
    }

    static CookieHandler handler = null;

    /* setCookieHandler is used only when a particular CookieHandler is required
     * to determine the cookie value of a particular URL on the fly.
     */
    public static void setCookieHandler(CookieHandler h)  {
        handler = h;
    }

    protected boolean fromClassLoader() {
	Exception e = new Exception();
	ByteArrayOutputStream ba = new ByteArrayOutputStream();
	PrintStream pos = new PrintStream(ba);
	//e.printStackTrace(pos);
	String trace = ba.toString();
	StringTokenizer tok = new StringTokenizer(trace);
	String s = null;
	while(tok.hasMoreTokens()) {
	    s = tok.nextToken();
	    if ((s.startsWith("sun.applet.AppletClassLoader")) ||
		(s.startsWith("sun.applet.AppletResourceLoader")) || 
		(s.startsWith("sun.jvmp.jpav.ActivatorClassLoader"))) {
		return true;
	    }
	}
	return false;
    }
    
    protected boolean rightExt() {
	String fname = url.getFile();
	return (fname.endsWith(".jar") || fname.endsWith(".class"));
    }
    
    void privBlock() throws Exception {
	try {
	    if ("http".equals(url.getProtocol()) && !failedOnce) {
		http = HttpClient.New(url, proxy, proxyPort);
	    } else {
		// make sure to construct new connection if first attempt failed
		http = getProxiedClient(url, proxy, proxyPort);
	    }
	    ps = (PrintStream)http.getOutputStream();
	} catch (IOException e) {
	    throw e;
	}
    }
    
    // overridden in HTTPS subclass

    public synchronized void connect() throws IOException {
	if (connected)
	    return;

	// Determine proxy setting for the connection

        if (proxy == null) {
	   ProxyInfo pinfo = null;
	   ProxyHandler proxyHandler = ActivatorProxyHandler.getDefaultProxyHandler();

	   if (proxyHandler!=null) {
		pinfo = proxyHandler.getProxyInfo(url);

		if (pinfo != null)
		{
		    proxy = pinfo.getProxy();
		    proxyPort = pinfo.getPort();
		}
	   }
	}

	// Check whether the applet is allowed to connect to the host.
	// This is necessary because we will enable privilege to 
	// connect to the proxy
	SecurityManager m = System.getSecurityManager();
	if (m != null)	{
	    m.checkConnect(url.getHost(), url.getPort());
	}

        // Determine cookie value
        if (handler != null)
        {
	    // Use browser cookie only if the cookie has not 
	    // been set by the applet.
	    if (getRequestProperty("cookie") == null)
	    {
	        String cookie = handler.getCookieInfo(url);

		if (cookie != null)
		    setRequestProperty("cookie", cookie);
	    }
        }

	try {
	    AccessController.doPrivileged(new PrivilegedBlockAction(this));	
	} catch (PrivilegedActionException e) {
	    IOException ioe = (IOException)e.getException();
	    throw ioe;
	}
	    
        // Workaround for Mozilla bugs
        String workaround = (String) java.security.AccessController.doPrivileged(
                                        new sun.security.action.GetPropertyAction("mozilla.workaround"));

        if (workaround != null && workaround.equalsIgnoreCase("true"))
        {
            // Disable browser caching in Mozilla
            setUseCaches(false);
        }
        else
        {
	    // constructor to HTTP client calls openserver
	    setUseCaches(getUseCaches() && rightExt());
        }

	connected = true;
    }

    private InputStream cacheStream = null;

    public synchronized InputStream getInputStream() throws IOException{
	if (!connected) 
	    connect();
	if (useCaches) {
    	    try {
		// check if we are allowed to read files. If not, there is
		// no point to caching. Now, we have no idea where the
		// browser will put the file, so lets try to read
		// java.home property
		if (cacheStream != null)
		    return cacheStream;
	
		cacheStream = (InputStream)
		    AccessController.doPrivileged(new FileCreator(this));
		if (cacheStream==null) {
		    useCaches = false;
		} else {
		    return cacheStream;
		}
	    } catch (PrivilegedActionException e) { //IOException. fall through, and try
		//remote
		System.out.println("IO Exception, using remote copy");
		//e.printStackTrace();
		useCaches = false;
	    } catch (SecurityException e1) {
		System.out.println("Security exception, using remote copy");
		useCaches = false;
	    }  
	}
	return super.getInputStream();
    }

    /**
     * Create a new HttpClient object, bypassing the cache of
     * HTTP client objects/connections.
     *
     * @param url	the URL being accessed
     */
    protected sun.net.www.http.HttpClient getNewClient (URL url)
    throws IOException {

	// This is a hack to get the failedOnce value. Since getNewClient is only called by the 
	// super class when failedOnce is true, we can obtain the failedOnce value this way. 
	failedOnce = true;

	return getProxiedClient(url, proxy, proxyPort);
    }


    /**
     * Create a new HttpClient object, set up so that it uses
     * per-instance proxying to the given HTTP proxy.  This
     * bypasses the cache of HTTP client objects/connections.
     *
     * @param url	the URL being accessed
     * @param proxyHost	the proxy host to use
     * @param proxyPort	the proxy port to use
     */
    protected sun.net.www.http.HttpClient getProxiedClient (URL url, String proxyHost, int proxyPort)
    throws IOException {
	return new HttpClient (url, proxyHost, proxyPort);
    }

    /**
     * Method to get an internationalized string from the Activator resource.
     */
    public static String getMessage(String key) {
	    return key;
    }

    // Localization strings.
    private static ResourceBundle rb;

    class FileCreator implements PrivilegedExceptionAction {
	HttpURLConnection conn;

	FileCreator(HttpURLConnection c) {
	    conn = c;
	}

	public Object run() throws Exception {
	    
	    String fileName = null; //sun.plugin.CacheHandler.getCacheFile(conn.getURL());
	    if (fileName!=null)
	       return new BufferedInputStream(new FileInputStream(fileName));
	    else 
	       return null;
	}
    }

    class PrivilegedBlockAction implements PrivilegedExceptionAction {
	HttpURLConnection conn;
	PrivilegedBlockAction(HttpURLConnection c)
	{
	    conn = c;
	}

	public Object run() throws Exception {
	    conn.privBlock();
	    return null;
	}
    }

}


