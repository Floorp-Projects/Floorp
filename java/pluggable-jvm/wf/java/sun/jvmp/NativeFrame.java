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
 * $Id: NativeFrame.java,v 1.2 2001/07/12 19:57:53 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import java.awt.*;

public class NativeFrame implements IDObject
{
    protected Frame m_f;
    protected int m_id = 0;

    public NativeFrame(Frame f)
    {
	m_f = f; 
    }

    public void setID(int id)
    {
	if (m_id !=0) return;
	m_id = id;
    }

    public int getID()
    {
	return m_id;
    }
        
    public Frame getFrame()
    {
	return m_f;
    }
    public boolean equals(Object obj)     
    {
	NativeFrame f;
	if (obj == null) return false;
	try {
	    f = (NativeFrame)obj;
	} catch (ClassCastException e) {
	    return false;
	}
        return (m_f == f.getFrame());
    }

    public String toString()
    {
	if (m_f != null) return m_f.toString();
	return "null NativeFrame object";
    }
}
