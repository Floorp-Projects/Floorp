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
 * $Id: JavaPluginAVFactory.java,v 1.1 2001/05/09 17:29:59 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav;

import sun.jvmp.*;
import sun.jvmp.applet.*;
import java.util.*;
import java.security.*;
import java.net.URL;
import java.awt.Frame;
import java.applet.Applet;
import java.rmi.server.RMISocketFactory;
import java.io.*;

public class JavaPluginAVFactory implements sun.jvmp.ObjectFactory
{
  Vector         cids;
  Hashtable      permsHash;
  Permissions    permsForAll;
  boolean        inited=false;
  CodeSource     codesource;
  PluggableJVM   jvm;
  static JavaPluginAVFactory instance = null;

 
  public static ObjectFactory getFactory(PluggableJVM jvm,
					 CodeSource codesource) 
      throws ComponentException             
  {
    if (instance == null) 
	instance = new JavaPluginAVFactory(jvm, codesource); 
    return instance;
  }

  protected JavaPluginAVFactory(PluggableJVM jvm, CodeSource codesource) 
      throws ComponentException
  {
     cids = new Vector();
     cids.add(WFAppletViewer.CID);
     permsHash = new Hashtable();
     this.jvm = jvm;
     this.codesource = codesource;
     /*  Permissions p = new Permissions();
       // XXX: correct?
       //p.add(new java.security.AllPermission());
       permsHash.put(codesource, p);
     */
     // XXX: rewrite with policy file
     permsForAll = new Permissions();
     permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.jpav"));
     permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.jpav.protocol.jdk12.http")); 
     permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.jpav.protocol.jdk12.ftp"));
     permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.jpav.protocol.jdk12.jar"));
     permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.jpav.protocol.jdk12.https"));
     permsForAll.add(new java.lang.RuntimePermission("accessClassInPackage.sun.jvmp.jpav.protocol.jdk12.gopher"));     
  }
  
  public String         getName() 
    { 
	return "Java Plugin applet viewer"; 
    }
    
  public Enumeration    getCIDs()
  {
    return cids.elements();
  }
  
  public boolean        handleConflict(String cid, 
				       ObjectFactory f)
  {
    return false;
  }
  
  public Object         instantiate(String cid, Object arg)
  {
    AppletViewerArgs a;
    try {
      a = (AppletViewerArgs)arg;
    } catch (ClassCastException e) {
      return null;
    }
    if (!inited) initEnvironment(a.support);
    if ((cid == null) || (!WFAppletViewer.CID.equals(cid)) || (a == null)) 
      return null;
    return new TempAV(a.jvm, a.ctx, a.support, a.atts);
  }

  public PermissionCollection getPermissions(CodeSource codesource)
  {
    // pity, but no clone() here
    Permissions p = new Permissions();
    for (Enumeration e=permsForAll.elements(); e.hasMoreElements(); )
	p.add((Permission)e.nextElement());
    if (codesource != null) 
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
    
   /**
     * Prepare the enviroment for executing applets.
     */
  void initEnvironment(BrowserSupport support)
    {	
	// if we are already initialized, just return
	if (inited) return;
	// Get our internationalization resources.  (Make sure this is
        // done before calling showConsoleWindow.)
        //rb = ResourceBundle.getBundle("sun.plugin.resources.Activator");
	//rb = null;	
	/**
	 * maybe move all this stuff to plugin.policy file 
	 **/
	Properties props = new Properties(System.getProperties());
	// Define a number of standard properties
	props.put("acl.read", "+");
	props.put("acl.read.default", "");
	props.put("acl.write", "+");
	props.put("acl.write.default", "");	
	// Standard browser properties
	props.put("browser", "sun.jvmp");
	//props.put("browser.version", theVersion);
	props.put("browser.vendor", "Sun Microsystems Inc.");
	props.put("http.agent", "Waterfall Applet Viewer/1.0");	
	// Define which packages can NOT be accessed by applets
	props.put("package.restrict.access.sun", "true");	
	// 
	// This is important to set the netscape package access to "false". 
	// Some applets running in IE and NS will access 
	// netscape.javascript.JSObject sometimes. If we set this 
	// restriction to "true", these applets will not run at all. 
	// However, if we set it to "false", the applet may continue 
	// to run by catching an exception.
	props.put("package.restrict.access.netscape", "false");

	// Define which packages can NOT be extended by applets
	props.put("package.restrict.definition.java", "true");
	props.put("package.restrict.definition.sun", "true");
	props.put("package.restrict.definition.netscape", "true");
	
	// Define which properties can be read by applets.
	// A property named by "key" can be read only when its twin
	// property "key.applet" is true.  The following ten properties
	// are open by default.  Any other property can be explicitly
	// opened up by the browser user setting key.applet=true in
	// ~/.java/properties.   Or vice versa, any of the following can
	// be overridden by the user's properties.
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
	props.remove("proxyHost");
	props.remove("proxyPort");
        props.remove("http.proxyHost");
        props.remove("http.proxyPort");
        props.remove("https.proxyHost");
        props.remove("https.proxyPort");
        props.remove("ftpProxyHost");
        props.remove("ftpProxyPort");
        props.remove("ftpProxySet");
        props.remove("gopherProxyHost");
        props.remove("gopherProxyPort");
        props.remove("gopherProxySet");
	props.remove("socksProxyHost");
	props.remove("socksProxyPort");
	
	// Set allow default user interaction in HTTP/HTTPS
	java.net.URLConnection.setDefaultAllowUserInteraction(true);
	
	// Make sure proxy is detected on the fly in http, ftp and gopher.
	ProxyHandler handler 
	    = getDefaultProxyHandler(ActivatorAppletPanel.PROXY_TYPE_AUTOCONFIG,
				     null,
				     null,
				     support);
	
	try {
	    ActivatorProxyHandler.setDefaultProxyHandler(handler); 	       
	} catch(Throwable e) {
	    PluggableJVM.trace(e, PluggableJVM.LOG_WARNING);
	}
	
	// Set RMI socket factory for proxy.
	try {
	    RMISocketFactory.setSocketFactory(new RMIActivatorSocketFactory());
	}
	catch (IOException e)  {
	}
	System.setProperties(props);	
	inited=true;	
    }
    
    protected static ProxyHandler 
	getDefaultProxyHandler(int proxyType, 
			       String proxyList, 
			       String proxyOverride,
			       BrowserSupport ctx) 
    {
	return new ActivatorProxyHandler(proxyType,
					 proxyList,
					 proxyOverride,
					 ctx);
    }
}

class TempAV implements WFAppletViewer
{
  ActivatorAppletPanel av;
  TempAV(PluggableJVM jvm, WFAppletContext ctx, 
	 BrowserSupport support, Hashtable atts)
  {
    av = new ActivatorAppletPanel(jvm, ctx, atts);
  }

  public void   startApplet()
  {
      av.appletStart();
  }

  public void   stopApplet()
  {
      av.appletStop();
  }
  
  public void   destroyApplet(int timeout)
  {
    av.onClose(timeout);
  }
  
  public int    getLoadingStatus()
  {
    return av.getLoadingStatus();
  }
   
  public void   setDocumentBase(URL docbase)
  {
      av.setDocumentBase(docbase);
  }

  public void   setWindow(Frame f)
  {
      av.setWindow(f);
  }

  public Applet getApplet()
  {
      return av.getApplet();
  }
};


