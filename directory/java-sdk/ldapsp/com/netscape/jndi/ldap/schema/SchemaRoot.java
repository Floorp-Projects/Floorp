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

public class SchemaRoot extends SchemaDirContext {

    static final String m_className = "javax.naming.directory.DirContext"; // for class name is bindings
    
    SchemaDirContext m_classContainer, m_attrContainer, m_matchRuleContainer;
    
    SchemaManager m_schemaMgr;
    
    public SchemaRoot(LDAPConnection ld) throws NamingException{
        m_path = "";
        m_schemaMgr = new SchemaManager(ld);
        m_classContainer = new SchemaObjectClassContainer(m_schemaMgr);
        m_attrContainer = new SchemaAttributeContainer(m_schemaMgr);
        m_matchRuleContainer = new SchemaMatchingRuleContainer(m_schemaMgr);
    }

    SchemaObjectSubordinateNamePair resolveSchemaObject(String name) throws NamingException{
        
        SchemaDirContext obj = null;
        
        if (name.length() == 0) {
            obj = this;
        }
        else if (name.startsWith(CLASSDEF) || name.startsWith(CLASSDEF.toLowerCase())) {
            name = name.substring(CLASSDEF.length());
            obj = m_classContainer;
        }    
        else if (name.startsWith(ATTRDEF) || name.startsWith(ATTRDEF.toLowerCase())) {
            name = name.substring(ATTRDEF.length());
            obj = m_attrContainer;
        }    
        else if (name.startsWith(MRULEDEF) || name.startsWith(MRULEDEF.toLowerCase())) {
            name = name.substring(MRULEDEF.length());
            obj = m_matchRuleContainer;
            
        }
        else {
            throw new NameNotFoundException(name);
        }
        
        if (name.length() > 1 && name.startsWith("/")) {
            name = name.substring(1);
        }    
        return new SchemaObjectSubordinateNamePair(obj, name);
    }    
    

    /**
     * Attribute Operations
     */

