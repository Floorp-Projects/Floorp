/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *	Luke (lukemz@onemodel.org)
 */
package com.netscape.jndi.ldap;

import javax.naming.*;
import javax.naming.directory.*;

import netscape.ldap.*;

import java.util.*;

/**
 * Wrapper for LDAPAttributeSet which implements JNDI Attribute interface
 */
class AttributesImpl implements Attributes {

    // TODO Create JndiAttribute class so that getAttributeDefinition and
    // getAttributeSyntaxDefinition can be implemented
   
    LDAPAttributeSet m_attrSet;

    /**
     * A list of predefined binary attribute name
     */
    static String[] m_binaryAttrs = {
      "photo", "userpassword", "jpegphoto", "audio", "thumbnailphoto", "thumbnaillogo",
      "usercertificate",  "cacertificate", "certificaterevocationlist",
      "authorityrevocationlist", "crosscertificatepair", "personalsignature",
      "x500uniqueidentifier", "javaserializeddata"};
    
    /**
     * A list of user defined binary attributes specified with the environment
     * property java.naming.ldap.attributes.binary
     */
    static String[] m_userBinaryAttrs = null;
    
    public AttributesImpl(LDAPAttributeSet attrSet, String[] userBinaryAttrs) {
        m_attrSet = attrSet;
        m_userBinaryAttrs = userBinaryAttrs;
    }    
    
    public Object clone() {
        return new AttributesImpl((LDAPAttributeSet)m_attrSet.clone(), m_userBinaryAttrs);
    }    

    public Attribute get(String attrID) {
        LDAPAttribute attr = m_attrSet.getAttribute(attrID);
        return (attr == null) ? null : ldapAttrToJndiAttr(attr);
    }

    public NamingEnumeration getAll() {
        return new AttributeEnum(m_attrSet.getAttributes());
    }

    public NamingEnumeration getIDs() {
        return new AttributeIDEnum(m_attrSet.getAttributes());
    }

    public boolean isCaseIgnored() {
        return false;
    }

    public Attribute put(String attrID, Object val) {
        LDAPAttribute attr = m_attrSet.getAttribute(attrID);
        if (val == null) { // no Value
            m_attrSet.add(new LDAPAttribute(attrID));
        }    
        else if (val instanceof byte[]) {
            m_attrSet.add(new LDAPAttribute(attrID, (byte[])val));
        }
        else {
            m_attrSet.add(new LDAPAttribute(attrID, val.toString()));
        }
        return (attr == null) ? null : ldapAttrToJndiAttr(attr);
    }

    public Attribute put(Attribute jndiAttr) {
        try {
            LDAPAttribute oldAttr = m_attrSet.getAttribute(jndiAttr.getID());
            m_attrSet.add(jndiAttrToLdapAttr(jndiAttr));
            return (oldAttr == null) ? null : ldapAttrToJndiAttr(oldAttr);
        }
        catch (NamingException e) {
            System.err.println( "Error in AttributesImpl.put(): " + e.toString() );
            e.printStackTrace(System.err);
        }
        return null;
    }

    public Attribute remove(String attrID) {
        Attribute attr = get(attrID);
        m_attrSet.remove(attrID);
        return attr;
    }    
    
