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
        this.syntaxString = internalSyntaxToString();
        this.single = single;
    }

    /**
     * Constructs an attribute type definition, using the specified
     * information.
     * @param name Name of the attribute type.
     * @param oid Object identifier (OID) of the attribute type
     * in dotted-string format (for example, "1.2.3.4").
     * @param description Description of attribute type.
     * @param syntaxString Syntax of this attribute type in dotted-string
     * format (for example, "1.2.3.4.5").
     * @param single <CODE>true</CODE> if the attribute type is single-valued.
     */
    public LDAPAttributeSchema( String name, String oid, String description,
                            String syntaxString, boolean single ) {
        super( name, oid, description );
        attrName = "attributetypes";
        this.syntaxString = syntaxString;
        this.syntax = syntaxCheck( syntaxString );
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
        parseValue( raw );
        String val = (String)properties.get( SYNTAX );
        if ( val != null ) {
            syntaxString = val;
            syntax = syntaxCheck( val );
        }
        single = properties.containsKey( "SINGLE-VALUE" );
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
     * <LI><CODE>unknown</CODE> (not a known syntax)
     * </UL>
     */
    public int getSyntax() {
        return syntax;
    }

    /**
     * Gets the syntax of the attribute type in dotted-decimal format,
     * for example "1.2.3.4.5"
     * @return the attribute syntax in dotted-decimal format.
     */
    public String getSyntaxString() {
        return syntaxString;
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
            s = syntaxString;
        }
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
            s = syntaxString;
        }
        return s;
    }

    /**
     * Prepare a value in RFC 2252 format for submitting to a server
     *
     * @param quotingBug <CODE>true</CODE> if SUP and SYNTAX values are to
     * be quoted. That is to satisfy bugs in certain LDAP servers.
     * @return A String ready to be submitted to an LDAP server
     */
	String getValue( boolean quotingBug ) {
        String s = getValuePrefix();
        s += getValue( SUPERIOR, false );
        String val = getOptionalValues( MATCHING_RULES );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        s += getValue( SYNTAX, quotingBug );
        s += ' ';
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
     * Get the definition of the attribute type in a user friendly format.
     * This is the format that the attribute type definition uses when
     * you print the attribute type or the schema.
     * @return Definition of the attribute type in a user friendly format.
     */
    public String toString() {
        String s = "Name: " + name + "; OID: " + oid + "; Type: ";
        s += syntaxToString();
        s += "; Description: " + description + "; ";
        if ( single ) {
            s += "single-valued";
        } else {
            s += "multi-valued";
        }
        s += getQualifierString( EXPLICIT );
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

    /**
     * Parses an attribute schema definition to see if the SYNTAX value
     * is quoted. It shouldn't be (according to RFC 2252), but it is for
     * some LDAP servers. It will either be:<BR>
     * <CODE>SYNTAX 1.3.6.1.4.1.1466.115.121.1.15</CODE> or<BR>
     * <CODE>SYNTAX '1.3.6.1.4.1.1466.115.121.1.15'</CODE>
     * @param raw Definition of the attribute type in the
     * AttributeTypeDescription format.
     */
    static boolean isSyntaxQuoted( String raw ) {
        int ind = raw.indexOf( " SYNTAX " );
        if ( ind >= 0 ) {
            ind += 8;
            int l = raw.length() - ind;
            // Extract characters
            char[] ch = new char[l];
            raw.getChars( ind, raw.length(), ch, 0 );
            ind = 0;
            // Skip to ' or start of syntax value
            while( (ind < ch.length) && (ch[ind] == ' ') ) {
                ind++;
            }
            if ( ind < ch.length ) {
                return ( ch[ind] == '\'' );
            }
        }
        return false;
    }

    // Predefined qualifiers
	public static final String EQUALITY = "EQUALITY";
	public static final String ORDERING = "ORDERING";
	public static final String SUBSTR = "SUBSTR";
	public static final String SINGLE = "SINGLE-VALUE";
	public static final String COLLECTIVE = "COLLECTIVE";
	public static final String NO_USER_MODIFICATION = "NO-USER-MODIFICATION";
	public static final String USAGE = "USAGE";
	public static final String SYNTAX = "SYNTAX";
	
    protected int syntax = unknown;
    protected String syntaxString = null;
    protected boolean single = false;

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
    // Qualifiers tracked explicitly
	static final String[] EXPLICIT = { SINGLE,
                                       OBSOLETE,
                                       COLLECTIVE,
                                       NO_USER_MODIFICATION,
                                       SYNTAX};
}