    public Attributes getAttributes(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);        
        if (objNamePair.schemaObj == this) {
            throw new OperationNotSupportedException();
        }
        else {
            return objNamePair.schemaObj.getAttributes(objNamePair.subordinateName);
        }
    }

    public Attributes getAttributes(String name, String[] attrIds) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);        
        if (objNamePair.schemaObj == this) {
            throw new OperationNotSupportedException();
        }
        else {
            return objNamePair.schemaObj.getAttributes(objNamePair.subordinateName, attrIds);
        }
    }

    public Attributes getAttributes(Name name) throws NamingException {
        return getAttributes(name.toString());
    }

    public Attributes getAttributes(Name name, String[] attrIds) throws NamingException {
        return getAttributes(name.toString(), attrIds);
    }

    public void modifyAttributes(String name, int mod_op, Attributes attrs) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void modifyAttributes(String name, ModificationItem[] mods) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void modifyAttributes(Name name, int mod_op, Attributes attrs) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void modifyAttributes(Name name, ModificationItem[] mods) throws NamingException {
        throw new OperationNotSupportedException();
    }

    /**
     * Ldap entry operations
     */

    public Context createSubcontext(String name) throws NamingException {
        // Directory entry must have attributes
        throw new OperationNotSupportedException();
    }

    public Context createSubcontext(Name name) throws NamingException {
        // Directory entry must have attributes
        throw new OperationNotSupportedException();
    }

    public DirContext createSubcontext(String name, Attributes attrs) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            throw new OperationNotSupportedException();
        }
        else {
            return objNamePair.schemaObj.createSubcontext(objNamePair.subordinateName, attrs);
        }
    }

    public DirContext createSubcontext(Name name, Attributes attrs) throws NamingException {
        return createSubcontext(name.toString(), attrs);
    }

    public void destroySubcontext(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            throw new OperationNotSupportedException();
        }
        else {
            objNamePair.schemaObj.destroySubcontext(objNamePair.subordinateName);
        }
    }

    public void destroySubcontext(Name name) throws NamingException {
        destroySubcontext(name.toString());
    }

    /**
     * Naming Bind operations
     */

    public void bind(String name, Object obj) throws NamingException {
        if (obj instanceof DirContext) {
            createSubcontext(name, ((DirContext)obj).getAttributes(""));
        }
        else {
            throw new IllegalArgumentException("Can not bind this type of object");
        }    
    }

    public void bind(Name name, Object obj) throws NamingException {
        bind(name.toString(), obj);
    }

    public void rebind(String name, Object obj) throws NamingException {
        unbind(name);
        bind(name, obj);
    }

    public void rebind(Name name, Object obj) throws NamingException {
        rebind(name.toString(), obj);
    }

    public void rename(String oldName, String newName) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void rename(Name oldName, Name newName) throws NamingException {
        rename(oldName.toString(), newName.toString());
    }

    public void unbind(String name) throws NamingException {
        // In ldap every entry is naming context
        destroySubcontext(name);
    }

    public void unbind(Name name) throws NamingException {
        // In ldap every entry is naming context
        destroySubcontext(name);
    }

    /**
     * List Operations
     */

    public NamingEnumeration list(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            return new SchemaRootNameClassPairEnum();
        }
        else {
            return objNamePair.schemaObj.list(objNamePair.subordinateName);
        }
    }

    public NamingEnumeration list(Name name) throws NamingException {
        return list(name.toString());
    }

    public NamingEnumeration listBindings(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            return new SchemaRootBindingEnum();
        }
        else {
            return objNamePair.schemaObj.listBindings(objNamePair.subordinateName);
        }

    }

    public NamingEnumeration listBindings(Name name) throws NamingException {
        return listBindings(name.toString());
    }

    /**
     * Lookup Operations
     */

    public Object lookup(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            return this;
        }
        else {
            return objNamePair.schemaObj.lookup(objNamePair.subordinateName);
        }

    }

    public Object lookup(Name name) throws NamingException {
        return lookup(name.toString());
    }

    public Object lookupLink(String name) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public Object lookupLink(Name name) throws NamingException {
        throw new OperationNotSupportedException();
    }

    /**
     * Test program
     */
    public static void main(String args[]) {
        try {
            String name = args[0];
            System.out.println((new SchemaRoot(null)).resolveSchemaObject(name));
        }
        catch (Exception e) {
            System.err.println(e);
        }    
    }
    
    /**
     * NamigEnumeration of NameClassPairs
     */
    class SchemaRootNameClassPairEnum implements NamingEnumeration {

        private int m_idx = -1;

        public Object next() {
            return nextElement();
        }

        public Object nextElement() {
            m_idx++;
            if (m_idx == 0 ) {
                return new NameClassPair(CLASSDEF, m_className);
            }
            else if (m_idx == 1) {
                return new NameClassPair(ATTRDEF, m_className);
            }
            else if (m_idx == 2) {
                return new NameClassPair(MRULEDEF, m_className);
            }
            else {
                throw new NoSuchElementException("SchemaRootEnumerator");
            }    
                
        }

        public boolean hasMore() {
            return hasMoreElements();
        }

        public boolean hasMoreElements() {
            return m_idx < 2;
        }

        public void close() {}
    }

    /**
     * NamingEnumeration of Bindings
     */
    class SchemaRootBindingEnum implements NamingEnumeration {

        private int m_idx = -1;

        public Object next() {
            return nextElement();
        }

        public Object nextElement() {
            m_idx++;
            if (m_idx == 0 ) {
                return new Binding(CLASSDEF, m_className, m_classContainer);
            }
            else if (m_idx == 1) {
                return new Binding(ATTRDEF, m_className, m_attrContainer);
            }
            else if (m_idx == 2) {
                return new Binding(MRULEDEF, m_className, m_matchRuleContainer);
            }
            else {
                throw new NoSuchElementException("SchemaRootEnumerator");
            }    
                
        }

        public boolean hasMore() {
            return hasMoreElements();
        }

        public boolean hasMoreElements() {
            return m_idx < 2;
        }

        public void close() {}
    }
}
