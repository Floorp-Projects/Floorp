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
 * The definition of an attribute type in the schema.
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of an attribute type.
 * According to the RFC, the description of an attribute type can
 * include the following information:
 * <P>
 *
 * <UL>
 * <LI>an OID identifying the attribute type
 * <LI>a name identifying the attribute type
 * <LI>a description of the attribute type
 * <LI>the name of the parent attribute type
 * <LI>the syntax used by the attribute (for example,
 * <CODE>cis</CODE> or <CODE>int</CODE>)
 * <LI>an indication of whether or not the attribute type is single-valued
 * or multi-valued
 * </UL>
 * <P>
 *
 * When you construct an <CODE>LDAPAttributeSchema</CODE> object, you can
 * specify these types of information as arguments to the constructor or
 * in the AttributeTypeDescription format specified in RFC 2252.
 * (When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.)
 * <P>
 *
 * You can get the name, OID, and description of this attribute type
 * definition by using the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
 * <CODE>getDescription</CODE> methods inherited from the abstract class
 * <CODE>LDAPSchemaElement</CODE>.
 * <P>
 *
 * If you need to add or remove this attribute type definition from the
 * schema, you can use the <CODE>add</CODE> and <CODE>remove</CODE>
 * methods, which this class inherits from the <CODE>LDAPSchemaElement</CODE>
 * abstract class.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPSchemaElement
 **/

public class LDAPAttributeSchema extends LDAPSchemaElement {
    /**
     * Construct a blank element.
     */
    protected LDAPAttributeSchema() {
        super();
    }

    /**
     * Constructs an attribute type definition, using the specified
     * information.
     * @param name Name of the attribute type.
     * @param oid Object identifier (OID) of the attribute type
     * in dotted-string format (for example, "1.2.3.4").
     * @param description Description of attribute type.
     * @param syntax Syntax of this attribute type. The value of this
     * argument can be one of the following:
     * <UL>
     * <LI><CODE>cis</CODE> (case-insensitive string)
     * <LI><CODE>ces</CODE> (case-exact string)
     * <LI><CODE>binary</CODE> (binary data)
     * <LI><CODE>int</CODE> (integer)
     * <LI><CODE>telephone</CODE> (telephone number -- identical to cis,
     * but blanks and dashes are ignored during comparisons)
     * <LI><CODE>dn</CODE> (distinguished name)
     * </UL>
     * @param single <CODE>true</CODE> if the attribute type is single-valued.
     */
    public LDAPAttributeSchema( String name, String oid, String description,
                            int syntax, boolean single ) {
        super( name, oid, description );
        attrName = "attributetypes";
        this.syntax = syntax;
        this.single = single;
    }

    /**
     * Constructs an attribute type definition based on a description in
     * the AttributeTypeDescription format. For information on this format,
     * (see <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
     * Attribute Syntax Definitions</A>.  This is the format that LDAP servers
     * and clients use to exchange schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of the "attributetypes" attribute are attribute type descriptions
     * in this format.)
     * <P>
     *
     * @param raw Definition of the attribute type in the
     * AttributeTypeDescription format.
     */
    public LDAPAttributeSchema( String raw ) {
        attrName = "attributetypes";
        raw.trim();
        // Skip past "( " and ")"
        int l = raw.length();
        raw = raw.substring( 2, l - 1 );
        l = raw.length();
        int ind = raw.indexOf( ' ' );
        oid = raw.substring( 0, ind );
        char[] ch = new char[l];

        raw = raw.substring( ind + 1, l );
        l = raw.length();
        raw.getChars( 0, l, ch, 0 );

        ind = 0;
        l = ch.length;
        while ( ind < l ) {
            String s = "";
            String val;
            while( ch[ind] == ' ' )
                ind++;
            int last = ind + 1;
            while( (last < l) && (ch[last] != ' ') )
                last++;
            if ( (ind < l) && (last < l) ) {
                s = new String( ch, ind, last-ind );
                ind = last;
                if ( s.equalsIgnoreCase( "SINGLE-VALUE" ) ) {
                    single = true;
                    continue;
                }
            } else {
                ind = l;
            }

            while( (ind < l) && (ch[ind] != '\'')  )
                ind++;
            last = ind + 1;
            while( (last < l) && (ch[last] != '\'') ) {
                last++;
            }
            if ( (ind < last) && (last < l) ) {
                val = new String( ch, ind+1, last-ind-1 );
                ind = last + 1;

                if ( s.equalsIgnoreCase( "NAME" ) ) {
                    name = val;
                } else if ( s.equalsIgnoreCase( "DESC" ) ) {
                    description = val;
                } else if ( s.equalsIgnoreCase( "SYNTAX" ) ) {
                    syntax = syntaxCheck( val );
                }
            }
        }
    }

