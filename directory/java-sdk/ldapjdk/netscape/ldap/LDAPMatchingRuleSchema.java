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
 * The definition of a matching rule in the schema.
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of a matching rule.
 * According to the RFC, the description of a matching rule can
 * include the following information:
 * <P>
 *
 * <UL>
 * <LI>an OID identifying the matching rule
 * <LI>a name identifying the matching rule
 * <LI>a description of the matching rule
 * <LI>the syntax of the matching rule
 * </UL>
 * <P>
 *
 * The <CODE>LDAPMatchingRuleSchema</CODE> class also specifies
 * the matching rule "use description", which describes the
 * attributes which can be used with the matching rule.
 * <P>
 *
 * When you construct an <CODE>LDAPMatchingRuleSchema</CODE> object, you can
 * specify these types of information as arguments to the constructor or
 * in the MatchingRuleDescription and MatchingRuleUseDescription formats
 * specified in RFC 2252.
 * (When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.)
 * <P>
 *
 * You can get the name, OID, and description of this matching rule
 * definition by using the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
 * <CODE>getDescription</CODE> methods inherited from the abstract class
 * <CODE>LDAPSchemaElement</CODE>.
 * <P>
 *
 * If you need to add or remove this matching rule definition from the
 * schema, you can use the <CODE>add</CODE> and <CODE>remove</CODE>
 * methods, which this class inherits from the <CODE>LDAPSchemaElement</CODE>
 * abstract class.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPSchemaElement
 **/

public class LDAPMatchingRuleSchema extends LDAPAttributeSchema {
    /**
     * Construct a matching rule definition, using the specified
     * information.
     * @param name Name of the matching rule.
     * @param oid Object identifier (OID) of the matching rule
     * in dotted-string format (for example, "1.2.3.4").
     * @param description Description of the matching rule.
     * @param attributes Array of the OIDs of the attributes for which
     * the matching rule is applicable.
     * @param syntax Syntax of this matching rule. The value of this
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
     */
    public LDAPMatchingRuleSchema( String name, String oid, String description,
                            String[] attributes, int syntax ) {
        super( name, oid, description, syntax, true );
        attrName = "matchingrules";
        this.attributes = new String[attributes.length];
        for( int i = 0; i < attributes.length; i++ )
            this.attributes[i] = new String( attributes[i] );
    }

    /**
     * Constructs a matching rule definition based on descriptions in
     * the MatchingRuleDescription format and MatchingRuleUseDescription
     * format. For information on this format,
     * (see <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
     * Attribute Syntax Definitions</A>.  This is the format that LDAP servers
     * and clients use to exchange schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with attributes that include "matchingrule" and "matchingruleuse".
     * The values of these attributes are matching rule descriptions
     * in this format.)
     * <P>
     *
     * @param raw Definition of the matching rule in the
     * MatchingRuleDescription format.
     * @param use Definition of the use of the matching rule in the
     * MatchingRuleUseDescription format.
     */
    public LDAPMatchingRuleSchema( String raw, String use ) {
        super();
        attrName = "matchingrules";
        oid = processRule( raw );
        String o = processRule( use );
        if ( (o != null) && (oid != null) && !o.equals(oid) ) {
            System.err.println( "Rule oid " + oid +
                                " differs from use oid " + o );
        }
        if ( oid == null )
            oid = o;
    }

    /**
     * Construct a rule definition from a value returned from
     * a search of the schema in a Directory.
     * @param raw The raw string value returned as one of the values
     * of <code>matchingRuleUse</code> in searching the schema.
     */
    private String processRule( String raw ) {
        if ( raw == null )
            return null;
        /* Process the matchingRuleUse value */
        raw.trim();
        // Skip past "( " and ")"
        int l = raw.length();
        raw = raw.substring( 2, l - 1 );
        l = raw.length();
        int ind = raw.indexOf( ' ' );
        String o = raw.substring( 0, ind );
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
            } else {
                ind = l;
            }

