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
 * $Id: WFPolicy.java,v 1.2 2001/07/12 19:58:03 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.security;

import sun.jvmp.*;
import java.security.*;
import java.net.*;
import java.util.*;

public class WFPolicy extends Policy
{
    Policy           sysPolicy;
    URL              wfcodebase;
    PluggableJVM     jvm;
    String           wfextpath;
    public WFPolicy(PluggableJVM jvm, URL wfcodebase)
    {
	this.wfcodebase = wfcodebase;
	wfextpath = wfcodebase.getPath()+"ext";
	this.jvm = jvm;
	this.sysPolicy = Policy.getPolicy();
    }

    protected boolean isAllPermissionGranted(CodeSource codesource)
    {
	URL codebase = codesource.getLocation();
	if (codebase == null) return false;
	if (wfcodebase.equals(codebase)) return true;
	//System.out.println("path="+codebase.getPath()+" extpath="+wfextpath+" proto="+codebase.getProtocol());
	String path = codebase.getPath();
	if ("file".equals(codebase.getProtocol()) && path != null
	    && path.startsWith(wfextpath)) return true;
	return false;
    }
    
    public PermissionCollection getPermissions(CodeSource codesource)
    {
	jvm.trace("asked perms for: "+codesource, PluggableJVM.LOG_DEBUG);
        Permissions perms = new Permissions();
	if (isAllPermissionGranted(codesource))
	    {
		perms.add(new java.security.AllPermission());
		jvm.trace("all perms granted to "+codesource,
			  PluggableJVM.LOG_DEBUG);
		return perms;
	    }
	PermissionCollection p = null;
	if (sysPolicy != null) p = sysPolicy.getPermissions(codesource);
	if (p != null)
	    {
		for (Enumeration e=p.elements(); e.hasMoreElements(); )
		    perms.add((Permission)e.nextElement());
	    }
	p = jvm.getExtensionsPermissions(codesource);
	if (p != null)
 {
		for (Enumeration e=p.elements(); e.hasMoreElements(); )
		    perms.add((Permission)e.nextElement());
	    }
	return perms;
    }

    public void refresh()
    {
	if (sysPolicy != null) sysPolicy.refresh();
    }
}








