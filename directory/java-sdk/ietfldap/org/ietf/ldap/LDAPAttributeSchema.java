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
 * The definition of an attribute type in the schema.
 * <A HREF="http://www.ietf.org/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * to specify when defining an attribute type. According to the RFC, 
 * the description of an attribute type can include the following:
 * <P>
 *
 * <UL>
 * <LI>an OID identifying the attribute type
 * <LI>a name identifying the attribute type
 * <LI>a description of the attribute type
 * <LI>the name of the parent attribute type
 * <LI>the syntax used by the attribute (for example,
 * <CODE>cis</CODE> or <CODE>int</CODE>)
 * <LI>an indication of whether the attribute type is single-valued
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
 *
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
 * To get the name, OID, and description of this attribute type
 * definition, use the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
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
 * @see org.ietf.ldap.LDAPSchemaElement
 **/

public class LDAPAttributeSchema extends LDAPSchemaElement {

    static final long serialVersionUID = 2482595821879862595L;

    /**
     * Constructs a blank element.
     */
    protected LDAPAttributeSchema() {
        super();
    }

    /**
     * Constructs an attribute type definition, using the specified
     * information
     * @param names names of the attribute type
     * @param oid object identifier (OID) of the attribute type
     * in dotted-string format (for example, "1.2.3.4")
     * @param description description of attribute type
     * @param syntaxString syntax of this attribute type in dotted-string
     * format (for example, "1.2.3.4.5")
     * @param single <CODE>true</CODE> if the attribute type is single-valued
     * @param superior superior attribute as a name or OID; <CODE>null</CODE>
     * if there is no superior
     * @param obsolete true if this attribute is obsolete
     * @param equality Object Identifier of the equality matching rule
     * for the attribute in dotted-decimal format; MAY be null
     * @param ordering Object Identifier of the ordering matching rule
     * for the attribute in dotted-decimal format; MAY be null
     * @param substring Object Identifier of the substring matching rule 
     * for the attribute in dotted-decimal format; MAY be null
     * @param collective true if this is a collective attribute. 
     * @param userMod true if the attribute is modifiable by users. 
     * @param usage One of the following: 
     * <UL>
     * <LI>USER_APPLICATIONS
     * <LI>DIRECTORY_OPERATION 
     * <LI>DISTRIBUTED_OPERATION 
     * <LI>DSA_OPERATION 
     * </UL>
     */
    public LDAPAttributeSchema( String[] names,
                                String oid,
                                String description,
                                String syntaxString,
                                boolean single,
                                String superior,
                                boolean obsolete,
                                String equality,
                                String ordering,
                                String substring,
                                String collective,
                                boolean userMod,
                                int usage ) {
        super( names, oid, description );
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
        if ( obsolete ) {
            setQualifier( OBSOLETE, "" );
        }
        if ( (equality != null) && (equality.length() > 0) ) {
            setQualifier( EQUALITY, equality );
        }
        if ( (ordering != null) && (ordering.length() > 0) ) {
            setQualifier( ORDERING, ordering );
        }
        if ( (substring != null) && (substring.length() > 0) ) {
            setQualifier( SUBSTR, substring );
        }
        if ( (collective != null) && (collective.length() > 0) ) {
            setQualifier( COLLECTIVE, collective );
        }
        if ( !userMod ) {
            setQualifier( NO_USER_MODIFICATION, "" );
        }
        setQualifier( USAGE, String.valueOf( usage ) );
    }

    /**
     * Constructs an attribute type definition based on a description in
     * the AttributeTypeDescription format. For information on this format,
     * (see <A HREF="http://www.ietf.org/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
     * Attribute Syntax Definitions</A>.  This is the format that LDAP servers
     * and clients use to exchange schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of "attributetypes" are attribute type descriptions
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
     * Returns the value of a property of the object or null if there is none
     *
     * @param key the name of the property
     * @return the value of a property of the object or null if there is none
     */
     private String getStringProperty( String key ) {
        String[] val = getQualifier( key );
        return ((val != null) && (val.length > 0)) ? val[0] : null;
     }

    /**
     * Returns the Object Identifier of the equality matching rule in effect 
     * for this attribute, or null if there is none
     * @return the Object Identifier of the equality matching rule in effect 
     * for this attribute, or null if there is none
     */
     public String getEqualityMatchingRule() {
         return getStringProperty( EQUALITY );
     }

