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
 * $Id: MozillaPeerFactory.java,v 1.3 2001/07/12 20:26:18 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.mozilla;

import sun.jvmp.*;
import sun.jvmp.security.*;
import sun.jvmp.applet.*;
import java.net.*;
import java.util.StringTokenizer;
import java.util.*;
import java.security.*;
import java.lang.reflect.*;

public class MozillaPeerFactory implements HostObjectPeerFactory,
					   AccessControlDecider,
					   BrowserSupport
{
    protected final String cid = "@sun.com/wf/mozilla/appletpeer;1";
    protected final int    version  = 1;
    PluggableJVM           m_jvm = null;
    protected long         m_params = 0;
    protected int          m_id;
    private static boolean initialized = false;
    protected URL[]        m_codebase;
    AppletManager          m_mgr;
    Constructor            jsConstructor;

    protected MozillaPeerFactory(PluggableJVM jvm, long data) 
	throws Exception
    {
	loadLibrary();
	m_jvm = jvm;
	m_params = data;
	/* as we could load only one factory, this will allow
	   generic operations, like JS evaluation, to be performed without
	   any applets at all */ 
	try {
	    Class jsClass = Class.forName("sun.jvmp.mozilla.JSObject");
	    jsConstructor = jsClass.getDeclaredConstructor(new Class[]{Long.class});        
	    Field eval = jsClass.getDeclaredField("m_evaluator");
	    eval.setLong(null, data);
	} catch (Exception e) {
	    jsConstructor = null;
	    jvm.trace("Liveconnect not inited - sorry", PluggableJVM.LOG_ERROR);
	    jvm.trace(e, PluggableJVM.LOG_ERROR);
	}

	m_mgr = new AppletManager(jvm);	
	try {
	    m_codebase = null;
	    URLClassLoader cl = 
		(URLClassLoader)this.getClass().getClassLoader();
	    if (cl != null) m_codebase = cl.getURLs(); 
	} catch (ClassCastException e) {
	    // do nothing here
	}
	System.setSecurityManager(new MozillaSecurityManager());
    }

    netscape.javascript.JSObject getJSObject(long params)
    {
        if (jsConstructor == null) return null;
        netscape.javascript.JSObject jso = null;
        try {
            Object o = jsConstructor.newInstance(new Object[]{new Long(params)});
            jso = (netscape.javascript.JSObject)o;
        } catch (Exception e) {
            m_jvm.trace(e, PluggableJVM.LOG_ERROR);
        }
        return jso;
    }

    public static HostObjectPeerFactory start(PluggableJVM jvm, Long data)
    {
	MozillaPeerFactory factory = null;
	try {
	    factory = new MozillaPeerFactory(jvm, data.longValue());
	}  catch (Exception e) {
	    jvm.trace("MOZILLA FACTORY NOT CREATED", PluggableJVM.LOG_ERROR);
	    e.printStackTrace();
	    return null;
	}
	return factory;
    }

    public String getCID()
    {
	return cid;
    }

    public HostObjectPeer create(int version)
    {
	
	return new MozillaHostObjectPeer(this, version);
    }
    
    public int handleEvent(SecurityCaps caps, 
			   int          eventID,
			   long         eventData)
    {
	return 1;
    }
    
    private native int nativeHandleCall(int arg1, long arg2);
    
    public int  handleCall(SecurityCaps caps, int arg1, long arg2) 
    {
	int rv = 0;
	if (arg1 > 0) 
	    rv = nativeHandleCall(arg1, arg2);
	else
	    rv = 1;
	return rv;
    }
    
    public int destroy(SecurityCaps caps)
    {
	return 1;
    }

    public PermissionCollection getPermissions(CodeSource codesource)
    {
	Permissions perms = new Permissions();
	URL         location = codesource.getLocation();
	if (m_codebase != null &&  location != null) {
	    for (int i=0; i<m_codebase.length; i++)
		{		    
		    // XXX: maybe I'm totally wrong here
		    if (location.equals(m_codebase[i])) 
			{	
			    PluggableJVM.trace("extension granted all permissions to own codesource "+codesource,
					       PluggableJVM.LOG_WARNING);
			    perms.add(new java.security.AllPermission());
			    return perms;
			}
		}
	}
	perms.add(new RuntimePermission("accessClassInPackage.sun.jvmp.mozilla"));	
	return perms;
    }
    
    private void loadLibrary() throws UnsatisfiedLinkError
    {
	String libname = "wf_moz6";
	try 
	    {
		System.loadLibrary(libname);
	  } 
	catch (UnsatisfiedLinkError ex) 
	    {
		PluggableJVM.trace("System could not load DLL: " + libname,
				   PluggableJVM.LOG_ERROR);
		PluggableJVM.trace("Path is:" + 
				   System.getProperty("java.library.path"),
				   PluggableJVM.LOG_WARNING);
		PluggableJVM.trace(ex.toString(),
				   PluggableJVM.LOG_WARNING);
		throw ex;
	    };
    }
    
    /**
     * AccessControlDecider methods
     */
    public int  decide(CallingContext ctx, String principal, int cap_no)
    {
	return NA;
    }
    
    public boolean belongs(int cap_no)
    {
	return false;
    }

    /**
     * BrowserSupport methods
     */
    public ProxyInfo getProxyInfoForURL(URL url)
    {
	String s = nativeGetProxyInfoForURL(url.toString());
	m_jvm.trace("Proxy string for:\""+
		    url.toString() +"\" is \""+s+"\"",
		    PluggableJVM.LOG_DEBUG);
	ProxyInfo info = extractAutoProxySetting(s);
	return info;
    }
    
    public URLConnection getConnectionForURL(URL url)
    {
	return null;
    }
    

    private native String nativeGetProxyInfoForURL(String s);
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

    public int getID() 
    {
	return m_id;
    }
    
    public void setID(int id)
    {
	if (m_id != 0) return;
	m_id = id;       
    }
}