            while( (ind < l) && (ch[ind] != '\'') && (ch[ind] != '(')  )
                ind++;
            last = ind + 1;
            while( (last < l) && (ch[last] != '\'') && (ch[last] != ')') ) {
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
                } else if ( s.equalsIgnoreCase( "APPLIES" ) ) {
                    /* Parse the list of attribute OIDs */
                    Vector v = new Vector();
                    StringTokenizer st = new StringTokenizer( val, " " );
                    while ( st.hasMoreTokens() ) {
                        String tok = st.nextToken();
                        if ( !tok.equals( "$" ) ) {
                            v.addElement( tok );
                        }
                    }
                    attributes = new String[v.size()];
                    v.copyInto( attributes );
                    v.removeAllElements();
                    v = null;
                }
            }
        }
        return o;
    }

    /**
     * Get the list of the OIDs of the attribute types which can be used
     * with the matching rule. The list is a deep copy.
     * @return Array of the OIDs of the attribute types which can be used
     * with the matching rule.
     */
    public String[] getAttributes() {
        return attributes;
    }

    /**
     * Get the matching rule definition in the string representation
     * of the MatchingRuleDescription data type defined in X.501 (see
     * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol
     * (v3): Attribute Syntax Definitions</A>
     * for a description of these formats).
     * This is the format that LDAP servers and clients use to exchange
     * schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "matchingrules" and "matchingruleuse".  The
     * values of these attributes are matching rule description and
     * matching rule use description in these formats.)
     * <P>
     *
     * @return A string in a format that can be used as the value of
     * the <CODE>matchingrule</CODE> attribute (which describes
     * a matching rule in the schema) of a <CODE>subschema</CODE> object.
     */
    public String getValue() {
        String s = "( " + oid + " NAME \'" + name + "\' DESC \'" +
            description + "\' SYNTAX \'";
        s += internalSyntaxToString();
        s += "\' )";
        return s;
    }

    /**
     * Get the matching rule use definition in the string representation
     * of the MatchingRuleUseDescription data type defined in X.501 (see
     * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol
     * (v3): Attribute Syntax Definitions</A>
     * for a description of these formats).
     * This is the format that LDAP servers and clients use to exchange
     * schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "matchingrules" and "matchingruleuse".  The
     * values of these attributes are matching rule description and
     * matching rule use description in these formats.)
     * <P>
     *
     * @return A string in a format that can be used as the value of
     * the <CODE>matchingruleuse</CODE> attribute (which describes the use of
     * a matching rule in the schema) of a <CODE>subschema</CODE> object.
     */
    public String getUseValue() {
        String s = "( " + oid;
        s += " APPLIES ( ";
        for( int i = 0; i < attributes.length; i++ ) {
            if ( i > 0 )
                s += " $ ";
            s += attributes[i];
        }
        s += ") )";
        return s;
    }

    /**
     * Add, remove or modify the definition from a Directory.
     * @param ld An open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op Type of modification to make.
     * @param name Name of attribute in the schema entry to modify. This
     * is ignored here.
     * @exception LDAPException if the definition can't be added/removed.
     */
    protected void update( LDAPConnection ld, int op, String name )
                            throws LDAPException {
        LDAPAttribute[] attrs = new LDAPAttribute[2];
        attrs[0] = new LDAPAttribute( "matchingRules",
                                      getValue() );
        /* Must update the matchingRuleUse value as well */
        attrs[1] = new LDAPAttribute( "matchingRuleUse",
                                      getUseValue() );
        update( ld, op, attrs );
    }

    /**
     * Get the definition of the matching rule in a user friendly format.
     * This is the format that the matching rule definition uses when
     * you print the matching rule or the schema.
     * @return Definition of the matching rule in a user friendly format.
     */
    public String toString() {
        String s = "Name: " + name + "; OID: " + oid + "; Type: ";
        s += syntaxToString();
        s += "; Description: " + description;
        if ( attributes != null ) {
            s += "; Applies to: ";
            for( int i = 0; i < attributes.length; i++ ) {
                if ( i > 0 )
                    s += ", ";
                s += attributes[i];
            }
        }
        return s;
    }

    private String[] attributes = null;
}
