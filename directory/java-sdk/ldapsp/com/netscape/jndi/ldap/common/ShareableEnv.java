/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package com.netscape.jndi.ldap.common;

import java.util.*;

/**
 * ShareableEnv manages a set of environment properties. The class enables a memory
 * efficient sharing of the environment between multiple contexts, while preserving
 * the semantics that each context has its own environment. If the environment for a
 * context is changed, the change is not visible to other contexts. 
 *
 * The efficiency is achieved by implementing inheritance and override of environment
 * properties ("subclass-on-write"). A read-only table of properties
 * is shared among multiple contexts (<code>_sharedEnv</code>. If a context wants to
 * modified a shared property, it will create a separate table (<code>_privateEnv</code>)
 * to make the modifications. This table overrides the values in the shared table.
 * 
 * Note1: The class is not thread safe, it requires external synchronization
 * Note2: The class does not provide enumaration. Call getAllProperties() and then
 * use the standard Hashtable enumaration techniques.
 */
public class ShareableEnv implements Cloneable{

    /**
     * A special value that denotes a removed propety. Because shared property tables 
     * are read-only, a shared property is deleted in the curent context by assigning 
     * the REMOVED_PROPERTY value in the _modEnv table.
     */
    private static final Object REMOVED_PROPERTY = new Object();

    /**
     * A table of most recent environment modifications. These modifications are not
     * shared until the current context is cloned, which moves _privateEnv to
     * _sharedEnv
     */
    protected Hashtable m_privateEnv;
    
    /**
     * A set of environment propeties that have been changed in this Context and are
     * shared with one or more child contexts. Read-only access.
     */
    protected Vector m_sharedEnv; // A vector of Hashtables
     
    /**
     * Shared environment inherited from the parent context
     */
    protected ShareableEnv m_parentEnv;
    
    /**
     * Index into parent _sharedEnv list. Designates all environment chnages
     * in the parent context that are visible to this child context
     */
    protected int m_parentSharedEnvIdx = -1;

    /**
     * No-arg constructor is private
     */
    private ShareableEnv() {}
    
    /**
     * Constructor for non root Contexts
     *
     * @param parent A reference to the parent context environment
     * @param parentSharedEnvIdx index into parent's shared environemnt list
     */
    public ShareableEnv(ShareableEnv parent, int parentSharedEnvIdx) {
        m_parentEnv = parent;
        m_parentSharedEnvIdx = parentSharedEnvIdx;
    }
    
    /**
     * Constructor for the root context
     * 
     * @param initialEnv a hashtable with environemnt properties
     */
    public ShareableEnv(Hashtable initialEnv) {
        m_privateEnv = initialEnv;
    }

    /**
     * Set the property value.
     *
     * @return the previous value of the specified property,
     *         or <code>null</code> if it did not exist before.     
     * @param prop property name
     * @param val property value
     */
    public Object setProperty(String prop, Object val) {
        
        // Need the current value to be returned 
        Object oldVal = getProperty(prop);
        
        // Lazy create _privateEnv table
        if (m_privateEnv == null) {
            m_privateEnv = new Hashtable(5);
        }

        // Add/Modify property
        m_privateEnv.put(prop, val);
        
        return oldVal;
    }

    /**
     * Get the property value.
     *
     * @return the object associated with the property name
     *         or  <code>null</code> if property is not found
     * @param  prop property name
     */
    public Object getProperty(String prop) {
        
        // First check most recent modifications
        if (m_privateEnv != null) {
            Object val = m_privateEnv.get(prop);
            if (val != null) {
                return (val == REMOVED_PROPERTY) ? null : val;
            }
        }
        
        // Then try shared properties
        if (m_sharedEnv != null) {
            return getSharedProperty(m_sharedEnv.size()-1, prop);
        }
        else {
            return getSharedProperty(-1, prop);
        }
    }

