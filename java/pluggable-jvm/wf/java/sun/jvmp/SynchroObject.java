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
 * $Id: SynchroObject.java,v 1.2 2001/07/12 19:57:54 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

public abstract class SynchroObject implements IDObject
{
    protected int m_id = 0;

    /* much like standard Java synchro API */
    public abstract void _lock();
    public abstract void _unlock();
    public abstract void _wait(int milli) throws InterruptedException;
    public abstract void _notify();
    public abstract void _notifyAll();
    public abstract void _destroy();
    
    public void setID(int id)
    {
	if (m_id != 0) return;
	m_id = id;
    }

    public int getID()
    {
	return m_id;
    }
}
