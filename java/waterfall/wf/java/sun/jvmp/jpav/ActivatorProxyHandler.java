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
 * $Id: ActivatorProxyHandler.java,v 1.1 2001/05/09 17:29:59 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav;

import java.net.URL;
import java.util.HashMap;
import java.util.StringTokenizer;
import sun.jvmp.*;
import sun.jvmp.applet.*;


public class ActivatorProxyHandler implements ProxyHandler, ProxyType {

    private int proxyType = PROXY_TYPE_NO_PROXY;

    /* proxyList contains all the proxy information in the form of
     * "http=webcache1-cup:8080;ftp=webcache2-cup;gopher=javaweb:9090". Proxy
     * information for a particular protocol is seperated by ';'. If a port
     * number is specified, it is specified by using ':' following the proxy
     * host. If a specified protocol is not specified in the list, no proxy
     * is assumed. There is another form of this string which is "webcache-cup:8080".
     * In this case, it means all protocol will use this proxy (Apply-all).
     * Notice that no '=' is in the string in this case.
     */
    private String proxyList = null;

    /* proxyOverride contains all the domain which no proxy should be used when
     * a connection is made. For example, "a.eng,b.eng,c.*.eng". In this case,
     * if the host name is a.eng, b.eng, or c.c.eng, no proxy is used. Otherwise,
     * proxy information will be returned according to the protocol in the URL.
     * Note that wildcard can be used. If all local address should be bypassed,
     * a special string '<local>' is used.
     */
    private String proxyOverride = null;

    /**
     * <p> Proxy info cache.
     * </p>
     */
    private HashMap proxyTable = null;
    
    private BrowserSupport support = null;

    public ActivatorProxyHandler(int proxyType, 
				 String proxyList, 
				 String proxyOverride,
				 BrowserSupport sup)
    {
        this.proxyType = proxyType;
        this.proxyList = proxyList;
        this.proxyOverride = proxyOverride;
	this.support =  sup;
        proxyTable = new HashMap();
    }


    /* getProxyInfo is a function which takes a proxy-info-list, a
     * proxy-bypass-list, a URL, and returns the corresponding proxy information
     * with respect to the given URL.
     *
     * parameters :
     *	    u	      [in]   a string which contains all the proxy information
     *
     * out:
     *	    ProxyInfo [out]  ProxyInfo contains the corresponding proxy result.
     *
     */
    public synchronized ProxyInfo getProxyInfo(URL u)
    {
	ProxyInfo pinfo = null;
	
	try   {
	    pinfo = (ProxyInfo) proxyTable.get(u.getProtocol() 
					       + u.getHost() 
					       + u.getPort());

	    // XXX: is it OK?
	    if (pinfo != null)
		return pinfo;

            if (proxyType == PROXY_TYPE_NO_PROXY ||
    	       (proxyType == PROXY_TYPE_MANUAL && isProxyBypass(proxyOverride, u)))
		// Check if it is direct connect
		pinfo = new ProxyInfo(null, -1);
	    else if (proxyType == PROXY_TYPE_MANUAL)
	    {
		String socksInfo = null;

		// Extract info about SOCKS
		int i = proxyList.indexOf("socks=");
		if (i != -1)
		{
		    int j = proxyList.indexOf(";", i);
		    if (j == -1)
		        socksInfo = proxyList.substring(i + 6);
		    else
			socksInfo = proxyList.substring(i + 6, j);
		}

		
		if (proxyList.indexOf("=") == -1)
		{
		    // Apply all option
		    pinfo = new ProxyInfo(proxyList);
		}
		else 
		{		    
	    	    // Parse proxy list
		    String protocol = u.getProtocol();

		    i = proxyList.indexOf(protocol + "=");
		    if (i == -1)
			// Cannot find the protocol proxy
			pinfo = new ProxyInfo(null, socksInfo);
		    else
		    {
			int j = proxyList.indexOf(";", i);

			if (j == -1) {
			    pinfo = new ProxyInfo(proxyList.substring(i + protocol.length() + 1), socksInfo);
			}
			else {
			    pinfo = new ProxyInfo(proxyList.substring(i + protocol.length() + 1, j), socksInfo);
			}
		    }
		}
	    }
	    else
	    {
		if (support == null)
		    pinfo = null;
		else
		    pinfo = support.getProxyInfoForURL(u);
	    }

	    proxyTable.put(u.getProtocol() + u.getHost() + u.getPort(), pinfo);
	}
	catch (Exception e)   {
	    PluggableJVM.trace("Proxy is not configured", 
			       PluggableJVM.LOG_ERROR);
	    PluggableJVM.trace(e, PluggableJVM.LOG_ERROR);
	    pinfo = new ProxyInfo(null, -1);
	}

	return pinfo;
    }

    
    /* isProxyBypass is a function which takes a proxy override list and a URL
     * , and returns true if the hostname matches the proxy override list;
     * otherwise, false is returned.
     *
     * parameters :
     *  proxyOverride      [in]  a string which contains the proxy override list
     *	u   	           [in]  a URL which contains the hostname
     *
     * out:
     *	boolean		   [out] if the proxy override list matches the hostname,
     *				 true is returned. Otherwise, false is returned.
     *
     * Notes: i) proxyOverride contains all the domain which no proxy should be
     *        used when a connection is made. For example, "a.eng,b.eng,c.*.eng".
     *        In this case, if the host name is a.eng, b.eng, or c.c.eng, no
     *        proxy is used. Otherwise, proxy information will be returned
     *        according to the protocol in the URL. Note that wildcard can be
     *        used. If all local address should be bypassed, a special string
     *        '<local>' is used.
     *
     */
    private boolean isProxyBypass(String proxyOverride, URL u)
    {
        if (proxyOverride == null || proxyOverride.equals(""))
            return false;

        String host = u.getHost();

        // Extract proxy-override list
	StringTokenizer st = new StringTokenizer(proxyOverride, ",", false);
	while (st.hasMoreTokens()) 
	{  
	    String pattern = st.nextToken();     

            if (pattern.equals("<local>") && host.indexOf(".") == -1)
                return true;
            else if (shExpMatch(host, pattern))
                return true;
	}

        return false;
    }


