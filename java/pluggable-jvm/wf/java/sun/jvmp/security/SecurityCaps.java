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
 * $Id: SecurityCaps.java,v 1.2 2001/07/12 19:58:03 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.security;

import sun.jvmp.PluggableJVM;

public class SecurityCaps
{
    // keep in sync with jvmp_caps.h
    final int JVMP_MAX_CAPS_BYTES = 128;
    final int JVMP_MAX_CAPS = JVMP_MAX_CAPS_BYTES * 8;
    final int JVMP_MAX_SYS_CAP_BYTES = 2;  
    final int JVMP_MAX_SYS_CAP = JVMP_MAX_SYS_CAP_BYTES * 8;  

    byte bits[];

    public SecurityCaps()
    {
	bits = new byte[JVMP_MAX_CAPS_BYTES];
	for (int i=0; i<JVMP_MAX_CAPS_BYTES; i++)
	    bits[i] = 0;
    }
    
    public SecurityCaps(byte raw[])
    {
	bits = new byte[JVMP_MAX_CAPS_BYTES];
	System.arraycopy(raw, 0, bits, 0, JVMP_MAX_CAPS_BYTES);	
    }
    
    public boolean isPureSystem()
    {
	boolean res = true;
	for (int i=JVMP_MAX_SYS_CAP_BYTES; i<JVMP_MAX_CAPS_BYTES; i++)
	    {
		res &= (bits[i] == 0);
		if (!res) return false;
	    }
	return true;
    }
    
    public boolean isPureUser()
    {
	boolean res = true;
	for (int i=0; i<JVMP_MAX_SYS_CAP_BYTES; i++)
	    {
		res &= (bits[i] == 0);
		if (!res) return false;
	    }
	return true;
    }
    
    
    
    public boolean isAllowed(int action_no)
    {
	if (action_no < 0 || action_no >= JVMP_MAX_CAPS) return false;
	return 
	    (((bits[action_no >> 3]) & (1 << (action_no & 0x7))) != 0) ? true : false;	
    }
    
    public boolean isForbidden(int action_no)
    {
	return !isAllowed(action_no);
    }
    
    public boolean permits(SecurityCaps act)
    {
	//  boolean res = true;
//  	for (int i=0; i<JVMP_MAX_CAPS_BYTES; i++)
//  	    res &= 
	return true;
    }
    
};





