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
 * $Id: ProxyInfo.java,v 1.1 2001/07/12 20:25:56 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.applet;

public class ProxyInfo  {
    String     proxyHost;
    int        proxyPort = -1;
    String     socksProxyHost;
    int        socksProxyPort = -1;

    public ProxyInfo(String pinfo)  {
	this(pinfo, null);
    }

    public ProxyInfo(String pinfo, String spinfo)  {
	// handle HTTP proxy settings
	// accepted format is http://somehost.somedomain.com:port
	if (pinfo != null) 
	    {
		int i = pinfo.indexOf("//");
		// XXX: should I do smth meaningful with protocol
		if (i >= 0) pinfo = pinfo.substring(i + 2);
		i =  pinfo.indexOf(":");
		if (i < 0) 
		    {
			proxyHost = pinfo;
			proxyPort = 3128; // or 80?
		    }
		else
		    {	
			proxyHost = pinfo.substring(0, i);
			try {
			    proxyPort = Integer.parseInt(pinfo.substring(i + 1));
			} catch (NumberFormatException e) {
			    proxyPort = 3128;
			}
		    }
	    }

	// handle SOCKS proxy settings
	// accepted format is host:port
	if (spinfo != null)
	    {
		int i =  spinfo.indexOf(":");
		if (i < 0) 
		    {
			socksProxyHost = spinfo;
			socksProxyPort = 1080;
		    }
		else
		    {	
			socksProxyHost = spinfo.substring(0, i);
			try {
			    socksProxyPort 
				= Integer.parseInt(spinfo.substring(i + 1));
			} catch (NumberFormatException e) {
			    socksProxyPort = 1080;
			}
		    }
	    }
    }

    public ProxyInfo(String proxy, int port)  {
	this(proxy, port, null, -1);
    }

    public ProxyInfo(String proxy, int port, String socksProxy, int socksPort) 
    {
        this.proxyHost = proxy;
        this.proxyPort = port;
	this.socksProxyHost = socksProxy;
	this.socksProxyPort = socksPort;
    }

    public String getProxy()  {
        return proxyHost;
    }

    public int getPort()  {
        return proxyPort;
    }

    public String getSocksProxy() {
	return socksProxyHost;
    }

    public int getSocksPort() {
	return socksProxyPort;
    }

    public boolean isProxyUsed() {
	return (proxyHost != null || socksProxyHost != null);
    }

    public boolean isSocksUsed() {
	return (socksProxyHost != null);
    }
}
