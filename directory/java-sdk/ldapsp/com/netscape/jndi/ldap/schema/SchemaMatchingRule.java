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
    private static String[] m_allAttrIds = {NUMERICOID, NAME, DESC, OBSOLETE, SYNTAX, APPLIES };
    
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
        String name=null, oid=null, desc=null, syntax=null;
        boolean obsolete = false;
        Vector applies = new Vector();

        for (Enumeration attrEnum = attrs.getAll(); attrEnum.hasMoreElements(); ) {
            Attribute attr = (Attribute) attrEnum.nextElement();
            String attrName = attr.getID();
            if (attrName.equals(NAME)) {
                name = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(NUMERICOID)) {
                oid = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(SYNTAX)) {
                syntax = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(DESC)) {
                desc = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(APPLIES)) {
                for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
                    applies.addElement((String)valEnum.nextElement());
                }
            }            
            else if (attrName.equals(OBSOLETE)) {                     
                obsolete = parseTrueFalseValue(attr);
            }
            else { 
                throw new NamingException("Invalid schema attribute type for matching rule definition "    + attrName);
            }
        }    
            
        LDAPMatchingRuleSchema mrule = new LDAPMatchingRuleSchema(name, oid, desc, vectorToStringAry(applies), syntax);
        
        if (obsolete) {
            mrule.setQualifier(OBSOLETE, "");
        }
        return mrule;
    }
    
        
    /**
     * Exctract specified attributes from the ldapMatchingRule
     */
    Attributes extractAttributeIds(String[] attrIds) throws NamingException{
        Attributes attrs = new BasicAttributes();
        String val = null;
        for (int i = 0; i < attrIds.length; i++) {
            if (attrIds[i].equals(NUMERICOID)) {
                val = m_ldapMatchingRule.getOID();
                if (val != null) {
                    attrs.put(new BasicAttribute(NUMERICOID, val));
                }                    
            }
            else if (attrIds[i].equals(NAME)) {
                val = m_ldapMatchingRule.getName();
                if (val != null) {
                    attrs.put(new BasicAttribute(NAME, val));
                }                    
            }
            else if (attrIds[i].equals(DESC)) {
                val = m_ldapMatchingRule.getDescription();
                if (val != null) {
                    attrs.put(new BasicAttribute(DESC, val));
                }                    
            }
            else if (attrIds[i].equals(SYNTAX)) {
                val = m_ldapMatchingRule.getSyntaxString();
                if (val != null) {
                    attrs.put(new BasicAttribute(SYNTAX, val));
                }                    
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
            else if (attrIds[i].equals(OBSOLETE)) {
                if (m_ldapMatchingRule.getQualifier(OBSOLETE)!= null) {
                    attrs.put(new BasicAttribute(OBSOLETE, "true"));
                }                    
            }

            else {
                throw new NamingException("Invalid schema attribute type for matching rule definition "    + attrIds[i]);
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
