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

public class SchemaObjectClassContainer extends SchemaElementContainer {

    public SchemaObjectClassContainer(SchemaManager schemaMgr) throws NamingException{
        super(schemaMgr, CLASSDEF);
    }
    
    /**
     * Ldap entry operations
     */

    public DirContext createSchemaElement(String name, Attributes attrs) throws NamingException {
        if (name.length() == 0) {
            throw new NamingException("Empty name for schema objectclass");
        }
        LDAPObjectClassSchema objclass = SchemaObjectClass.parseDefAttributes(attrs);
        m_schemaMgr.createObjectClass(objclass);
        return new SchemaObjectClass(objclass, m_schemaMgr);

    }

    public void removeSchemaElement(String name) throws NamingException {
        if (name.length() == 0) {
            throw new NamingException("Can not delete schema object container");
        }    
        m_schemaMgr.removeObjectClass(name);
    }

    /**
     * List Operations
     */

    public NamingEnumeration getNameList(String name) throws NamingException {
        SchemaDirContext schemaObj = (SchemaDirContext)lookup(name);
        if (schemaObj == this) {
            return new SchemaElementNameEnum(m_schemaMgr.getObjectClassNames());
        }
        else {
            throw new NotContextException(name);
        }
    }

    public NamingEnumeration getBindingsList(String name) throws NamingException {
        SchemaDirContext schemaObj = (SchemaDirContext)lookup(name);
        if (schemaObj == this) {
            return new SchemaElementBindingEnum(m_schemaMgr.getObjectClasses(), m_schemaMgr);
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
        LDAPObjectClassSchema objclass = m_schemaMgr.getObjectClass(name);
        if (objclass == null) {
            throw new NameNotFoundException(name);
        }
        return new SchemaObjectClass(objclass, m_schemaMgr);

    }
}