    /**
     * Gets the syntax of the attribute type.
     * @return One of the following values:
     * <UL>
     * <LI><CODE>cis</CODE> (case-insensitive string)
     * <LI><CODE>ces</CODE> (case-exact string)
     * <LI><CODE>binary</CODE> (binary data)
     * <LI><CODE>int</CODE> (integer)
     * <LI><CODE>telephone</CODE> (telephone number -- identical to cis,
     * but blanks and dashes are ignored during comparisons)
     * <LI><CODE>dn</CODE> (distinguished name)
     * </UL>
     */
    public int getSyntax() {
        return syntax;
    }

    /**
     * Determines if the attribute type is single-valued.
     * @return <code>true</code> if single-valued,
     * <code>false</code> if multi-valued.
     */
    public boolean isSingleValued() {
        return single;
    }

    protected String internalSyntaxToString() {
        String s;
        if ( syntax == cis ) {
            s = cisString;
        } else if ( syntax == binary ) {
            s = binaryString;
        } else if ( syntax == ces ) {
            s = cesString;
        } else if ( syntax == telephone ) {
            s = telephoneString;
        } else if ( syntax == dn ) {
            s = dnString;
        } else if ( syntax == integer ) {
            s = intString;
        } else {
            s = "unknown";
        }
        return s;
    }

    /**
     * Get the attribute type definition in a string representation
     * of the AttributeTypeDescription data type defined in X.501 (see
     * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol
     * (v3): Attribute Syntax Definitions</A>
     * for a description of this format).
     * This is the format that LDAP servers and clients use to exchange
     * schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of the "attributetypes" attribute are attribute type
     * descriptions in this format.)
     * <P>
     *
     * @return A string in a format that can be used as the value of
     * the <CODE>attributetypes</CODE> attribute (which describes
     * an attribute type in the schema) of a <CODE>subschema</CODE> object.
     */
    public String getValue() {
        String s = "( " + oid + " NAME \'" + name + "\' DESC \'" +
            description + "\' SYNTAX \'";
        s += internalSyntaxToString();
        s += "\' ";
        if ( single )
            s += "SINGLE-VALUE ";
        s += ')';
        return s;
    }

    protected String syntaxToString() {
        String s;
        if ( syntax == cis ) {
            s = "case-insensitive string";
        } else if ( syntax == binary ) {
            s = "binary";
        } else if ( syntax == integer ) {
            s = "integer";
        } else if ( syntax == ces ) {
            s = "case-exact string";
        } else if ( syntax == telephone ) {
            s = "telephone";
        } else if ( syntax == dn ) {
            s = "distinguished name";
        } else {
            s = "unknown";
        }
        return s;
    }

    /**
     * Get the definition of the attribute type in a user friendly format.
     * This is the format that the attribute type definition uses when
     * you print the attribute type or the schema.
     * @return Definition of the attribute type in a user friendly format.
     */
    public String toString() {
        String s = "Name: " + name + "; OID: " + oid + "; Type: ";
        s += syntaxToString();
        s += "; Description: " + description + "; ";
        if ( single )
            s += "single-valued";
        else
            s += "multi-valued";
        return s;
    }

    protected int syntaxCheck( String syntax ) {
        int i = unknown;
        if ( syntax.equals( cisString ) ) {
            i = cis;
        } else if ( syntax.equals( binaryString ) ) {
            i = binary;
        } else if ( syntax.equals( cesString ) ) {
            i = ces;
        } else if ( syntax.equals( intString ) ) {
            i = integer;
        } else if ( syntax.equals( telephoneString ) ) {
            i = telephone;
        } else if ( syntax.equals( dnString ) ) {
            i = dn;
        }
        return i;
    }

    protected int syntax = unknown;
    private boolean single = false;
}
