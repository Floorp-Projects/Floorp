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

public class SchemaAttributeContainer extends SchemaElementContainer {

    public SchemaAttributeContainer(SchemaManager schemaMgr) throws NamingException{
        super(schemaMgr, ATTRDEF);
    }
    
    /**
     * Ldap entry operations
     */

    public DirContext createSchemaElement(String name, Attributes attrs) throws NamingException {
        if (name.length() == 0) {
            throw new NamingException("Empty name for schema objectclass");
        }
        LDAPAttributeSchema attr = SchemaAttribute.parseDefAttributes(attrs);
        m_schemaMgr.createAttribute(attr);
        return new SchemaAttribute(attr, m_schemaMgr);

    }

    public void removeSchemaElement(String name) throws NamingException {
        if (name.length() == 0) {
            throw new NamingException("Can not delete schema object container");
        }
        m_schemaMgr.removeAttribute(name);
    }

    /**
     * List Operations
     */

    public NamingEnumeration getNameList(String name) throws NamingException {
        SchemaDirContext schemaObj = (SchemaDirContext)lookup(name);
        if (schemaObj == this) {
            return new SchemaElementNameEnum(m_schemaMgr.getAttributeNames());
        }
        else {
            throw new NotContextException(name);
        }
    }

    public NamingEnumeration getBindingsList(String name) throws NamingException {
        SchemaDirContext schemaObj = (SchemaDirContext)lookup(name);
        if (schemaObj == this) {
            return new SchemaElementBindingEnum(m_schemaMgr.getAttributes(), m_schemaMgr);
        }
        else {
            throw new NotContextException(name);
        }
    }

    /**
     * Lookup Operations
     */

    public Object lookupSchemaElement(String name) throws NamingException {    
        if (name.length() == 0) {
            return this;
        }
        
        // No caching; Always create a new object
        LDAPAttributeSchema attr = m_schemaMgr.getAttribute(name);
        if (attr == null) {
            throw new NameNotFoundException(name);
        }
        return new SchemaAttribute(attr, m_schemaMgr);

    }
}