    /**
     * Get the property value for the specified _sharedEnv index
     *
     * @return the object associated with the property name     
     * @param envIdx start index in the shared environment list
     * @param prop property name
     */
    private Object getSharedProperty(int envIdx, String prop) {
        
        // Check first the frozen updtates list
        if (envIdx >= 0) {
            for (int i= envIdx; i >= 0; i--) {
                Hashtable tab = (Hashtable) m_sharedEnv.elementAt(i);
                Object val = tab.get(prop);
                if (val != null) {
                    return (val == REMOVED_PROPERTY) ? null : val;
                }
            }
        }
        
        // Check the parent env context
        if (m_parentSharedEnvIdx >= 0) {
            return m_parentEnv.getSharedProperty(m_parentSharedEnvIdx, prop);
        }
        
        return null;
    }

    /**
     * Remove property
     *
     * @return the previous value of the specified property,
     *         or <code>null</code> if it did not exist before.     
     * @param prop property name
     */
    public Object removeProperty(String prop) {
        
        Object val = null;
        // Is this a shared property ?
        if (m_sharedEnv != null) {
            val = getSharedProperty(m_sharedEnv.size()-1, prop);
        }
        else {
            val = getSharedProperty(-1, prop);
        }
        
        if (val == null) { // Not a shared property, remove if physically
            if (m_privateEnv != null) {
                return m_privateEnv.remove(prop);
            }
        }
        else {  //A shared property, hide it
            setProperty(prop, REMOVED_PROPERTY);
        }
        return val;
    }    

    /**
     * Create a table of all properties. First read all properties from the parent env,
     * then merge the local updates. Notice that the data is processed in reverse order
     * than in the getProperty method.
     *
     * @return a hashtable containing all properties visible in this context     
     */
    public Hashtable getAllProperties() {
        Hashtable res = null;
        
        // First copy shared properties
        if (m_sharedEnv != null) {
            res = getAllSharedProperties(m_sharedEnv.size()-1);
        }
        else {
            res = getAllSharedProperties(-1);
        }

        if (res == null) {
            res = new Hashtable(51);
        }    
        
        // Then apply private env
        if (m_privateEnv != null) {
            Hashtable tab = m_privateEnv;
            for (Enumeration e = tab.keys(); e.hasMoreElements();) {
                Object key = e.nextElement();
                Object val = tab.get(key);
                if (val != REMOVED_PROPERTY) {
                    res.put(key, val);
                }
                else {
                    res.remove(key);
                }    
            }
        }    

        return res;
    }
    
    /**
     * Create a table of all shared properties for the given envIdx. First read all
     * properties from the parent env, then merge the local environment.
     *
     * @return a hashtable containing all properties visible for the shared environment index
     * @param envIdx start index in the shared environment list
     */
    private Hashtable getAllSharedProperties(int envIdx) {
        Hashtable res = null;
        
        // First copy parent env
        if (m_parentEnv != null) {
            res = m_parentEnv.getAllSharedProperties(m_parentSharedEnvIdx);
        }
        
        if (res == null) {
            res = new Hashtable(51);
        }    

        // Then copy _sharedEnv (older entries are processed before newer ones)
        if (envIdx >= 0) {
            for (int i= 0; i <= envIdx; i++) {
                Hashtable tab = (Hashtable) m_sharedEnv.elementAt(i);
                for (Enumeration e = tab.keys(); e.hasMoreElements();) {
                    Object key = e.nextElement();
                    Object val = tab.get(key);
                    if (val != REMOVED_PROPERTY) {
                        res.put(key, val);
                    }
                    else {
                        res.remove(key);
                    }    
                }
            }
        }    
        return res;
    }

    /**
     * Freeze all environment changes changes in the current context. The "Freeze" is
     * done by moving the _privateEnv table to _sharedEnv vector.
     */
    protected void freezeUpdates() {
        if (m_privateEnv != null) {
            // Lazy create _sharedEnv vector
            if (m_sharedEnv == null) {
                m_sharedEnv = new Vector();
            }    
            m_sharedEnv.addElement(m_privateEnv);
            m_privateEnv = null;
        }
    }
    
    /**
     * Clone ShareableEnv
     *
     * @return A "clone" of the current context environment
     */
    public Object clone() {
        
        // First freeze updates for this context
        freezeUpdates();
        
         // If the context has been modified, then it is the parent of the clone
        if (m_sharedEnv != null) {
            return new ShareableEnv(this, m_sharedEnv.size()-1);
        }
        
        // No changes has been done to the inherited parent context. Pass the parent
        // context to the clone
        else {
            return new ShareableEnv(m_parentEnv, m_parentSharedEnvIdx);
        }    
    }    

