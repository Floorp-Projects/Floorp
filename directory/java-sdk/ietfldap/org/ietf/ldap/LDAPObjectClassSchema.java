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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.util.*;

/**
 * The definition of an object class in the schema.
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of an object class.
 * According to the RFC, the description of an object class can
 * include the following information:
 * <P>
 *
 * <UL>
 * <LI>an OID identifying the object class
 * <LI>a name identifying the object class
 * <LI>a description of the object class
 * <LI>the name of the parent object class
 * <LI>the list of attribute types that are required in this object class
 * <LI>the list of attribute types that are allowed (optional) in this
 * object class
 * </UL>
 * <P>
 *
 * When you construct an <CODE>LDAPObjectSchema</CODE> object, you can specify
 * these types of information as arguments to the constructor or in the
 * ObjectClassDescription format specified in RFC 2252.
 * When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.
 * <P>
 *
 * RFC 2252 also notes that you can specify whether or not an object class
 * is abstract, structural, or auxiliary in the object description.
 * Abstract object classes are used only to derive other object classes.
 * Entries cannot belong to an abstract object class. <CODE>top</CODE>
 * is an abstract object class. Entries must belong to a structural
 * object class, so most object classes are structural object classes.
 * Objects of the <CODE>LDAPObjectClassSchema</CODE> class are structural
 * object classes by default. Auxiliary object classes can be used to
 * add attributes to entries of different types. For example, an
 * auxiliary object class might be used to specify personal preference
 * attributes. An entry can not contain just that object class, but may
 * include it along with a structural object class, for example
 * inetOrgPerson.
 * If the definition of an object (in ObjectClassDescription format)
 * specifies the AUXILIARY keyword, an <CODE>LDAPObjectClassSchema</CODE>
 * object created from that description represents an auxiliary object class.
 * <P>
 *
 * You can get the name, OID, and description of this object class
 * definition by using the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
 * <CODE>getDescription</CODE> methods inherited from the abstract class
 * <CODE>LDAPSchemaElement</CODE>. Optional and custom qualifiers are
 * accessed with <CODE>getQualifier</CODE> and <CODE>getQualifierNames</CODE>
 * from <CODE>LDAPSchemaElement</CODE>.
 
 * <P>
 *
 * To add or remove this object class definition from the
 * schema, use the <CODE>add</CODE> and <CODE>remove</CODE>
 * methods, which this class inherits from the <CODE>LDAPSchemaElement</CODE>
 * abstract class.
 * <P>
 * RFC 2252 defines ObjectClassDescription as follows:
 * <P>
 * <PRE>
 *    ObjectClassDescription = "(" whsp
 *        numericoid whsp      ; ObjectClass identifier
 *        [ "NAME" qdescrs ]
 *        [ "DESC" qdstring ]
 *        [ "OBSOLETE" whsp ]
 *        [ "SUP" oids ]       ; Superior ObjectClasses
 *        [ ( "ABSTRACT" / "STRUCTURAL" / "AUXILIARY" ) whsp ]
 *                             ; default structural
 *        [ "MUST" oids ]      ; AttributeTypes
 *        [ "MAY" oids ]       ; AttributeTypes
 *    whsp ")"
 * </PRE>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPSchemaElement
 **/
public class LDAPObjectClassSchema extends LDAPSchemaElement {

    static final long serialVersionUID = -1732784695071118656L;

    /**
     * Constructs an object class definition, using the specified
     * information.
     * @param names names of the object class
     * @param oid object identifier (OID) of the object class
     * in dotted-string format (for example, "1.2.3.4")
     * @param description description of the object class
     * @param superiors names of parent object classes
     * (the object classes that this object class inherits from)
     * @param required array of names of attributes required
     * in this object class
     * @param optional array of names of optional attributes
     * allowed in this object class
     * @param type either ABSTRACT, STRUCTURAL, or AUXILIARY
     * @param obsolete <code>true</code> if the rule is obsolete
     */
    public LDAPObjectClassSchema( String[] names,
                                  String oid,
                                  String[] superiors,
                                  String description,
                                  String[] required,
                                  String[] optional,
                                  int type,
                                  boolean obsolete ) {
        super( names, oid, description );
        attrName = "objectclasses";
        setQualifier( SUPERIOR, superiors );
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
        if ( (superiors != null) && (superiors.length > 1) ) {
            setQualifier( SUPERIOR, superiors );
        }
        if ( obsolete ) {
            setQualifier( OBSOLETE, "" );
        }
    }