    public int size() {
        return m_attrSet.size();
    }

    
    /**
     * Check if an attribute is a binary one
     */
    static boolean isBinaryAttribute(String attrID) {
        
         // attr name contains ";binary"
        if (attrID.indexOf(";binary") >=0) {
            return true;
        }
        
        attrID = attrID.toLowerCase();
                
        // check the predefined binary attr table
        for (int i=0; i < m_binaryAttrs.length; i++) {
            if (m_binaryAttrs[i].equals(attrID)) {
                return true;
            }
        }
        
        // check user specified binary attrs with
        for (int i=0; m_userBinaryAttrs != null && i < m_userBinaryAttrs.length; i++) {
            if (m_userBinaryAttrs[i].equals(attrID)) {
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Convert a JNDI Attributes object into a LDAPAttributeSet
     */
    static LDAPAttributeSet jndiAttrsToLdapAttrSet(Attributes jndiAttrs) throws NamingException{
        LDAPAttributeSet attrs = new LDAPAttributeSet();
        for (Enumeration enum = jndiAttrs.getAll(); enum.hasMoreElements();) {
            attrs.add(jndiAttrToLdapAttr((Attribute) enum.nextElement()));
        }
        return attrs;
    }    
        
    /**
     * Convert a JNDI Attribute to a LDAPAttribute
     */
    static LDAPAttribute jndiAttrToLdapAttr(Attribute jndiAttr) throws NamingException{
        LDAPAttribute attr = new LDAPAttribute(jndiAttr.getID());
        
        for (NamingEnumeration vals = jndiAttr.getAll(); vals.hasMoreElements();) {
            Object val = vals.nextElement();
            if (val == null) {
                ;  // no value
            }    
            else if (val instanceof byte[]) {
                attr.addValue((byte[])val);
            }
            else {
                attr.addValue(val.toString());
            }    
        }
        return attr;
    }
    
    /**
     * Convert a LDAPAttribute to a JNDI Attribute
     */
    static Attribute ldapAttrToJndiAttr(LDAPAttribute attr) {
        BasicAttribute jndiAttr = new BasicAttribute(attr.getName());
        Enumeration enumVals = null;
        if (isBinaryAttribute(attr.getName())) {
            enumVals = attr.getByteValues();
        }
        else {
            enumVals = attr.getStringValues();
        }
	/* Performance enhancement for an attribute with many values.
	 * If number of value over threshold, use TreeSet to quickly
	 * eliminate value duplication. Locally extends JNDI attribute
	 * to pass TreeSet directly to Vector of JNDI attribute.
	 */    
	if (attr.size() < 50 ) {
          if (enumVals != null) {
              while ( enumVals.hasMoreElements() ) {
                  jndiAttr.add(enumVals.nextElement());
              }
          }    
	} 
	else {
	  /* A local class to allow constructing a JNDI attribute
	   * from a TreeSet.
	   */
	  class BigAttribute extends BasicAttribute {
		public BigAttribute (String id, TreeSet val) {
			super(id);
			values = new Vector (val);
		}
	  }
	  TreeSet valSet = new TreeSet();
          if (enumVals != null) {
              while ( enumVals.hasMoreElements() ) {
                  valSet.add(enumVals.nextElement());
              }
          }    
          jndiAttr = new BigAttribute(attr.getName(), valSet);
	}
        return jndiAttr;
    }

    /**
     * Convert and array of JNDI ModificationItem to a LDAPModificationSet
     */
    static LDAPModificationSet jndiModsToLdapModSet(ModificationItem[] jndiMods) throws NamingException{
        LDAPModificationSet mods = new LDAPModificationSet();
        for (int i=0; i < jndiMods.length; i++) {
            int modop = jndiMods[i].getModificationOp();
            LDAPAttribute attr = jndiAttrToLdapAttr(jndiMods[i].getAttribute());
            if (modop == DirContext.ADD_ATTRIBUTE) {
                mods.add(LDAPModification.ADD, attr);
            }
            else if (modop == DirContext.REPLACE_ATTRIBUTE) {
                mods.add(LDAPModification.REPLACE, attr);
            }    
            else if (modop == DirContext.REMOVE_ATTRIBUTE) {
                mods.add(LDAPModification.DELETE, attr);
            }
            else {
                // Should never come here. ModificationItem can not
                // be constructed with a wrong value
            }
        }
        return mods;
    }    

    /**
     * Create a LDAPModificationSet from a JNDI mod operation and JNDI Attributes
     */
    static LDAPModificationSet jndiAttrsToLdapModSet(int modop, Attributes jndiAttrs) throws NamingException{
        LDAPModificationSet mods = new LDAPModificationSet();
        for (NamingEnumeration attrEnum = jndiAttrs.getAll(); attrEnum.hasMore();) {
            LDAPAttribute attr = jndiAttrToLdapAttr((Attribute)attrEnum.next());
            if (modop == DirContext.ADD_ATTRIBUTE) {
                mods.add(LDAPModification.ADD, attr);
            }
            else if (modop == DirContext.REPLACE_ATTRIBUTE) {
                mods.add(LDAPModification.REPLACE, attr);
            }    
            else if (modop == DirContext.REMOVE_ATTRIBUTE) {
                mods.add(LDAPModification.DELETE, attr);
            }
            else {
                throw new IllegalArgumentException("Illegal Attribute Modification Operation");
            }
        }
        return mods;
    }
}

/**
 * Wrapper for enumeration of LDAPAttrubute-s. Convert each LDAPAttribute
 * into a JNDI Attribute accessed through the NamingEnumeration
 */
class AttributeEnum implements NamingEnumeration {
    
    Enumeration _attrEnum;
    
    public AttributeEnum(Enumeration attrEnum) {
        _attrEnum = attrEnum;
    }    

    public Object next() throws NamingException{
        LDAPAttribute attr = (LDAPAttribute) _attrEnum.nextElement();
        return AttributesImpl.ldapAttrToJndiAttr(attr);
    }

    public Object nextElement() {
        LDAPAttribute attr = (LDAPAttribute) _attrEnum.nextElement();
        return AttributesImpl.ldapAttrToJndiAttr(attr);
    }

    public boolean hasMore() throws NamingException{
        return _attrEnum.hasMoreElements();
    }

    public boolean hasMoreElements() {
        return _attrEnum.hasMoreElements();
    }

    public void close() {
    }
}

/**
 * Wrapper for enumeration of LDAPAttrubute-s. Return the name for each
 * LDAPAttribute accessed through the NamingEnumeration
 */
class AttributeIDEnum implements NamingEnumeration {

    Enumeration _attrEnum;
    
    public AttributeIDEnum(Enumeration attrEnum) {
        _attrEnum = attrEnum;
    }    

    public Object next() throws NamingException{
        LDAPAttribute attr = (LDAPAttribute) _attrEnum.nextElement();
        return attr.getName();
    }

    public Object nextElement() {
        LDAPAttribute attr = (LDAPAttribute) _attrEnum.nextElement();
        return attr.getName();
    }

    public boolean hasMore() throws NamingException{
        return _attrEnum.hasMoreElements();
    }

    public boolean hasMoreElements() {
        return _attrEnum.hasMoreElements();
    }

    public void close() {
    }
}    
