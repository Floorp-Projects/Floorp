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

class SchemaElementBindingEnum implements NamingEnumeration {

    /**
     * Enumeration of schema name-object bindings packaged into Binding object.
     */
    Enumeration m_schemaElementEnum;
    
    SchemaManager m_schemaMgr;

    static final String _className = "javax.naming.directory.DirContext"; // for class name is bindings

    public SchemaElementBindingEnum(Enumeration schemaElementEnum, SchemaManager schemaMgr) {
        m_schemaElementEnum = schemaElementEnum;
        m_schemaMgr = schemaMgr;
    }

    public Object next() throws NamingException{
        return nextElement();
    }

    public Object nextElement() {
        DirContext obj = null;
        LDAPSchemaElement schemaElement = (LDAPSchemaElement) m_schemaElementEnum.nextElement();
        if (schemaElement instanceof LDAPObjectClassSchema) {
            obj = new SchemaObjectClass((LDAPObjectClassSchema) schemaElement, m_schemaMgr);
        }
        else if (schemaElement instanceof LDAPAttributeSchema) {
            obj = new SchemaAttribute((LDAPAttributeSchema) schemaElement, m_schemaMgr);
        }
        else if (schemaElement instanceof LDAPMatchingRuleSchema) {
            obj = new SchemaMatchingRule((LDAPMatchingRuleSchema) schemaElement, m_schemaMgr);
        }
        return new Binding(schemaElement.getName(), _className, obj, /*isRelative=*/true);
    }

    public boolean hasMore() throws NamingException{
        return m_schemaElementEnum.hasMoreElements();
    }

    public boolean hasMoreElements() {
        return m_schemaElementEnum.hasMoreElements();
    }

    public void close() {}
}

