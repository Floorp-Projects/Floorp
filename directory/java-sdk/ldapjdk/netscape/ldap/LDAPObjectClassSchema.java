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
 * (When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.)
 * <P>
 *
 * RFC 2252 also notes that you can specify whether or not an object class
 * is abstract, structural, or auxiliary in the object description.
 * (Abstract object classes are used only to derive other object classes.
 * Entries cannot belong to an abstract object class. <CODE>top</CODE>
 * is an abstract object class.)  Entries must belong to a structural
 * object class, so most object classes are structural object classes.
 * Objects of the <CODE>LDAPObjectClassSchema</CODE> class are structural
 * object classes by default. Auxiliary object classes can be used to
 * construct entries of different types. For example, an auxiliary object
 * class might be used to specify a type of entry with a structural object
 * class that can vary (an auxiliary object class might specify that an
 * entry must either be an organizationalPerson or residentialPerson).
 * If the definition of an object (in ObjectClassDescription format)
 * specifies the AUXILIARY keyword, an <CODE>LDAPObjectClassSchema</CODE>
 * object created from that description represents an auxiliary object class.
 * <P>
 *
 * You can get the name, OID, and description of this object class
 * definition by using the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
 * <CODE>getDescription</CODE> methods inherited from the abstract class
 * <CODE>LDAPSchemaElement</CODE>.
 * <P>
 *
 * If you need to add or remove this object class definition from the
 * schema, you can use the <CODE>add</CODE> and <CODE>remove</CODE>
 * methods, which this class inherits from the <CODE>LDAPSchemaElement</CODE>
 * abstract class.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPSchemaElement
 **/
public class LDAPObjectClassSchema extends LDAPSchemaElement {
    /**
     * Constructs an object class definition, using the specified
     * information.
     * @param name Name of the object class.
     * @param oid Object identifier (OID) of the object class
     * in dotted-string format (for example, "1.2.3.4").
     * @param description Description of the object class.
     * @param superior Name of the parent object class
     * (the object class that the new object class inherits from).
     * @param required Array of the names of the attributes required
     * in this object class.
     * @param optional Array of the names of the optional attributes
     * allowed in this object class.
     */
    public LDAPObjectClassSchema( String name, String oid, String superior,
                                  String description,
                                  String[] required, String optional[] ) {
        super( name, oid, description );
        attrName = "objectclasses";
        this.superior = superior;
        for( int i = 0; i < required.length; i++ )
            must.addElement( required[i] );
        for( int i = 0; i < optional.length; i++ )
            may.addElement( optional[i] );
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
     * @param raw Definition of the object in the ObjectClassDescription
     * format.
     */
    public LDAPObjectClassSchema( String raw ) {
        attrName = "objectclasses";
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
                if ( s.equalsIgnoreCase( "AUXILIARY" ) ) {
                    auxiliary = true;
                    continue;
                }
            } else {
                ind = l;
            }

            while( (ind < l) && (ch[ind] != '\'') && (ch[ind] != '(')  )
                ind++;
            last = ind + 1;
            if ( ind >= l ) {
                break;
            }
            if ( ch[ind] == '\'' ) {
                while( (last < l) && (ch[last] != '\'') ) {
                    last++;
                }
                if ( (ind < last) && (last < l) ) {
                    val = new String( ch, ind+1, last-ind-1 );

                    if ( s.equalsIgnoreCase( "NAME" ) ) {
                        name = val;
                    } else if ( s.equalsIgnoreCase( "DESC" ) ) {
                        description = val;
                    } else if ( s.equalsIgnoreCase( "SUP" ) ) {
                        superior = val;
                    }
                }
            } else {
                Vector v = null;
                if ( s.equalsIgnoreCase( "MAY" ) ) {
                    v = may;
                } else if ( s.equalsIgnoreCase( "MUST" ) ) {
                    v = must;
                } else {
                    continue;
                }
                while( (last < l) && (ch[last] != ')') ) {
                    last++;
                }
                if ( (ind < last) && (last < l) ) {
                    val = new String( ch, ind+1, last-ind-1 );
                    StringTokenizer st = new StringTokenizer( val, " " );
                    while ( st.hasMoreTokens() ) {
                        String tok = st.nextToken();
                        if ( !tok.equals( "$" ) ) {
                            v.addElement( tok );
                        }
                    }
                }
            }
            ind = last + 1;
        }
    }

    /**
     * Get the name of the object class that this class inherits from.
     * @return The name of the object class that this class
     * inherits from.
     */
    public String getSuperior() {
        return superior;
    }

    /**
     * Get an enumeration of the names of the required attribute for
     * this object class.
     * @return An enumeration of the names of the required attributes
     * for this object class.
     */
    public Enumeration getRequiredAttributes() {
        return must.elements();
    }

    /**
     * Get an enumeration of names of optional attributes allowed
     * in this object class.
     * @return An enumeration of the names of optional attributes
     * allowed in this object class.
     */
    public Enumeration getOptionalAttributes() {
        return may.elements();
    }

    /**
     * Get the object class definition in a string representation
     * of the ObjectClassDescription data type defined in X.501 (see
     * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
     * Attribute Syntax Definitions</A> for a description of this format).
     * This is the format that LDAP servers and clients use to exchange
     * schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of the "objectclasses" attribute are object class descriptions
     * in this format.)
     * <P>
     *
     * @return A string in a format that can be used as the value of
     * the <code>objectclasses</code> attribute (which describes
     * an object class in the schema) of a <CODE>subschema</CODE> object.
     */
    public String getValue() {
        String s = "( " + oid + " NAME \'" + name + "\' DESC \'" +
            description + "\' SUP \'" + superior + "\' ";
        if ( auxiliary )
            s += "AUXILIARY ";
        s += "MUST ( ";
        int i = 0;
        Enumeration e = getRequiredAttributes();
        while( e.hasMoreElements() ) {
            if ( i > 0 )
                s += " $ ";
            i++;
            s += (String)e.nextElement();
        }
        s += " ) MAY ( ";
        e = getOptionalAttributes();
        i = 0;
        while( e.hasMoreElements() ) {
            if ( i > 0 )
                s += " $ ";
            i++;
            s += (String)e.nextElement();
        }
        s += ") )";
        return s;
    }

    /**
     * Get the definition of the object class in a user friendly format.
     * This is the format that the object class definition uses when
     * you print the object class or the schema.
     * @return Definition of the object class in a user friendly format.
     */
    public String toString() {
        String s = "Name: " + name + "; OID: " + oid +
            "; Superior: " + superior +
            "; Description: " + description + "; Required: ";
        int i = 0;
        Enumeration e = getRequiredAttributes();
        while( e.hasMoreElements() ) {
            if ( i > 0 )
                s += ", ";
            i++;
            s += (String)e.nextElement();
        }
        s += "; Optional: ";
        e = getOptionalAttributes();
        i = 0;
        while( e.hasMoreElements() ) {
            if ( i > 0 )
                s += ", ";
            i++;
            s += (String)e.nextElement();
        }
        return s;
    }

    private String superior = "";
    private Vector must = new Vector();
    private Vector may = new Vector();
    private boolean auxiliary = false;
}
