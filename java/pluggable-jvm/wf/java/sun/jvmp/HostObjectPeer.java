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
 * $Id: HostObjectPeer.java,v 1.2 2001/07/12 19:57:53 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import sun.jvmp.security.*;
// peer event types, synchronize with nsIJavaHTMLObject.idl
public interface HostObjectPeer extends IDObject
{
    /**
     * Some peer types.  Just for reference.
     **/
    public static final int PT_UNKNOWN     = 0;
    public static final int PT_EMBED       = 1;
    public static final int PT_OBJECT      = 2;
    public static final int PT_APPLET      = 3;
    public static final int PT_PLUGLET     = 4;
 
    /**
     * Some peer events. Just for reference.
     **/
    public static final int PE_NOPE        = 0;
    public static final int PE_CREATE      = 1;
    public static final int PE_SETWINDOW   = 2;
    public static final int PE_DESTROY     = 3;
    public static final int PE_START       = 4;
    public static final int PE_STOP        = 5;
    public static final int PE_NEWPARAMS   = 6;
    public static final int PE_SETTYPE     = 7;
    public static final int PE_PEEREVENT   = 8;
    public static final int PE_GETPEER     = 9;
    
    /**
     * Java object events - usage is voluntary.
     **/
    public static final int JE_NOPE         = 0;
    public static final int JE_SHOWSTATUS   = 1;
    public static final int JE_SHOWDOCUMENT = 2;
    // generic event sent by applet
    public static final int JE_APPLETEVENT  = 3;
    // error requiring browser's attention happened in applet
    public static final int JE_APPLETERROR  = 4;
    // error requiring browser's attention happened in JVM
    public static final int JE_JVMERROR     = 5;
    
    /**
     * returns pointer to peer factory used to create this peer
     **/
    public HostObjectPeerFactory getFactory();
    /** 
     * Event handler - called when SendEvent/PostEvent methods called.
     * Calls comes directly from PluggableJVM main event loop - 
     * maybe it's better for each peer to have own message queue, 
     * but it will make performance overhead.
     **/   
    public int handleEvent(SecurityCaps caps, int eventID, long eventData);
    
    /**
     * This method is called directly from native thread - 
     * do anything you want.
     **/
    public int handleCall(SecurityCaps caps, int arg1, long arg2);

    /**
     * Method called when host calls JVMP_DestroyPeer.
     * Final clean up here. Return 0 if don't want to die, or caps
     * isn't good enough.
     */
    public int  destroy(SecurityCaps caps);
};











