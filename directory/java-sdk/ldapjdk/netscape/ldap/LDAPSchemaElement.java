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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap;

import java.util.*;

/**
 *
 * Abstract class representing an element (such as an object class
 * definition, an attribute type definition, or a matching rule
 * definition) in the schema. The specific types of elements are
 * represented by the <CODE>LDAPObjectClassSchema</CODE>,
 * <CODE>LDAPAttributeSchema</CODE>, and <CODE>LDAPMatchingRuleSchema</CODE>
 * subclasses.
 * <P>
 *
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of an object class,
 * attribute type, or matching rule.  All of these schema elements
 * can specify the following information:
 * <P>
 *
 * <UL>
 * <LI>a name identifying the element
 * <LI>an OID identifying the element
 * <LI>a description of the element
 * </UL>
 * <P>
 *
 * The <CODE>LDAPSchemaElement</CODE> class implements methods that
 * you can use with different types of schema elements (object class
 * definitions, attribute type definitions, and matching rule definitions).
 * You can do the following:
 * <UL>
 * <LI>get the name of a schema element
 * <LI>get the OID of a schema element
 * <LI>get the description of a schema element
 * <LI>add an element to the schema
 * <LI>remove an element from the schema
 * </UL>
 * <P>
 *
 * @see netscape.ldap.LDAPObjectClassSchema
 * @see netscape.ldap.LDAPAttributeSchema
 * @see netscape.ldap.LDAPMatchingRuleSchema
 * @version 1.0
 **/

public abstract class LDAPSchemaElement {
    /**
     * Construct a blank element.
     */
    protected LDAPSchemaElement() {
    }

    /**
     * Construct a definition explicitly.
     * @param name Name of element.
     * @param oid Dotted-string object identifier.
     * @param description Description of element.
     */
    protected LDAPSchemaElement( String name, String oid,
                                 String description ) {
        this.name = name;
        this.oid = oid;
        this.description = description;
    }

    /**
     * Gets the name of the object class, attribute type, or matching rule.
     * @return The name of the object class, attribute type, or
     * matching rule.
     */
    public String getName() {
        return name;
    }

    /**
     * Gets the object ID (OID) of the object class, attribute type,
     * or matching rule in dotted-string format (for example, "1.2.3.4").
     * @return The OID of the object class, attribute type,
     * or matching rule.
     */
    public String getOID() {
        return oid;
    }

    /**
     * Get the description of the object class, attribute type,
     * or matching rule.
     * @return The description of the object class, attribute type,
     * or matching rule.
     */
    public String getDescription() {
        return description;
    }

    /**
     * Add, remove or modify the definition from a Directory.
     * @param ld An open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op Type of modification to make.
     * @param atrr Attribute in the schema entry to modify.
     * @exception LDAPException if the definition can't be added/removed.
     */
    protected void update( LDAPConnection ld, int op, LDAPAttribute attr )
                            throws LDAPException {
        LDAPAttribute[] attrs = new LDAPAttribute[1];
        attrs[0] = attr;
        update( ld, op, attrs );
    }

    /**
     * Add, remove or modify the definition from a Directory.
     * @param ld An open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op Type of modification to make.
     * @param attrs Attributes in the schema entry to modify.
     * @exception LDAPException if the definition can't be added/removed.
     */
    protected void update( LDAPConnection ld, int op, LDAPAttribute[] attrs )
                            throws LDAPException {
        LDAPModificationSet mods = new LDAPModificationSet();
        for( int i = 0; i < attrs.length; i++ ) {
            mods.add( op, attrs[i] );
        }
        ld.modify( "cn=schema", mods );
    }

    /**
     * Add, remove or modify the definition from a Directory.
     * @param ld An open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op Type of modification to make.
     * @param name Name of attribute in the schema entry to modify.
     * @exception LDAPException if the definition can't be added/removed.
     */
    protected void update( LDAPConnection ld, int op, String name )
                            throws LDAPException {
        LDAPAttribute attr = new LDAPAttribute( name,
                                                getValue() );
        update( ld, op, attr );
    }

    /**
     * Adds the current object class, attribute type, or matching rule
     * definition to the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @exception LDAPException The specified definition cannot be
     * added to the schema.
     */
    public void add( LDAPConnection ld ) throws LDAPException {
        update( ld, LDAPModification.ADD, attrName );
    }

    /**
     * Replace a single value of the object class, attribute type,
     * or matching rule definition in the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @param newValue The new value
     * @exception LDAPException The specified definition cannot be
     * modified.
     */
    public void modify( LDAPConnection ld, LDAPSchemaElement newValue )
                        throws LDAPException {
        LDAPModificationSet mods = new LDAPModificationSet();
        mods.add( LDAPModification.DELETE,
                  new LDAPAttribute( attrName, getValue() ) );
        mods.add( LDAPModification.ADD,
                  new LDAPAttribute( attrName, newValue.getValue() ) );
        ld.modify( "cn=schema", mods );
    }

    /**
     * Removes the current object class, attribute type, or matching rule
     * definition from the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @exception LDAPException The specified definition cannot be
     * removed from the schema.
     */
    public void remove( LDAPConnection ld ) throws LDAPException {
        update( ld, LDAPModification.DELETE, attrName );
    }

    /**
     * Gets the definition of the current object class, attribute type, or
     * matching rule in a format expected by the server when adding the
     * definition to the directory. This must be overridden by derived classes.
     * @return A String that can be used as a value of the element
     * in the schema of a Directory.
     */
    public abstract String getValue();

    public static final int unknown = 0;
    public static final int cis = 1;
    public static final int binary = 2;
    public static final int telephone = 3;
    public static final int ces = 4;
    public static final int dn = 5;
    public static final int integer = 6;

    protected static final String cisString =
                                      "1.3.6.1.4.1.1466.115.121.1.15";
    protected static final String binaryString =
                                      "1.3.6.1.4.1.1466.115.121.1.5";
    protected static final String telephoneString =
                                      "1.3.6.1.4.1.1466.115.121.1.50";
    protected static final String cesString =
                                      "1.3.6.1.4.1.1466.115.121.1.26";
    protected static final String intString =
                                      "1.3.6.1.4.1.1466.115.121.1.27";
    protected static final String dnString =
                                      "1.3.6.1.4.1.1466.115.121.1.12";
    protected String oid = "";
    protected String name = "";
    protected String description = "";
    protected String attrName = null;
}
