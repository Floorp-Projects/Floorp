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

public class SchemaMatchingRule extends SchemaElement {

    
    private static final String APPLIES = "APPLIES";    
    	
	LDAPMatchingRuleSchema m_ldapMatchingRule;
	
	// Attribute IDs that are exposed through Netscape LdapJDK
	private static String[] m_allAttrIds = {NUMERICOID, NAME, DESC, SYNTAX, APPLIES };
	
	LDAPConnection m_ld;
	
	public SchemaMatchingRule(LDAPMatchingRuleSchema ldapMatchingRule,  SchemaManager schemaManager) {
	    super(schemaManager);
		m_ldapMatchingRule = ldapMatchingRule;
		m_path = ATTRDEF + "/" + m_ldapMatchingRule.getName();
	}

	public SchemaMatchingRule(Attributes attrs,  SchemaManager schemaManager) throws NamingException {
	    super(schemaManager);
		m_ldapMatchingRule = parseDefAttributes(attrs);
		m_path = ATTRDEF + "/" + m_ldapMatchingRule.getName();
	}	
	
	/**
	 * Parse Definition Attributes for a LDAP matching rule
	 */
	static LDAPMatchingRuleSchema parseDefAttributes(Attributes attrs) throws NamingException {		
		String name="", oid="", desc="";
		int syntax=-1;
		Vector applies = new Vector();

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
			else if (attrName.equals(APPLIES)) {
				for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
					applies.addElement((String)valEnum.nextElement());
				}
			}			
			else if (attrName.equals(OBSOLETE)) {			         
				; //ignore
			}
			else { 
				throw new NamingException("Invalid schema attribute type for attribute definition "	+ attrName);
			}
		}	
			
		if (name.length() == 0 || oid.length() == 0 || syntax == -1) {			   	
		   	throw new NamingException("Incomplete schema attribute definition");
		}
			
		return new LDAPMatchingRuleSchema(name, oid, desc, vectorToStringAry(applies), syntax);
	}
	
		
	/**
	 * Exctract specified attributes from the ldapMatchingRule
	 */
	Attributes extractAttributeIds(String[] attrIds) throws NamingException{
		Attributes attrs = new BasicAttributes();
		for (int i = 0; i < attrIds.length; i++) {
			if (attrIds[i].equals(NUMERICOID)) {
				attrs.put(new BasicAttribute(NUMERICOID, m_ldapMatchingRule.getOID()));
			}
			else if (attrIds[i].equals(NAME)) {
				attrs.put(new BasicAttribute(NAME, m_ldapMatchingRule.getName()));
			}
			else if (attrIds[i].equals(DESC)) {
				attrs.put(new BasicAttribute(DESC, m_ldapMatchingRule.getDescription()));
			}
			else if (attrIds[i].equals(SYNTAX)) {
				attrs.put(new BasicAttribute(SYNTAX, syntaxIntToString(m_ldapMatchingRule.getSyntax())));
			}
			else if (attrIds[i].equals(APPLIES)) {
				String[] appliesToAttrs = m_ldapMatchingRule.getAttributes();
				if (appliesToAttrs != null && appliesToAttrs.length > 0) {
					BasicAttribute applies = new BasicAttribute(APPLIES);
					for (int a=0; a < appliesToAttrs.length; a++) {
						applies.add(appliesToAttrs[a]);
					}
					attrs.put(applies);
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
		LDAPMatchingRuleSchema modLdapMatchingRule = parseDefAttributes(modAttrs);
		m_schemaMgr.modifyMatchingRule(m_ldapMatchingRule, modLdapMatchingRule);
		m_ldapMatchingRule = modLdapMatchingRule;
	}

	public void modifyAttributes(String name, ModificationItem[] mods) throws NamingException {
		if (name.length() != 0) { // no subcontexts
			throw new NameNotFoundException(name); // no such object
		}
		Attributes modAttrs = extractAttributeIds(m_allAttrIds);
		modifySchemaElementAttrs(modAttrs, mods);
		LDAPMatchingRuleSchema modLdapMatchingRule = parseDefAttributes(modAttrs);
		m_schemaMgr.modifyMatchingRule(m_ldapMatchingRule, modLdapMatchingRule);
		m_ldapMatchingRule = modLdapMatchingRule;
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
