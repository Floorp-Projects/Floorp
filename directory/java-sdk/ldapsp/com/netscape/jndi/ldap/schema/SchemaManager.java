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
import java.util.*;
import com.netscape.jndi.ldap.common.*;

/**
 * A wrapper calss for LDAPSchema. It main purpose is to manage loading of schema
 * on demand. The schema is loaded when accessed for the first time, or after changes
 * to the schema have been made.
 */
class SchemaManager {

    /**
     * LdapJDK main schema object
     */
    private LDAPSchema m_schema;
    
    /**
     * LDAP Connection object
     */
    private LDAPConnection m_ld;
    
    /**
     * Flag whether schema needs to be loaded by calling fetchSchema()
     */
    private boolean m_isLoaded;
    
    /**
     * Flag whether schema objects have been modified in the Directory (add, remove)
     * but the change has not been propagated to the cached m_schema object
     */
    private boolean m_isObjectClassDirty, m_isAttributeDirty, m_isMatchingRuleDirty;
    
    /**
     * Must constract with LDAP Connection
     */
    private SchemaManager() {}
    
    /**
     * Connstructor
     */
    public SchemaManager(LDAPConnection ld) {
        m_ld = ld;
        m_isLoaded = false;
        m_isObjectClassDirty = m_isAttributeDirty = m_isMatchingRuleDirty = false;
    }
    
    /**
     * Load the schema
     */
    void load() throws NamingException {
        try {
            m_schema = new LDAPSchema();
            m_schema.fetchSchema(m_ld);
            m_isLoaded = true;
            m_isObjectClassDirty = m_isAttributeDirty = m_isMatchingRuleDirty = false;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }    
    
    LDAPObjectClassSchema getObjectClass(String name) throws NamingException {
        if (!m_isLoaded || m_isObjectClassDirty) {
            load();
        }
        return m_schema.getObjectClass(name);
    }    

    LDAPAttributeSchema getAttribute(String name) throws NamingException {
        if (!m_isLoaded || m_isAttributeDirty) {
            load();
        }
        return m_schema.getAttribute(name);
    }    

    LDAPMatchingRuleSchema getMatchingRule(String name) throws NamingException {
        if (!m_isLoaded || m_isMatchingRuleDirty) {
            load();
        }
        return m_schema.getMatchingRule(name);
    }


    Enumeration getObjectClassNames() throws NamingException {
        if (!m_isLoaded || m_isObjectClassDirty) {
            load();
        }
        return m_schema.getObjectClassNames();
    }    

    Enumeration getAttributeNames() throws NamingException {
        if (!m_isLoaded || m_isAttributeDirty) {
            load();
        }
        return m_schema.getAttributeNames();
    }    

    Enumeration getMatchingRuleNames() throws NamingException {
        if (!m_isLoaded || m_isMatchingRuleDirty) {
            load();
        }
        return m_schema.getMatchingRuleNames();
    }

    Enumeration getObjectClasses() throws NamingException {
        if (!m_isLoaded || m_isObjectClassDirty) {
            load();
        }
        return m_schema.getObjectClasses();
    }    

    Enumeration getAttributes() throws NamingException {
        if (!m_isLoaded || m_isAttributeDirty) {
            load();
        }
        return m_schema.getAttributes();
    }    

    Enumeration getMatchingRules() throws NamingException {
        if (!m_isLoaded || m_isMatchingRuleDirty) {
            load();
        }
        return m_schema.getMatchingRules();
    }

     void createObjectClass(LDAPObjectClassSchema objclass) throws NamingException {
        try {
            objclass.add(m_ld);
            m_isObjectClassDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }    

    void createAttribute(LDAPAttributeSchema attr) throws NamingException {
        try {
            attr.add(m_ld);
            m_isAttributeDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }    

     void createMatchingRule(LDAPMatchingRuleSchema mrule) throws NamingException {
        try {
            mrule.add(m_ld);
            m_isMatchingRuleDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

     void removeObjectClass(String name) throws NamingException {
         LDAPObjectClassSchema objclass = getObjectClass(name);
         
        if (objclass == null) {
            throw new NameNotFoundException(name);
        }
        
        try {
            objclass.remove(m_ld);
            m_isObjectClassDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }    

    void removeAttribute(String name) throws NamingException {
         LDAPAttributeSchema attr = getAttribute(name);
         
        if (attr == null) {
            throw new NameNotFoundException(name);
        }

        try {
            attr.remove(m_ld);
            m_isAttributeDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }    

     void removeMatchingRule(String name) throws NamingException {
         LDAPMatchingRuleSchema mrule = getMatchingRule(name);
         
        if (mrule == null) {
            throw new NameNotFoundException(name);
        }

        try {
            mrule.remove(m_ld);
            m_isMatchingRuleDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

     void modifyObjectClass(LDAPObjectClassSchema objclass, LDAPObjectClassSchema modObjClass) throws NamingException {
        try {
            objclass.modify(m_ld, modObjClass);
            m_isObjectClassDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }    

    void modifyAttribute(LDAPAttributeSchema attr, LDAPAttributeSchema modAttr) throws NamingException {
        try {
            attr.modify(m_ld, modAttr);
            m_isAttributeDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }    

     void modifyMatchingRule(LDAPMatchingRuleSchema mrule, LDAPMatchingRuleSchema modMRule) throws NamingException {
        try {
            mrule.modify(m_ld, modMRule);
            m_isMatchingRuleDirty = true;
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

}
