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

public class SchemaAttribute extends SchemaElement {

	LDAPAttributeSchema m_ldapAttribute;
	
	// Attribute IDs that are exposed through Netscape LdapJDK
	private static String[] m_allAttrIds = {NUMERICOID, NAME, DESC, SYNTAX, SINGLEVALUE };
	
	public SchemaAttribute(LDAPAttributeSchema ldapAttribute, SchemaManager schemaManager) {
	    super(schemaManager);
		m_ldapAttribute = ldapAttribute;
		m_path = ATTRDEF + "/" + m_ldapAttribute.getName();
	}

	public SchemaAttribute(Attributes attrs,  SchemaManager schemaManager) throws NamingException {
	    super(schemaManager);	    
		m_ldapAttribute = parseDefAttributes(attrs);
		m_path = ATTRDEF + "/" + m_ldapAttribute.getName();
	}	
	
	/**
	 * Parse Definition Attributes for a LDAP attribute
	 */
	static LDAPAttributeSchema parseDefAttributes(Attributes attrs) throws NamingException {		
		String name="", oid="", desc="";
		int syntax=-1;
		boolean singleValued = false;

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
			else if (attrName.equals(SYNTAX)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					String syntaxString = (String)valEnum.nextElement();
					syntax = syntaxStringToInt(syntaxString);
					break;
				}
			}
			else if (attrName.equals(DESC)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					desc = (String)valEnum.nextElement();
					break;
				}
			}			
			else if (attrName.equals(SINGLEVALUE)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					String flag = (String)valEnum.nextElement();
					if (flag.equals("true")) {
						singleValued = true;
					}
					else if (flag.equals("false")) {
						singleValued = false;
					}
					else {
						throw new InvalidAttributeValueException(attrName);
					}	
					break;
				}
			}			
			else if (attrName.equals(OBSOLETE) || attrName.equals(SUP) || attrName.equals(USAGE) ||
			         attrName.equals(EQUALITY) || attrName.equals(ORDERING) || attrName.equals(SUBSTRING) ||
			         attrName.equals(COLLECTIVE) || attrName.equals(NOUSERMOD)) {			         
				; //ignore
			}
			else { 
				throw new NamingException("Invalid schema attribute type for attribute definition "	+ attrName);
			}
		}	
			
		if (name.length() == 0 || oid.length() == 0 || syntax == -1) {			   	
		   	throw new NamingException("Incomplete schema attribute definition");
		}
			
		return new LDAPAttributeSchema(name, oid, desc, syntax, singleValued);
	}
	
		
	/**
	 * Exctract specified attributes from the ldapObjectClass
	 */
	Attributes extractAttributeIds(String[] attrIds) throws NamingException{
		Attributes attrs = new BasicAttributes();
		for (int i = 0; i < attrIds.length; i++) {
			if (attrIds[i].equals(NUMERICOID)) {
				attrs.put(new BasicAttribute(NUMERICOID, m_ldapAttribute.getOID()));
			}
			else if (attrIds[i].equals(NAME)) {
				attrs.put(new BasicAttribute(NAME, m_ldapAttribute.getName()));
			}
			else if (attrIds[i].equals(DESC)) {
				attrs.put(new BasicAttribute(DESC, m_ldapAttribute.getDescription()));
			}
			else if (attrIds[i].equals(SYNTAX)) {
				attrs.put(new BasicAttribute(SYNTAX, syntaxIntToString(m_ldapAttribute.getSyntax())));
			}
			else if (attrIds[i].equals(SINGLEVALUE)) {
				attrs.put(new BasicAttribute(SINGLEVALUE, 
				          (m_ldapAttribute.isSingleValued() ? "true" : "false")));
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
		LDAPAttributeSchema modLdapAttribute = parseDefAttributes(modAttrs);
		m_schemaMgr.modifyAttribute(m_ldapAttribute, modLdapAttribute);
		m_ldapAttribute = modLdapAttribute;
	}

	public void modifyAttributes(String name, ModificationItem[] mods) throws NamingException {
		if (name.length() != 0) { // no subcontexts
			throw new NameNotFoundException(name); // no such object
		}
		Attributes modAttrs = extractAttributeIds(m_allAttrIds);
		modifySchemaElementAttrs(modAttrs, mods);
		LDAPAttributeSchema modLdapAttribute = parseDefAttributes(modAttrs);
		m_schemaMgr.modifyAttribute(m_ldapAttribute, modLdapAttribute);
		m_ldapAttribute = modLdapAttribute;
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