    /**
     * Constructs an object class definition based on a description in
     * the ObjectClassDescription format.  For information on this format,
     * (see <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
     * Attribute Syntax Definitions</A>.  This is the format that LDAP servers
     * and clients use to exchange schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of the "objectclasses" attribute are object class descriptions
     * in this format.)
     * <P>
     *
     * @param raw definition of the object in the ObjectClassDescription
     * format
     */
    public LDAPObjectClassSchema( String raw ) {
        attrName = "objectclasses";
        parseValue( raw );
        setQualifier( TYPE, typeToString( getType() ) );
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
    }

    /**
     * Gets an enumeration of names of optional attributes allowed
     * in this object class.
     * @return an enumeration of the names of optional attributes
     * allowed in this object class.
     */
    public Enumeration getOptionalAttributes() {
        return may.elements();
    }

    /**
     * Gets an enumeration of the names of the required attributes for
     * this object class.
     * @return an enumeration of the names of the required attributes
     * for this object class.
     */
    public Enumeration getRequiredAttributes() {
        return must.elements();
    }

    /**
     * Gets the names of all object classes that this class inherits
     * from. Typically only one, but RFC 2252 allows multiple
     * inheritance.
     * @return the names of the object classes from which this class
     * inherits.
     */
    public String[] getSuperiors() {
        return getQualifier( SUPERIOR );
    }

    /**
     * Gets the type of the object class.
     * @return STRUCTURAL, ABSTRACT, or AUXILIARY.
     */
    public int getType() {
        int type = STRUCTURAL;
        if ( properties.containsKey( "AUXILIARY" ) ) {
            type = AUXILIARY;
        } else if ( properties.containsKey( "ABSTRACT" ) ) {
            type = ABSTRACT;
        }
        return type;
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
        String val = getValue( SUPERIOR, quotingBug );
        if ( (val != null) && (val.length() > 0) ) {
            s += val + ' ';
        }
        String[] vals = getQualifier( TYPE );
        if ( (vals != null) && (vals.length > 0) ) {
            s += vals[0] + ' ';
        }
        val = getOptionalValues( NOVALS );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
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
     * Gets the definition of the object class in Directory format
     *
     * @return definition of the object class in Directory format
     */
    public String toString() {
        return getValue();
    }

    /**
     * Gets the attribute name for a schema element
     *
     * @return The attribute name of the element
     */
    String getAttributeName() {
        return "objectclasses";
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

    /**
     * Returns the object class type as a String
     *
     * @param type one of STRUCTURAL, ABSTRACT, or AUXILIARY
     * @return one of "STRUCTURAL", "ABSTRACT", "AUXILIARY", or <CODE>null</CODE>
     */
    protected String typeToString( int type ) {
        switch( type ) {
        case STRUCTURAL: return "STRUCTURAL";
        case ABSTRACT: return "ABSTRACT";
        case AUXILIARY: return "AUXILIARY";
        default: return null;
        }
    }

    public static final int STRUCTURAL = 0;
    public static final int ABSTRACT = 1;
    public static final int AUXILIARY = 2;

    private Vector must = new Vector();
    private Vector may = new Vector();
    private int type = STRUCTURAL;

    // Qualifiers known to not have values; prepare a Hashtable
    static final String[] NOVALS = { "ABSTRACT", "STRUCTURAL",
                                     "AUXILIARY", "OBSOLETE" };
    static {
        for( int i = 0; i < NOVALS.length; i++ ) {
            novalsTable.put( NOVALS[i], NOVALS[i] );
        }
    }

    // Qualifiers which we output explicitly in toString()
    static final String[] IGNOREVALS = { "ABSTRACT", "STRUCTURAL",
                                         "AUXILIARY", "MUST", "MAY",
                                         "SUP", "OBSOLETE"};
    // Key for type in the properties Hashtable
    static final String TYPE = "TYPE";
}

