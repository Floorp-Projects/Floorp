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
 * $Id: ExtSecurityManager.java,v 1.2 2001/07/12 19:58:02 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.security;
import java.util.*;
import sun.jvmp.*;

public class ExtSecurityManager
{
    protected Vector checkers;
    PluggableJVM jvm;

    public ExtSecurityManager(PluggableJVM owner)
    {
	checkers = new Vector();
	jvm = owner;
    }

    public void registerDecider(AccessControlDecider decider,
				SecurityCaps caps,
				SecurityCaps mask)
    {
	
    }
    
    public void unregisterCapsChecker(AccessControlDecider decider)
    {	
    }
}