    /**
     * Returns the Object Identifier of the order matching rule in effect 
     * for this attribute, or null if there is none
     * @return the Object Identifier of the order matching rule in effect 
     * for this attribute, or null if there is none
     */
     public String getOrderMatchingRule() {
         return getStringProperty( ORDERING );
     }

    /**
     * Returns the Object Identifier of the substring matching rule in effect 
     * for this attribute, or null if there is none
     * @return the Object Identifier of the substring matching rule in effect 
     * for this attribute, or null if there is none
     */
     public String getSubstringMatchingRule() {
         return getStringProperty( SUBSTR );
     }

    /**
     * Gets the name of the attribute that this attribute inherits from,
     * if any.
     * @return the name of the attribute from which this attribute
     * inherits, or <CODE>null</CODE> if it does not have a superior.
     */
    public String getSuperior() {
        return getStringProperty( SUPERIOR );
    }

    /**
     * Gets the syntax of the schema element
     * @return one of the following values:
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
     * @deprecated use getSyntaxString instead
     */
    public int getSyntax() {
        return syntaxElement.syntax;
    }

    /**
     * Gets the usage property of the object
     *
     * @return the usage property of the object; one of the following:
     * <UL>
     * <LI>USER_APPLICATIONS
     * <LI>DIRECTORY_OPERATION 
     * <LI>DISTRIBUTED_OPERATION 
     * <LI>DSA_OPERATION 
     * </UL>
     */
    public int getUsage() {
        int usage;
        try {
            usage = Integer.parseInt( getStringProperty( USAGE ) );
        } catch ( Exception e ) {
            usage = USER_APPLICATIONS;
        }
        return usage;
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
     * Prepares a value in RFC 2252 format for submission to a server
     *
     * @param quotingBug <CODE>true</CODE> if SUP and SYNTAX values are to
     * be quoted. This is required to work with bugs in certain LDAP servers.
     * @return a String ready for submission to an LDAP server.
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
     * Determines if the attribute type is collective
     *
     * @return <code>true</code> if collective
     */
    public boolean isCollective() {
        return (properties != null) ? properties.containsKey( COLLECTIVE ) :
            false;
    }

    /**
     * Determines if the attribute type is single-valued
     *
     * @return <code>true</code> if single-valued,
     * <code>false</code> if multi-valued.
     */
    public boolean isSingleValued() {
        return (properties != null) ? properties.containsKey( SINGLE ) :
            false;
    }

    /**
     * Determines if the attribute type is modifiable by users
     *
     * @return <code>true</code> if modifiable by users
     */
    public boolean isUserModifiable() {
        return (properties != null) ?
            !properties.containsKey( NO_USER_MODIFICATION ) :
            true;
    }

    /**
     * Gets the definition of the attribute type in Directory format
     *
     * @return definition of the attribute type in Directory format
     */
    public String toString() {
        return getValue( false );
    }

    /**
     * Gets the attribute name for a schema element
     *
     * @return The attribute name of the element
     */
    String getAttributeName() {
        return "attributetypes";
    }

    // Predefined qualifiers
    public static final String EQUALITY = "EQUALITY";
    public static final String ORDERING = "ORDERING";
    public static final String SUBSTR = "SUBSTR";
    public static final String SINGLE = "SINGLE-VALUE";
    public static final String COLLECTIVE = "COLLECTIVE";
    public static final String NO_USER_MODIFICATION = "NO-USER-MODIFICATION";
    public static final String USAGE = "USAGE";

    // Usage constants
    /** An ordinary user attribute 
     */
    public static final int USER_APPLICATIONS = 0;
       
    /** An operational attribute used for a directory operation or which
     * holds a directory specific value 
     */
    public static final int DIRECTORY_OPERATION = 1;
        
    /** An operational attribute used to hold server (DSA) information that
     * is shared among servers holding replicas of the entry 
     */
    public static final int DISTRIBUTED_OPERATION = 2;
      
    /** An operational attribute used to hold server (DSA) information that
     * is local to a server     
     */
    public static final int DSA_OPERATION = 3;

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

    protected LDAPSyntaxSchemaElement syntaxElement =
        new LDAPSyntaxSchemaElement();
}