    /* shExpMatch is a function which takes a string and a pattern, and returns
     * true if the string matches the pattern; otherwise, false is returned.
     *
     * parameters :
     *  str         [in]  a string which is used for pattern matching
     *	shexp       [in]  a string which contains the pattern
     *
     * out:
     *	boolean	    [out] if the string match the pattern, true is returned.
     *                    otherwise, false is returned.
     *
     * Notes: i) shexp contains the pattern which may include the wildcard '*'.
     *	      ii) The pattern matching is case-insensitive.
     *
     */
    private boolean shExpMatch(String str, String shexp)
    {
	try  {
	    // Convert the string to lower case
	    str = str.toLowerCase();
	    shexp = shexp.toLowerCase();

	    return shExpMatch2(str, shexp);
	} catch (Throwable e)	{

	    // Error recovery
	    PluggableJVM.trace(e, PluggableJVM.LOG_WARNING);
	    return false;
	}	
    }

    /* shExpMatch2 is a function which takes a string and a pattern, and returns
     * true if the string matches the pattern; otherwise, false is returned.
     *
     * parameters :
     *  str         [in]  a string which is used for pattern matching
     *	shexp       [in]  a string which contains the pattern
     *
     * out:
     *	boolean	    [out] if the string match the pattern, true is returned.
     *                    otherwise, false is returned.
     *
     * Notes: i) shexp contains the pattern which may include the wildcard '*'.
     *
     *	      ii) This is the actual implementation of the pattern matching 
     *		  algorithm.
     *
     */
    private boolean shExpMatch2(String str, String shexp)
    {
	// NULL is not a valid input.
	//
        if (str == null || shexp == null)
    	    return false;

	if (shexp.equals("*"))
	    return true;

	// Check the position of the wildcard
	int index = shexp.indexOf('*');

	if (index == -1)
	{
	    // No wildcard anymore
	    return str.equals(shexp);
	}
	else if (index == 0)
	{
	    // Wildcard at the beginning of the pattern

	    for (int i=0; i <= str.length(); i++)
	    {
		// Loop through the string to see if anything match.
		if (shExpMatch2(str.substring(i), shexp.substring(1)))
		    return true;
	    }

	    return false;
	}
	else
	{
	    // index > 0
	    String sub = null, sub2 = null;

	    sub = shexp.substring(0, index);

	    if (index <= str.length())
		sub2 = str.substring(0, index);

	    if (sub != null && sub2 != null && sub.equals(sub2))
		return shExpMatch2(str.substring(index), shexp.substring(index));
	    else
		return false;
	}
    }

    /* extractAutoProxySetting is a function which takes a proxy-info-string
     * which returned from the JavaScript function FindProxyForURL, and returns
     * the corresponding proxy information.
     *
     * parameters :
     *	s         [in]	a string which contains all the proxy information
     *
     * out:
     *   ProxyInfo [out] ProxyInfo contains the corresponding proxy result.
     *
     * Notes: i) s contains all the proxy information in the form of
     *        "PROXY webcache1-cup:8080;SOCKS webcache2-cup". There are three
     *        possible values inside the string:
     *	        a) "DIRECT" -- no proxy is used.
     *          b) "PROXY"  -- Proxy is used.
     *          c) "SOCKS"  -- SOCKS support is used.
     *        Information for each proxy settings are seperated by ';'. If a
     *	      port number is specified, it is specified by using ':' following
     *        the proxy host.
     *
     */
    private ProxyInfo extractAutoProxySetting(String s)
    {
	if (s != null)
	{
	    StringTokenizer st = new StringTokenizer(s, ";", false);
	    if (st.hasMoreTokens()) 
	    {  
		String pattern = st.nextToken();     

		int i = pattern.indexOf("PROXY");

		if (i != -1)   {
		    // "PROXY" is specified
		    return new ProxyInfo(pattern.substring(i + 6));
		}

		i = pattern.indexOf("SOCKS");

		if (i != -1) 
		{
		    // "SOCKS" is specified
		    return new ProxyInfo(null, pattern.substring(i + 6));
		}
	    }
	}

        // proxy string contains 'DIRECT' or unrecognized text
        return new ProxyInfo(null, -1);
    }
    
    // The current default proxy handler
    private static ProxyHandler handler; 

    /*
     * <p>
     * @return the default proxy handler installed
     * </p>
     *
     */
    public static ProxyHandler getDefaultProxyHandler() {
        return handler;
    }

    /**
     * <p>
     * Set the default proxy handler reference
     * <p>
     * 
     * @paran newDefault new default proxy handler
     */
    public static void setDefaultProxyHandler(ProxyHandler newDefault) {
        handler = newDefault;
    }
}











