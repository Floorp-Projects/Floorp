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
 * $Id: AVClassLoader.java,v 1.1 2001/07/12 20:26:04 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.av;
 
import java.net.URL;
import sun.applet.*;
import java.net.*;
import java.security.*;
import java.io.*;
import java.util.Enumeration;

public class AVClassLoader extends sun.jvmp.applet.WFAppletClassLoader
{
    private URL base;
    public AVClassLoader(URL base) {
        super(base);
        this.base = base;	
    }

    /* this code is workaround for subtle bug/feature in JDK1.3.1 and 1.4,
       related to loading applets behind proxy */       
    protected PermissionCollection getPermissions(CodeSource codesource)
    {		
        PermissionCollection sysPerms = null;
	Policy policy = (Policy)
	    AccessController.doPrivileged(
		     new PrivilegedAction() {
			     public Object run() 
			     {
				 return Policy.getPolicy();
			     }});
	if (policy != null) 
	    sysPerms = policy.getPermissions(new CodeSource(null, null));
	else
	    sysPerms = new Permissions();
	
	final PermissionCollection perms = sysPerms;	
	if (base != null && base.getHost() != null)
	    perms.add(new SocketPermission(base.getHost()+":1-", 
					   "accept,connect,resolve"));
        URL url = codesource.getLocation();
	
        if (url.getProtocol().equals("file")) {
 
            String path = url.getFile().replace('/', File.separatorChar);
	    
            if (!path.endsWith(File.separator)) {
                int endIndex = path.lastIndexOf(File.separatorChar);
                if (endIndex != -1) {
		    path = path.substring(0, endIndex+1) + "-";
		    perms.add(new FilePermission(path, "read"));
                }
            }
            perms.add(new SocketPermission("localhost", "connect,accept"));
            AccessController.doPrivileged(new PrivilegedAction() {
		    public Object run() {
			try {
			    String host = InetAddress.getLocalHost().getHostName();
			    perms.add(new SocketPermission(host,"connect,accept"));
			} catch (UnknownHostException uhe) {
			}
			return null;
		    }
            });
	    
            if (base.getProtocol().equals("file")) {
                String bpath = base.getFile().replace('/', File.separatorChar);
                if (bpath.endsWith(File.separator)) {
                    bpath += "-";
                }
                perms.add(new FilePermission(bpath, "read"));
            }
	}
	//for (Enumeration e=perms.elements();e.hasMoreElements();)
	// System.err.println("p="+e.nextElement());
        return perms;
    }
};