    /**
     * Return string representation of the object
     *
     * @return a string representation of the object
     */
    public String toString() {
        StringBuffer buf = new StringBuffer();
        buf.append("ShareableEnv private=");
        if (m_privateEnv != null) {
            buf.append("(");
            buf.append(m_privateEnv.size());
            buf.append(")");
        }
        buf.append(" shared=");
        if (m_sharedEnv != null) {
            for (int i=0; i < m_sharedEnv.size(); i++) {
                Hashtable tab = (Hashtable) m_sharedEnv.elementAt(i);
                buf.append("(");
                buf.append(tab.size());
                buf.append(")");
            }
        }    
        buf.append(" parentIdx=");
        buf.append(m_parentSharedEnvIdx);
                
        return buf.toString();
    }    

    /**
     * Test program 
     */
    public static void main(String[] args) {
    
        ShareableEnv c0 = new ShareableEnv(null, -1); // c=context 0=level 0=index
        System.err.println("c0.getProperty(p1)="    + c0.getProperty("p1"));
        System.err.println("c0.getAllProperties()=" + c0.getAllProperties());
        System.err.println("c0.setProperty(p1,vxxx)=" + c0.setProperty("p1", "vxxx"));
        System.err.println("c0.setProperty(p2,v2)=" + c0.setProperty("p2", "v2"));    
        System.err.println("c0.setProperty(p3,v3)=" + c0.setProperty("p3", "v3"));
        System.err.println("c0.setProperty(p1,v1)=" + c0.setProperty("p1", "v1"));        
        System.err.println("c0.getAllProperties()=" + c0.getAllProperties());
    
        System.err.println("---");
        ShareableEnv c01 = (ShareableEnv)c0.clone();
        System.err.println("c01.getProperty(p1)="     + c01.getProperty("p1"));
        System.err.println("c01.getAllProperties()="  + c01.getAllProperties());
        System.err.println("c01.setProperty(p1,v1a)=" + c01.setProperty("p1", "v1a"));
        System.err.println("c01.getProperty(p1)="     + c01.getProperty("p1"));
        System.err.println("c01.removeProperty(p2)="  + c01.removeProperty("p2"));
        System.err.println("c01.getProperty(p2)="     + c01.getProperty("p2"));    
        System.err.println("c01.setProperty(p11,v11a)=" + c01.setProperty("p11", "v11a"));
        System.err.println("c01.getAllProperties()="  + c01.getAllProperties());

        System.err.println("---");
        ShareableEnv c02 = (ShareableEnv)c0.clone();
        System.err.println("c02.getProperty(p1)="     + c02.getProperty("p1"));
        System.err.println("c02.getAllProperties()="  + c02.getAllProperties());

        System.err.println("---");
        ShareableEnv c011 = (ShareableEnv)c01.clone();
        System.err.println("c011.getProperty(p1)="     + c011.getProperty("p1"));
        System.err.println("c011.getAllProperties()="  + c011.getAllProperties());
        System.err.println("c011.setProperty(p1,v11b)=" + c011.setProperty("p1", "v11b"));
        System.err.println("c011.getProperty(p1)="     + c011.getProperty("p1"));
        System.err.println("c011.getAllProperties()="  + c011.getAllProperties());

        System.err.println("---");
        System.err.println("c01.getAllProperties()="  + c01.getAllProperties());
        System.err.println("c01.removeProperty(p11)="  + c01.removeProperty("p11"));
        System.err.println("c01.getAllProperties()="  + c01.getAllProperties());
        System.err.println("c011.getAllProperties()="  + c011.getAllProperties());

        System.err.println("---");
        ShareableEnv c012 = (ShareableEnv)c01.clone();
        System.err.println("c012.getAllProperties()="  + c012.getAllProperties());
        System.err.println("c012.getProperty(p1)="  + c012.getProperty("p1"));
        
        System.err.println("---");
        System.err.println("c0="+c0);
        System.err.println("c01="+c01);
        System.err.println("c02="+c02);
        System.err.println("c011="+c011);
        System.err.println("c012="+c012);        
        
    }
}    
    