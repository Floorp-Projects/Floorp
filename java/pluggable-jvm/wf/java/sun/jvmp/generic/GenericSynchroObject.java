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
 * $Id: GenericSynchroObject.java,v 1.2 2001/07/12 19:57:56 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.generic;

import sun.jvmp.*;

public abstract class GenericSynchroObject extends SynchroObject 
{
    protected long    m_handle = 0;
    protected boolean m_state  = false;

    public void _lock()
    {
	int res;
	if (!m_state)
	    throw new IllegalMonitorStateException("Invalid object state");
	if ((res = doLock(m_handle)) != 0)
	    throw new IllegalMonitorStateException("Invalid native state: err="
						   +res);
    }

    public void _unlock()
    {
	int res;
	if (!m_state)
	    throw new IllegalMonitorStateException("Invalid object state");
	if ((res=doUnlock(m_handle)) != 0)
	    throw new IllegalMonitorStateException("Invalid native state: err="
						   +res);
    }

    public void _wait(int milli) throws InterruptedException 
    {
      int res;
      if (!m_state)
	throw new IllegalMonitorStateException("Invalid object state");
      if ((res = doWait(m_handle, milli)) < -1)
	throw new IllegalMonitorStateException("Invalid native state: err="
					       +res);
      if (res == -1) throw new InterruptedException("Native wait() interrupted");
    }

    public void _notify() 
    { 
      int res;
      if (!m_state)
	throw new IllegalMonitorStateException("Invalid object state");
      if ((res=doNotify(m_handle)) != 0)
	  throw new IllegalMonitorStateException("Invalid native state: err="
						 +res);
    }

    public void _notifyAll()
    {
      int res;
      if (!m_state)
	    throw new IllegalMonitorStateException("Invalid object state");
      if ((res=doNotifyAll(m_handle)) != 0)
	    throw new IllegalMonitorStateException("Invalid native state: err="
						   +res);
    }

    public void _destroy()
    {
	int res;
	if (!m_state)
	    throw new IllegalMonitorStateException("Invalid object state");
	if ((res=doDestroy(m_handle)) != 0)
	    throw new IllegalMonitorStateException("Invalid native state: err="
						   +res);
    }

    protected abstract int checkHandle(long handle);
    protected abstract int doLock(long handle);
    protected abstract int doUnlock(long handle);
    protected abstract int doWait(long handle, int milli);
    protected abstract int doNotify(long handle);
    protected abstract int doNotifyAll(long handle);
    protected abstract int doDestroy(long handle);    
};
