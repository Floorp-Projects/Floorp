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
 * When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.
 * <P>

 * There a number of additional optional description fields which
 * are not explicitly accessible through LDAPAttributeSchema, but which
 * can be managed with setQualifier, getQualifier, and getQualifierNames:
 * <P>
 *
 * <UL>
 * <LI>EQUALITY
 * <LI>ORDERING
 * <LI>SUBSTR
 * <LI>COLLECTIVE
 * <LI>NO-USER-MODIFICATION
 * <LI>USAGE
 * <LI>OBSOLETE
 * </UL>
 * <P>
 *
 * You can get the name, OID, and description of this attribute type
 * definition by using the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
 * <CODE>getDescription</CODE> methods inherited from the abstract class
 * <CODE>LDAPSchemaElement</CODE>. Optional and custom qualifiers are
 * accessed with <CODE>getQualifier</CODE> and <CODE>getQualifierNames</CODE>
 * from <CODE>LDAPSchemaElement</CODE>.
 * <P>
 *
 * To add or remove this attribute type definition from the
 * schema, use the <CODE>add</CODE> and <CODE>remove</CODE>
 * methods, which this class inherits from the <CODE>LDAPSchemaElement</CODE>
 * abstract class.
 * <P>
 * RFC 2252 defines AttributeTypeDescription as follows:
 * <P>
 * <PRE>
 *     AttributeTypeDescription = "(" whsp
 *          numericoid whsp              ; AttributeType identifier
 *        [ "NAME" qdescrs ]             ; name used in AttributeType
 *        [ "DESC" qdstring ]            ; description
 *        [ "OBSOLETE" whsp ]
 *        [ "SUP" woid ]                 ; derived from this other
 *                                       ; AttributeType
 *        [ "EQUALITY" woid              ; Matching Rule name
 *        [ "ORDERING" woid              ; Matching Rule name
 *        [ "SUBSTR" woid ]              ; Matching Rule name
 *        [ "SYNTAX" whsp noidlen whsp ] ; see section 4.3
 *        [ "SINGLE-VALUE" whsp ]        ; default multi-valued
 *        [ "COLLECTIVE" whsp ]          ; default not collective
 *        [ "NO-USER-MODIFICATION" whsp ]; default user modifiable
 *        [ "USAGE" whsp AttributeUsage ]; default userApplications
 *        whsp ")"
 *
 *    AttributeUsage =
 *        "userApplications"     /
 *        "directoryOperation"   /
 *        "distributedOperation" / ; DSA-shared
 *        "dSAOperation"          ; DSA-specific, value depends on server
 * </PRE>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPSchemaElement
 **/

public class LDAPAttributeSchema extends LDAPSchemaElement {
    /**
     * Constructs a blank element.
     */
    protected LDAPAttributeSchema() {
        super();
    }

    /**
     * Constructs an attribute type definition, using the specified
     * information.
     * @param name name of the attribute type
     * @param oid object identifier (OID) of the attribute type
     * in dotted-string format (for example, "1.2.3.4")
     * @param description description of attribute type
     * @param syntax syntax of this attribute type. The value of this
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
     * @param single <CODE>true</CODE> if the attribute type is single-valued
     */
    public LDAPAttributeSchema( String name, String oid, String description,
                                int syntax, boolean single ) {
        this( name, oid, description, cisString, single );
        syntaxElement.syntax = syntax;
        String syntaxType = syntaxElement.internalSyntaxToString( syntax );
        if ( syntaxType != null ) {
            syntaxElement.syntaxString = syntaxType;
        }
        setQualifier( SYNTAX, getSyntaxString() );
    }

    /**
     * Constructs an attribute type definition, using the specified
     * information.
     * @param name name of the attribute type
     * @param oid object identifier (OID) of the attribute type
     * in dotted-string format (for example, "1.2.3.4")
     * @param description description of attribute type
     * @param syntaxString syntax of this attribute type in dotted-string
     * format (for example, "1.2.3.4.5")
     * @param single <CODE>true</CODE> if the attribute type is single-valued
     */
    public LDAPAttributeSchema( String name, String oid, String description,
                                String syntaxString, boolean single ) {
        this( name, oid, description, syntaxString, single, null, null );
    }

    /**
     * Constructs an attribute type definition, using the specified
     * information.
     * @param name name of the attribute type
     * @param oid object identifier (OID) of the attribute type
     * in dotted-string format (for example, "1.2.3.4")
     * @param description description of attribute type
     * @param syntaxString syntax of this attribute type in dotted-string
     * format (for example, "1.2.3.4.5")
     * @param single <CODE>true</CODE> if the attribute type is single-valued
     * @param superior superior attribute as a name or OID; <CODE>null</CODE>
     * if there is no superior
     * @param aliases names which are to be considered aliases for this
     * attribute; <CODE>null</CODE> if there are no aliases
     */
    public LDAPAttributeSchema( String name, String oid, String description,
                                String syntaxString, boolean single,
                                String superior, String[] aliases ) {
        super( name, oid, description );
        attrName = "attributetypes";
        syntaxElement.syntax = syntaxElement.syntaxCheck( syntaxString );
        syntaxElement.syntaxString = syntaxString;
        setQualifier( SYNTAX, syntaxElement.syntaxString );
        if ( single ) {
            setQualifier( SINGLE, "" );
        }
        if ( (superior != null) && (superior.length() > 0) ) {
            setQualifier( SUPERIOR, superior );
        }
        if ( (aliases != null) && (aliases.length > 0) ) {
            this.aliases = aliases;
        }
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
     * @param raw definition of the attribute type in the
     * AttributeTypeDescription format
     */
    public LDAPAttributeSchema( String raw ) {
        attrName = "attributetypes";
        parseValue( raw );
        String val = (String)properties.get( SYNTAX );
        if ( val != null ) {
            syntaxElement.syntaxString = val;
            syntaxElement.syntax = syntaxElement.syntaxCheck( val );
        }
    }

    /**
     * Determines if the attribute type is single-valued.
     * @return <code>true</code> if single-valued,
     * <code>false</code> if multi-valued
     */
    public boolean isSingleValued() {
        return (properties != null) ? properties.containsKey( SINGLE ) :
            false;
    }

    /**
     * Gets the name of the attribute that this attribute inherits from,
     * if any.
     * @return the name of the attribute that this attribute
     * inherits from, or <CODE>null</CODE> if it does not have a superior
     */
    public String getSuperior() {
        String[] val = getQualifier( SUPERIOR );
        return ((val != null) && (val.length > 0)) ? val[0] : null;
    }

    /**
     * Gets the syntax of the schema element
     * @return One of the following values:
     * <UL>
     * <LI><CODE>cis</CODE> (case-insensitive string)
     * <LI><CODE>ces</CODE> (case-exact string)
     * <LI><CODE>binary</CODE> (binary data)
     * <LI><CODE>int</CODE> (integer)
     * <LI><CODE>telephone</CODE> (telephone number -- identical to cis,
     * but blanks and dashes are ignored during comparisons)
     * <LI><CODE>dn</CODE> (distinguished name)
     * <LI><CODE>unknown</CODE> (not a known syntax)
     * </UL>
     */
    public int getSyntax() {
        return syntaxElement.syntax;
    }

    /**
     * Gets the syntax of the attribute type in dotted-decimal format,
     * for example "1.2.3.4.5"
     * @return The attribute syntax in dotted-decimal format.
     */
    public String getSyntaxString() {
        return syntaxElement.syntaxString;
    }

    /**
     * Prepare a value in RFC 2252 format for submitting to a server
     *
     * @param quotingBug <CODE>true</CODE> if SUP and SYNTAX values are to
     * be quoted; that is to satisfy bugs in certain LDAP servers.
     * @return a String ready to be submitted to an LDAP server
     */
    String getValue( boolean quotingBug ) {
        String s = getValuePrefix();
        String val = getValue( SUPERIOR, false );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        val = getOptionalValues( MATCHING_RULES );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        val = getValue( SYNTAX, quotingBug );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        if ( isSingleValued() ) {
            s += SINGLE + ' ';
        }
        val = getOptionalValues( NOVALS );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        val = getOptionalValues( new String[] {USAGE} );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        val = getCustomValues();
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        s += ')';
        return s;
    }

    /**
     * Gets the definition of the attribute type in a user friendly format.
     * This is the format that the attribute type definition uses when
     * you print the attribute type or the schema.
     * @return definition of the attribute type in a user friendly format
     */
    public String toString() {
        String s = "Name: " + name + "; OID: " + oid + "; Type: ";
        s += syntaxElement.syntaxToString();
        s += "; Description: " + description + "; ";
        if ( isSingleValued() ) {
            s += "single-valued";
        } else {
            s += "multi-valued";
        }
        s += getQualifierString( IGNOREVALS );
        s += getAliasString();
        return s;
    }

    // Predefined qualifiers
    public static final String EQUALITY = "EQUALITY";
    public static final String ORDERING = "ORDERING";
    public static final String SUBSTR = "SUBSTR";
    public static final String SINGLE = "SINGLE-VALUE";
    public static final String COLLECTIVE = "COLLECTIVE";
    public static final String NO_USER_MODIFICATION = "NO-USER-MODIFICATION";
    public static final String USAGE = "USAGE";
    
    // Qualifiers known to not have values; prepare a Hashtable
    static String[] NOVALS = { SINGLE,
                               COLLECTIVE,
                               NO_USER_MODIFICATION };
    static {
        for( int i = 0; i < NOVALS.length; i++ ) {
            novalsTable.put( NOVALS[i], NOVALS[i] );
        }
    }
    static final String[] MATCHING_RULES = { EQUALITY,
                                             ORDERING,
                                             SUBSTR };
    // Qualifiers which we output explicitly in toString()
    static final String[] IGNOREVALS = { SINGLE,
                                         OBSOLETE,
                                         SUPERIOR,
                                         SINGLE,
                                         COLLECTIVE,
                                         NO_USER_MODIFICATION,
                                         SYNTAX};

    private LDAPSyntaxSchemaElement syntaxElement =
        new LDAPSyntaxSchemaElement();
}
