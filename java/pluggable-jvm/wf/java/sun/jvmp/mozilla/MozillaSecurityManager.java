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
 * $Id: MozillaSecurityManager.java,v 1.3 2001/07/12 20:26:18 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.mozilla;

import sun.jvmp.*;
import sun.jvmp.security.*;
import sun.jvmp.applet.*;
import java.security.*;

public class MozillaSecurityManager extends WFAppletSecurityManager
{
    java.net.SocketPermission resolvePerm = 
	new java.net.SocketPermission("*", "resolve");
    
    
    public void checkPackageAccess(final String p)
    {
	//System.err.println("checkPackageAccesss: "+p);
	// this check used to prevent infinite recursion when calling implies()
	// in JavaScriptProtectionDomain,, as it has to load some classes
        if (p.equals("sun.jvmp.mozilla"))
 	  {	    
  	      return;
  	  }
	super.checkPackageAccess(p);
    }
    
    

    public void checkPermission(Permission p)
    {
	// this kludge is workaround for some bug/feature in Sun's JDK 
	// 1.3.1 and 1.4 related to handling proxies and applets
	if (p != null && p instanceof java.net.SocketPermission)
	    {		
		String action = ((java.net.SocketPermission)p).getActions();
		if ("resolve".equals(action)) return;
		//AccessControlContext c = AccessController.getContext();
		//System.err.println("p="+p+" ctx="+c);		
	    }
	super.checkPermission(p);	
    }
}









