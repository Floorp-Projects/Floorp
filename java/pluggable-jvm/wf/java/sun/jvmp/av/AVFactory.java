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
 * $Id: AVFactory.java,v 1.1 2001/07/12 20:26:05 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.av;

import sun.jvmp.*;
import sun.jvmp.applet.*;
import java.util.*;
import java.security.*;
import java.net.*;
import java.awt.Frame;
import java.applet.Applet;


public class AVFactory implements sun.jvmp.ObjectFactory
{
    Vector         cids;
    Hashtable      permsHash;
    Permissions    permsForAll;
    boolean        inited=false;
    CodeSource     codesource;
    PluggableJVM   jvm;
    static AVFactory instance = null;
    
    public static ObjectFactory getFactory(PluggableJVM jvm,
					   CodeSource codesource)
	throws ComponentException
    {
	if (instance == null)
	    instance = new AVFactory(jvm, codesource);
	return instance;
    }
    

    protected AVFactory(PluggableJVM jvm, CodeSource codesource)
	throws ComponentException
    {
	cids = new Vector();
	cids.addElement(WFAppletViewer.CID);
	permsHash = new Hashtable();
	this.jvm = jvm;
	this.codesource = codesource;
	// XXX: rewrite with policy file
	permsForAll = new Permissions();
	permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.av"));
	permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.av.protocol.http"));
	permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.av.protocol.ftp"));
	permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.av.protocol.jar"));
	permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.av.protocol.https"));
	permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.av.protocol.gopher"));
	jvm.registerProtocolHandlers(this.getClass().getClassLoader(), "sun.jvmp.av.protocol");
    }
    
    public String         getName()
    {
        return "Waterfall applet viewer";
    }
 
    public Enumeration    getCIDs()
    {
	return cids.elements();
    }
    
    public boolean        handleConflict(String cid,
					 ObjectFactory f)
    {
	return true;
    }
 
    public Object         instantiate(String cid, Object arg)
    {
	AppletViewerArgs a;
	try {
	    a = (AppletViewerArgs)arg;
	} catch (ClassCastException e) {
	    return null;
	}
	initEnvironment(a.support);
	if ((cid == null) || (!WFAppletViewer.CID.equals(cid)) || (a == null))
	    return null;
	return new WFAppletViewerImpl(a.jvm, a.ctx, a.support, a.atts);
    }

    public PermissionCollection getPermissions(CodeSource codesource)
    {
	// pity, but no clone() here
	Permissions p = new Permissions();
	for (Enumeration e=permsForAll.elements(); e.hasMoreElements(); )
	    p.add((Permission)e.nextElement());
	if (codesource != null && codesource.getLocation() != null)
	    {
		Permissions p1 = (Permissions)permsHash.get(codesource);
		if (p1 != null)
		    {
			for (Enumeration e=p1.elements(); e.hasMoreElements(); )
			    p.add((Permission)e.nextElement());
		    }
	    }
	return p;
    }
    
    void initEnvironment(BrowserSupport support)
    {
	if (inited) return;
	Properties props = new Properties(System.getProperties()); 
        props.put("browser", "sun.jvmp");
        props.put("browser.vendor", "Sun Microsystems Inc.");
        props.put("http.agent", "Waterfall Applet Viewer/1.0");
        props.put("package.restrict.access.sun", "true");
        props.put("package.restrict.access.netscape", "false");
        props.put("package.restrict.definition.java", "true");
        props.put("package.restrict.definition.sun", "true");
        props.put("package.restrict.definition.netscape", "true");
        props.put("java.version.applet", "true");
        props.put("java.vendor.applet", "true");
	props.put("java.vendor.url.applet", "true");
        props.put("java.class.version.applet", "true");
        props.put("os.name.applet", "true");
        props.put("os.version.applet", "true");
        props.put("os.arch.applet", "true");
        props.put("file.separator.applet", "true");
        props.put("path.separator.applet", "true");
        props.put("line.separator.applet", "true");       
	System.setProperties(props);
	ProxyHandler handler
	    = getDefaultProxyHandler(ProxyType.PROXY_TYPE_AUTOCONFIG,
                                     null,
                                     null,
                                     support);
	try {
            WFProxyHandler.setDefaultProxyHandler(handler);
	    Authenticator.setDefault(new WFAuthenticatorImpl());
        } catch(Throwable e) {
            PluggableJVM.trace(e, PluggableJVM.LOG_WARNING);
        }
    }
    
    protected static ProxyHandler
        getDefaultProxyHandler(int proxyType,
                               String proxyList,
                               String proxyOverride,
                               BrowserSupport ctx)
    {
        return new WFProxyHandler(proxyType,
				  proxyList,
				  proxyOverride,
				  ctx);
    }
}
