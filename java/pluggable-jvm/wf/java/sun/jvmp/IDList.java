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
 * $Id: IDList.java,v 1.2 2001/07/12 19:57:53 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import java.util.Vector;
import java.util.Enumeration;
import java.util.Hashtable;

public class IDList {
    protected Vector    m_items;
    protected Hashtable m_ids;
    // this is common for all instances
    static    int       m_nextID = 1;
    public IDList() 
    {
	m_items = new Vector(0, 5);
	m_ids = new Hashtable();
    }

    // this is way to make all IDList's error-prone, as 
    // ID will be truly unique for all instances
    static synchronized int getNextID()
    {
	if (m_nextID == Integer.MAX_VALUE) m_nextID = 1;
	return m_nextID++;
    }
    
    // XXX: reimplement it with smth more efficient
    int getStorage(int id)
    {
	Integer storage = (Integer)m_ids.get(new Integer(id));
	if (storage == null) return -1;
	return storage.intValue();
    }

    int getId(int storage)
    {
	int id = getNextID();
	m_ids.put(new Integer(id), new Integer(storage));
	return id;
    }

    public synchronized int add(IDObject o)
    {
	int id = 0, free_slot = -1;	
	if (o == null) return 0; // null objects cannot be saved
	// first - find out if this object already added
	// reverse order - to give smallest IDs first.
	for (int i = m_items.size()-1; i >= 0; i--)
	    {
		IDObject o1 = (IDObject)m_items.elementAt(i);
		if (o1 == null)
		    { 
			free_slot = i;
		    }
		else 
		    {
			if (o1.equals(o)) 
			    {				
				id = o1.getID();
				o.setID(id);
				return id;
			    }
		    }
	    }
	if (free_slot != -1) 
	    {
		id = getId(free_slot);
		m_items.setElementAt(o, free_slot);
	    }
	else
	    {
		m_items.addElement(o);
		id = getId(m_items.indexOf(o));
	    }
	o.setID(id);
	return id;
    }
    // XXX: should I shrink underlying Vector?
    public synchronized IDObject remove(int id)
    {  
	IDObject o = get(id);
	m_items.setElementAt(null, getStorage(id));
	return o;
    }
    
    public synchronized IDObject remove(IDObject o)
    {
	if (o == null) return null;
	for (int i = 0; i < m_items.size(); i++)
	    {
	      IDObject o1 = (IDObject)m_items.elementAt(i); 
	      if (o.equals(o1))
		{
		  m_items.setElementAt(null, i);
		  return o1;
		}
	    }
	return null;
    }

    public synchronized IDObject get(int id)
    {
	int storage = getStorage(id);
	if (storage == -1) return null;
	return (IDObject)m_items.elementAt(storage);
    }

    public synchronized int length()
    {
	int count = 0;
	for (int i = 0; i < m_items.size(); i++)
	    if (m_items.elementAt(i) != null) count++;	
	return count;
    }

    public Enumeration elements()
    {
	return m_items.elements();
    }
}


