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
package com.netscape.jndi.ldap.schema;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;

import netscape.ldap.*;
import netscape.ldap.controls.*;

import java.util.*;

public class SchemaObjectClass extends SchemaElement {

	 LDAPObjectClassSchema m_ldapObjectClass;
	 
	// Attribute IDs that are exposed through Netscape LdapJDK
	private static String[] m_allAttrIds = {NUMERICOID, NAME, DESC, SUP, MUST, MAY };
	
	public SchemaObjectClass(LDAPObjectClassSchema ldapObjectClass, SchemaManager schemaManager) {
	    super(schemaManager);		
		m_ldapObjectClass = ldapObjectClass;
		m_path = CLASSDEF + "/" + m_ldapObjectClass.getName();
	}

	public SchemaObjectClass(Attributes attrs, SchemaManager schemaManager) throws NamingException {
	    super(schemaManager);
		m_ldapObjectClass = parseDefAttributes(attrs);
		m_path = CLASSDEF + "/" + m_ldapObjectClass.getName();
	}	
	
	/**
	 * Parse Definition Attributes for a LDAP objectcalss
	 */
	static LDAPObjectClassSchema parseDefAttributes(Attributes attrs) throws NamingException {		
		String name="", oid="", desc="", sup="";
		Vector must=new Vector(), may=new Vector();

		for (Enumeration attrEnum = attrs.getAll(); attrEnum.hasMoreElements(); ) {
			Attribute attr = (Attribute) attrEnum.nextElement();
			String attrName = attr.getID();
			if (attrName.equals(NAME)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					name = (String)valEnum.nextElement();
					break;
				}
			}
			else if (attrName.equals(NUMERICOID)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					oid = (String)valEnum.nextElement();
					break;
				}
			}
			else if (attrName.equals(SUP)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					sup = (String)valEnum.nextElement();
					break;
				}
			}
			else if (attrName.equals(DESC)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					desc = (String)valEnum.nextElement();
					break;
				}
			}			
			else if (attrName.equals(MAY)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					may.addElement((String)valEnum.nextElement());
				}
			}
			else if (attrName.equals(MUST)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					must.addElement((String)valEnum.nextElement());
				}
			}
			else if (attrName.equals(OBSOLETE) || attrName.equals(ABSTRACT) ||
			         attrName.equals(STRUCTURAL) || attrName.equals(AUXILIARY)) {
				; //ignore
			}
			else { 
				throw new NamingException("Invalid schema attribute type for object class definition "	+ attrName);
			}
		}	
			
		if (name.length() == 0 || oid.length() == 0 || sup.length() == 0 ||
		   (must.size() == 0 && may.size() == 0)) {
			   	
		   	throw new NamingException("Incomplete schema object class definition");
		}
			
		return  new LDAPObjectClassSchema(name, oid, sup, desc,
			                              vectorToStringAry(must),
			                              vectorToStringAry(may));
	}
		
	/**
	 * Exctract specified attributes from the ldapObjectClass
	 */
	Attributes extractAttributeIds(String[] attrIds) {
		Attributes attrs = new BasicAttributes();
		for (int i = 0; i < attrIds.length; i++) {
			if (attrIds[i].equals(NUMERICOID)) {
				attrs.put(new BasicAttribute(NUMERICOID, m_ldapObjectClass.getOID()));
			}
			else if (attrIds[i].equals(NAME)) {
				attrs.put(new BasicAttribute(NAME, m_ldapObjectClass.getName()));
			}
			else if (attrIds[i].equals(DESC)) {
				attrs.put(new BasicAttribute(DESC, m_ldapObjectClass.getDescription()));
			}
			else if (attrIds[i].equals(SUP)) {
				attrs.put(new BasicAttribute(SUP, m_ldapObjectClass.getSuperior()));
			}
			else if (attrIds[i].equals(MAY)) {
				BasicAttribute optional = new BasicAttribute(MAY);
				for (Enumeration e = m_ldapObjectClass.getOptionalAttributes(); e.hasMoreElements();) {
					optional.add(e.nextElement());
				}
				if (optional.size() != 0) {
					attrs.put(optional);
				}
			}
			else if (attrIds[i].equals(MUST)) {
				BasicAttribute required = new BasicAttribute(MUST);
				for (Enumeration e = m_ldapObjectClass.getRequiredAttributes(); e.hasMoreElements();) {
					required.add(e.nextElement());
				}
				if (required.size() != 0) {
					attrs.put(required);
				}
			}
			else {
				// ignore other attrIds as not supported by Netscape LdapJDK APIs
			}
		}

		return attrs;
	}	
		
		
	/**
	 * DirContext Attribute Operations
	 */

	public Attributes getAttributes(String name) throws NamingException {
		if (name.length() != 0) { // no subcontexts
			throw new NameNotFoundException(name); // no such object
		}
		return extractAttributeIds(m_allAttrIds);
	}

	public Attributes getAttributes(String name, String[] attrIds) throws NamingException {
		if (name.length() != 0) { // no subcontexts
			throw new NameNotFoundException(name); // no such object
		}
		return extractAttributeIds(attrIds);
	}

	public Attributes getAttributes(Name name) throws NamingException {
		return getAttributes(name.toString());
	}

	public Attributes getAttributes(Name name, String[] attrIds) throws NamingException {
		return getAttributes(name.toString(), attrIds);
	}

	public void modifyAttributes(String name, int mod_op, Attributes attrs) throws NamingException {
		if (name.length() != 0) { // no subcontexts
			throw new NameNotFoundException(name); // no such object
		}
		Attributes modAttrs = extractAttributeIds(m_allAttrIds);
		modifySchemaElementAttrs(modAttrs, mod_op, attrs);
		LDAPObjectClassSchema modLdapObjectClass = parseDefAttributes(modAttrs);
		m_schemaMgr.modifyObjectClass(m_ldapObjectClass, modLdapObjectClass);
		m_ldapObjectClass = modLdapObjectClass;
	}

	public void modifyAttributes(String name, ModificationItem[] mods) throws NamingException {
		if (name.length() != 0) { // no subcontexts
			throw new NameNotFoundException(name); // no such object
		}
		Attributes modAttrs = extractAttributeIds(m_allAttrIds);
		modifySchemaElementAttrs(modAttrs, mods);
		LDAPObjectClassSchema modLdapObjectClass = parseDefAttributes(modAttrs);
		m_schemaMgr.modifyObjectClass(m_ldapObjectClass, modLdapObjectClass);
		m_ldapObjectClass = modLdapObjectClass;
	}

	public void modifyAttributes(Name name, int mod_op, Attributes attrs) throws NamingException {
		modifyAttributes(name.toString(), mod_op, attrs);
	}

	public void modifyAttributes(Name name, ModificationItem[] mods) throws NamingException {
		modifyAttributes(name.toString(), mods);
	}
	
	/**
	 * Search operations are not implemented because of complexity. Ir will require
	 * to implement the full LDAP search filter sematics in the client. (The search 
	 * filter sematics is implemented by the Directory server).
	 */

}
