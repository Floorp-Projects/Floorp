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
 * $Id: AccessControlDecider.java,v 1.2 2001/07/12 19:58:02 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.security;

public interface AccessControlDecider
{
    final int NA = 0; /* N/A */
    final int AC = 1; /* allow chained */
    final int AF = 2; /* allow final */
    final int FC = 3; /* forbid chained */
    final int FF = 4; /* forbid final */
    
    /**
     * check this decider decision on this capability, using this 
     * calling context and principal
     */
    int       decide(CallingContext ctx, String principal, 
		     int cap_no);
    
    /**
     * check if this capablity belongs to the area of decision of 
     * this checker 
     */
    boolean   belongs(int cap_no);
}
