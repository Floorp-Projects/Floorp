/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.util.*;

/**
 * The definition of a name form in the schema.
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of a name form.
 * According to the RFC, the description of a name form can
 * include the following information:
 * <P>
 *
 * <UL>
 * <LI>an OID identifying the name form
 * <LI>a name identifying the name form
 * <LI>a description of the name form
 * <LI>the structural object class of this name form
 * <LI>the list of attribute types that are required in this name form
 * <LI>the list of attribute types that are allowed (optional) in this
 * name form
 * </UL>
 * <P>
 *
 * When you construct an <CODE>LDAPNameFormSchema</CODE> object,
 * you can specify
 * these types of information as arguments to the constructor or in the
 * NameFormDescription format specified in RFC 2252.
 * When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.
 * <P>
 *
 * You can get the name, OID, and description of this name form
 * definition by using the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
 * <CODE>getDescription</CODE> methods inherited from the abstract class
 * <CODE>LDAPSchemaElement</CODE>. Optional and custom qualifiers are
 * accessed with <CODE>getQualifier</CODE> and <CODE>getQualifierNames</CODE>
 * from <CODE>LDAPSchemaElement</CODE>.
 
 * <P>
 *
 * To add or remove this name form definition from the
 * schema, use the <CODE>add</CODE> and <CODE>remove</CODE>
 * methods, which this class inherits from the <CODE>LDAPSchemaElement</CODE>
 * abstract class.
 * <P>
 * RFC 2252 defines NameFormDescription as follows:
 * <P>
 * <PRE>
 *    NameFormDescription = "(" whsp
 *        numericoid whsp      ; NameForm identifier
 *        [ "NAME" qdescrs ]
 *        [ "DESC" qdstring ]
 *        [ "OBSOLETE" whsp ]
 *        "OC" woid            ; Structural ObjectClass
 *        [ "MUST" oids ]      ; AttributeTypes
 *        [ "MAY" oids ]       ; AttributeTypes
 *    whsp ")"
 * </PRE>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPSchemaElement
 **/
public class LDAPNameFormSchema extends LDAPSchemaElement {

    static final long serialVersionUID = 1665316286199590403L;

    /**
     * Constructs a name form definition, using the specified
     * information.
     * @param names name of the name form
     * @param oid object identifier (OID) of the name form
     * in dotted-string format (for example, "1.2.3.4")
     * @param description description of the name form
     * @param obsolete <code>true</code> if the rule is obsolete
     * @param objectClass the object to which this name form applies.
     * This may either be specified by name or numeric oid.
     * @param required array of names of attributes required
     * in this name form
     * @param optional array of names of optional attributes
     * allowed in this name form
     */
    public LDAPNameFormSchema( String[] names,
                               String oid,
                               String description,
                               boolean obsolete,
                               String objectClass,
                               String[] required,
                               String[] optional ) {
        super( names, oid, description );
        attrName = "nameforms";
        if ( obsolete ) {
            setQualifier( OBSOLETE, "" );
        }
        this.objectClass = objectClass;
        if ( required != null ) {
            for( int i = 0; i < required.length; i++ ) {
                must.addElement( required[i] );
            }
        }
        if ( optional != null ) {
            for( int i = 0; i < optional.length; i++ ) {
                may.addElement( optional[i] );
            }
        }
    }

    /**
     * Constructs a name form definition based on a description in
     * the NameFormDescription format.  For information on this format,
     * (see <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
     * Attribute Syntax Definitions</A>.  This is the format that LDAP servers
     * and clients use to exchange schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of the "objectclasses" attribute are name form descriptions
     * in this format.)
     * <P>
     *
     * @param raw definition of the object in the NameFormDescription
     * format
     */
    public LDAPNameFormSchema( String raw ) {
        attrName = "objectclasses";
        parseValue( raw );
        Object o = properties.get( "MAY" );
        if ( o != null ) {
            if ( o instanceof Vector ) {
                may = (Vector)o;
            } else {
                may.addElement( o );
            }
        }
        o = properties.get( "MUST" );
        if ( o != null ) {
            if ( o instanceof Vector ) {
                must = (Vector)o;
            } else {
                must.addElement( o );
            }
        }
        o = properties.get( "OC" );
        if ( o != null ) {
            objectClass = (String)o;
        }
    }

    /**
     * Returns the name of the object class that this name form applies to.
     *
     * @return the name of the object class that this name form applies to.
     */
    public String getObjectClass() {
        return objectClass;
    }

    /**
     * Gets the names of optional attributes allowed
     * in this name form.
     * @return the names of optional attributes
     * allowed in this name form.
     */
    public String[] getOptionalNamingAttributes() {
        String[] vals = new String[may.size()];
        may.copyInto( vals );
        return vals;
    }

    /**
     * Gets the names of the required attributes for
     * this name form.
     * @return the names of the required attributes
     * for this name form.
     */
    public String[] getRequiredNamingAttributes() {
        String[] vals = new String[must.size()];
        must.copyInto( vals );
        return vals;
    }

    /**
     * Gets the attribute name for a schema element
     *
     * @return The attribute name of the element
     */
    String getAttributeName() {
        return "nameforms";
    }

    /**
     * Prepares a value in RFC 2252 format for submitting to a server.
     *
     * @param quotingBug <CODE>true</CODE> if SUP and SYNTAX values are to
     * be quoted. That is to satisfy bugs in certain LDAP servers.
     * @return a String ready for submission to an LDAP server.
     */
    String getValue( boolean quotingBug ) {
        String s = getValuePrefix();
        String val = getOptionalValues( NOVALS );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        s += "OC " + objectClass + ' ';
        if ( must.size() > 0 ) {
            s += "MUST " + vectorToList( must );
            s += ' ';
        }
        if ( may.size() > 0 ) {
            s += "MAY " + vectorToList( may );
            s += ' ';
        }
        val = getCustomValues();
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        s += ')';
        return s;
    }

    /**
     * Gets the definition of the name form in Directory format
     *
     * @return definition of the name form in Directory format
     */
    public String toString() {
        return getValue( false );
    }

    /**
     * Creates a list within parentheses, with $ as delimiter
     *
     * @param vals values for list
     * @return a String with a list of values.
     */
    protected String vectorToList( Vector vals ) {
        String val = "( ";
        for( int i = 0; i < vals.size(); i++ ) {
            val += (String)vals.elementAt(i) + ' ';
            if ( i < (vals.size() - 1) ) {
                val += "$ ";
            }
        }
        val += ')';
        return val;
    }

    private Vector must = new Vector();
    private Vector may = new Vector();
    private String objectClass = null;

    // Qualifiers known to not have values; prepare a Hashtable
    static final String[] NOVALS = { "OBSOLETE" };
    static {
        for( int i = 0; i < NOVALS.length; i++ ) {
            novalsTable.put( NOVALS[i], NOVALS[i] );
        }
    }

    // Qualifiers which we output explicitly in toString()
    static final String[] IGNOREVALS = { "MUST", "MAY",
                                         "OBJECTCLASS", "OBSOLETE"};
}

