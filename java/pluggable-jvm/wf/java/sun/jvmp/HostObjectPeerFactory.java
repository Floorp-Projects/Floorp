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
 * $Id: HostObjectPeerFactory.java,v 1.2 2001/07/12 19:57:53 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import sun.jvmp.security.*;
import java.security.*;

// interface that should be implemented by all Waterfall extensions
// Java classes.
// creates Java peer of given version.
// vendor ID is already fixed, and should be known by this factory
public interface HostObjectPeerFactory extends IDObject
{
    public String            getCID();            
    public HostObjectPeer    create(int version);
    /** 
     * Event handler - called when SendExtEvent/PostExtEvent methods called.
     * Calls comes directly from PluggableJVM main event loop - 
     * maybe it's better for each extension to have own message queue, 
     * but it will make performance overhead.
     **/   
    public int handleEvent(SecurityCaps caps, 
			   int eventID,
			   long eventData);
     /**
     * This method is called directly from native thread - 
     * do anything you want.
     **/
    public int handleCall(SecurityCaps caps, int arg1, long arg2);

    /**
     * Method called when host calls JVMP_UnregisterExtension.
     * Final clean up here. Return 0 if don't want to die, or caps
     * isn't good enough.
     */
    public int  destroy(SecurityCaps caps);
    /**
     * Returns permission collection required for this extension to work
     * properly, null if nothing
     */
    public PermissionCollection getPermissions(CodeSource codesource);
}


