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
import com.netscape.jndi.ldap.common.DirContextAdapter;

import java.util.*;

public class SchemaDirContext extends DirContextAdapter {

    public static final String CLASSDEF = "ClassDefinition";
    public static final String ATTRDEF = "AttributeDefinition";
    public static final String MRULEDEF = "MatchingRule";
    

    String m_path;

    public void close() throws NamingException {
        ; //NOP
    }

    /**
     * Name operations
     */

    public String composeName(String name, String prefix) throws NamingException {
        return name + "," + prefix;
    }

    public Name composeName(Name name, Name prefix) throws NamingException {
        String compoundName = composeName(name.toString(), prefix.toString());
        return SchemaNameParser.getParser().parse(compoundName);
    }

    public String getNameInNamespace() throws NamingException {
        return new String(m_path);
    }

    public NameParser getNameParser(String name) throws NamingException {
        return SchemaNameParser.getParser();
    }

    public NameParser getNameParser(Name name) throws NamingException {
        return SchemaNameParser.getParser();
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
        try {
            bind(name, obj);
        }
        catch (NameAlreadyBoundException ex) {
            unbind(name);
            bind(name, obj);
        }
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
     * Empty enumeration for list operations
     */
    class EmptyNamingEnumeration implements NamingEnumeration {

        public Object next() throws NamingException{
            throw new NoSuchElementException("EmptyNamingEnumeration");                
        }

        public Object nextElement() {
            throw new NoSuchElementException("EmptyNamingEnumeration");                
        }

        public boolean hasMore() throws NamingException{
            return false;
        }

        public boolean hasMoreElements() {
            return false;
        }

        public void close() {}
    }
    
    static class SchemaObjectSubordinateNamePair {
        SchemaDirContext schemaObj;
        String subordinateName;
        
        public SchemaObjectSubordinateNamePair(SchemaDirContext object, String subordinateName) {
            this.schemaObj = object;
            this.subordinateName = subordinateName;
        }
        
        public String toString() {
            StringBuffer str = new StringBuffer("SchemaObjectSubordinateNamePair{obj:");
            str.append(((schemaObj == null) ? "null" : schemaObj.toString()));
            str.append(" name:");
            str.append(subordinateName);
            str.append("}");
            return str.toString();
        }
    }    
}
