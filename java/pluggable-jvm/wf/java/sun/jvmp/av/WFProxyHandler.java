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
 * $Id: WFProxyHandler.java,v 1.1 2001/07/12 20:26:07 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.av;

import java.net.URL;
import java.util.Hashtable;
import java.util.StringTokenizer;
import sun.jvmp.*;
import sun.jvmp.applet.*;


public class WFProxyHandler implements ProxyHandler, ProxyType {

    private int proxyType = PROXY_TYPE_NO_PROXY;

    private String proxyList = null;
    private String proxyOverride = null;
    private Hashtable proxyCache = null;
    private BrowserSupport support = null;

    public WFProxyHandler(int proxyType, 
			  String proxyList, 
			  String proxyOverride,
			  BrowserSupport sup)
    {
        this.proxyType = proxyType;
        this.proxyList = proxyList;
        this.proxyOverride = proxyOverride;
	this.support =  sup;
	// XXX: implement me
	if (proxyType == PROXY_TYPE_MANUAL) 
	    {
		PluggableJVM.trace("Manual proxy config not supported yet", 
				   PluggableJVM.LOG_ERROR);
	    }
	proxyCache = new Hashtable();
    }

    public synchronized ProxyInfo getProxyInfo(URL u)
    {
	ProxyInfo pinfo = null;
	
	try   {
	    pinfo = (ProxyInfo) proxyCache.get(u.getProtocol() 
					       + u.getHost() 
					       + u.getPort());

	    // currently the only way to get proxy is to ask it from browser
	    if (pinfo != null)
		return pinfo;
	    if (support == null)
		    pinfo = null;
	    else
		pinfo = support.getProxyInfoForURL(u);	    
	    proxyCache.put(u.getProtocol() + u.getHost() + u.getPort(), pinfo);
	}
	catch (Exception e)   {
	    PluggableJVM.trace("Proxy is not configured", 
			       PluggableJVM.LOG_ERROR);
	    PluggableJVM.trace(e, PluggableJVM.LOG_ERROR);
	    pinfo = new ProxyInfo(null, -1);
	}
	return pinfo;
    }

    // The current default proxy handler
    private static ProxyHandler handler; 

    public static ProxyHandler getDefaultProxyHandler() 
    {
        return handler;
    }

    public static void setDefaultProxyHandler(ProxyHandler newDefault) 
    {
        handler = newDefault;
    }
}











