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
 * $Id: WFAppletSecurityManager.java,v 1.1 2001/07/12 20:26:00 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

/**
 * this class created for 3 reasons:
 *  - to hide from extension developers sun.applet.* classes
 *  - to give extension developers basic point for security decisions, like
 *    security code can do: 
 *    sm = System.getSecurityManager();
 *    if (sm instanceof sun.jvmp.applet.WFAppletSecurityManager)
 *     {
 *        Class[] clazz = mgr.getExecutionStackContext();
 *        and return codesource properly
 *     }
 *  - to allow fix bugs in sun.applet.* security manager
 **/
package sun.jvmp.applet;

import java.security.*;

public class WFAppletSecurityManager extends sun.applet.AppletSecurity {

    public WFAppletSecurityManager() { 
    }

    public Class[] getExecutionStackContext()
    {
	return super.getClassContext();
    }
}









