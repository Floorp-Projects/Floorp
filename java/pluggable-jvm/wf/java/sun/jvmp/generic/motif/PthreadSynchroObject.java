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
 * $Id: PthreadSynchroObject.java,v 1.2 2001/07/12 19:57:58 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.generic.motif;

import sun.jvmp.generic.GenericSynchroObject;

public class PthreadSynchroObject extends GenericSynchroObject 
{
    public PthreadSynchroObject(long handle) throws IllegalArgumentException
	{
	    m_handle = handle;
	    if (checkHandle(handle) != 0) 
		throw new IllegalArgumentException("Invalid handle passed to constructor");
	    m_state = true;
	}
    protected native int checkHandle(long handle);
    protected native int doLock(long handle);
    protected native int doUnlock(long handle);
    protected native int doWait(long handle, int milli);
    protected native int doNotify(long handle);
    protected native int doNotifyAll(long handle);
    protected native int doDestroy(long handle);
};
