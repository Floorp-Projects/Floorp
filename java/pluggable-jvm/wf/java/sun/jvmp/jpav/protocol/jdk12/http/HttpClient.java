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
 * $Id: HttpClient.java,v 1.1 2001/05/10 18:12:30 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav.protocol.jdk12.http;

import java.io.*;
import java.net.*;
import java.util.*;
import sun.net.NetworkClient;
import sun.net.ProgressEntry;
import sun.net.ProgressData;
import sun.net.www.MessageHeader;
import sun.net.www.HeaderParser;
import sun.net.www.MeteredStream;
import sun.misc.Regexp;
import sun.misc.RegexpPool;
import sun.jvmp.applet.ProxyInfo;
import sun.jvmp.jpav.*;

public class HttpClient extends sun.net.www.http.HttpClient {

    /* This package-only CTOR should only be used for FTP piggy-backed on HTTP 
     * HTTP URL's that use this won't take advantage of keep-alive.
     * Additionally, this constructor may be used as a last resort when the
     * first HttpClient gotten through New() failed (probably b/c of a 
     * Keep-Alive mismatch).
     *
     * XXX That documentation is wrong ... it's not package-private any more
     */
    public HttpClient(URL url, String proxy, int proxyPort)
			throws IOException {
	super (url, proxy, proxyPort);
    }

    /* This class has no public constructor for HTTP.  This method is used to
     * get an HttpClient to the specifed URL.  If there's currently an 
     * active HttpClient to that server/port, you'll get that one.
     */
    public static HttpClient New(URL url, String proxy, int proxyPort) 
    throws IOException {
	/* see if one's already around */
	HttpClient ret = (HttpClient) kac.get(url);
	if (ret == null) {
	    ret = new HttpClient (url, proxy, proxyPort);  // CTOR called openServer()
	} else {
	    ret.url = url;
	}
	// don't know if we're keeping alive until we parse the headers
	// for now, keepingAlive is false
	return ret;
    }

    /**
     * Return a socket connected to the server, with any
     * appropriate options pre-established
     */
    protected Socket doConnect (String server, int port)
    throws IOException, UnknownHostException {

	ProxyHandler handler = sun.jvmp.jpav.ActivatorProxyHandler.getDefaultProxyHandler();
	if (handler != null)
	{
	    ProxyInfo pinfo = handler.getProxyInfo(url);

	    if (pinfo != null && pinfo.isSocksUsed())
	    {
		// Use SOCKS !!
	        return new SocksSocket(server, port, pinfo.getSocksProxy(), pinfo.getSocksPort());
	    }
	}

	return super.doConnect(server, port);
    }
}
