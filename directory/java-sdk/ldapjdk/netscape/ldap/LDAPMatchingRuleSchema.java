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
 *   	Ingo Schaefer (ingo.schaefer@fh-brandenburg.de)
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
 * When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.
 * <P>
 *
 * You can get the name, OID, and description of this matching rule
 * definition by using the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
 * <CODE>getDescription</CODE> methods inherited from the abstract class
 * <CODE>LDAPSchemaElement</CODE>. Custom qualifiers are
 * accessed with <CODE>getQualifier</CODE> and <CODE>getQualifierNames</CODE>
 * from <CODE>LDAPSchemaElement</CODE>.
  * <P>
 *
 * To add or remove this matching rule definition from the
 * schema, use the <CODE>add</CODE> and <CODE>remove</CODE>
 * methods, which this class inherits from the <CODE>LDAPSchemaElement</CODE>
 * abstract class.
 * <P>
 * RFC 2252 defines MatchingRuleDescription and MatchingRuleUseDescription
 * as follows:
 * <P>
 * <PRE>
 *    MatchingRuleDescription = "(" whsp
 *        numericoid whsp  ; MatchingRule identifier
 *        [ "NAME" qdescrs ]
 *        [ "DESC" qdstring ]
 *        [ "OBSOLETE" whsp ]
 *        "SYNTAX" numericoid
 *    whsp ")"
 *
 * Values of the matchingRuleUse list the attributes which are suitable
 * for use with an extensible matching rule.
 *
 *    MatchingRuleUseDescription = "(" whsp
 *        numericoid whsp  ; MatchingRule identifier
 *        [ "NAME" qdescrs ]
 *        [ "DESC" qdstring ]
 *        [ "OBSOLETE" ]
 *       "APPLIES" oids    ; AttributeType identifiers
 *    whsp ")"
 * </PRE>
 * <P>
 * <CODE>LDAPMatchingRuleSchema</CODE> abstracts away from the two types and
 * manages their relationships transparently.
 *
 * @version 1.0
 * @see netscape.ldap.LDAPSchemaElement
 **/

public class LDAPMatchingRuleSchema extends LDAPAttributeSchema {

    static final long serialVersionUID = 6466155218986944131L;

    /**
     * Constructs a matching rule definition, using the specified
     * information.
     * @param name name of the matching rule
     * @param oid object identifier (OID) of the matching rule
     * in dotted-decimal format (for example, "1.2.3.4")
     * @param description description of the matching rule
     * @param attributes array of the OIDs of the attributes for which
     * the matching rule is applicable
     * @param syntax syntax of this matching rule. The value of this
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
    public LDAPMatchingRuleSchema( String name, String oid,
                                   String description,
                                   String[] attributes, int syntax ) {
        this( name, oid, description, attributes, cisString );
        syntaxElement.syntax = syntax;
        String syntaxType = syntaxElement.internalSyntaxToString( syntax );
        if ( syntaxType != null ) {
            syntaxElement.syntaxString = syntaxType;
        }
        setQualifier( SYNTAX, syntaxElement.syntaxString );
    }

    /**
     * Constructs a matching rule definition, using the specified
     * information.
     * @param name name of the matching rule.
     * @param oid object identifier (OID) of the matching rule
     * in dotted-decimal format (for example, "1.2.3.4").
     * @param description description of the matching rule.
     * @param attributes array of the OIDs of the attributes for which
     * the matching rule is applicable.
     * @param syntaxString syntax of this matching rule in dotted-decimal
     * format
     */
    public LDAPMatchingRuleSchema( String name, String oid,
                                   String description,
                                   String[] attributes,
                                   String syntaxString ) {
        this( name, oid, description, attributes, syntaxString, null );
    }

    /**
     * Constructs a matching rule definition, using the specified
     * information.
     * @param name name of the matching rule.
     * @param oid object identifier (OID) of the matching rule
     * in dotted-decimal format (for example, "1.2.3.4").
     * @param description description of the matching rule.
     * @param attributes array of the OIDs of the attributes for which
     * the matching rule is applicable.
     * @param syntaxString syntax of this matching rule in dotted-decimal
     * format
     * @param aliases names which are to be considered aliases for this
     * matching rule; <CODE>null</CODE> if there are no aliases
     */
    public LDAPMatchingRuleSchema( String name, String oid,
                                   String description,
                                   String[] attributes,
                                   String syntaxString,
                                   String[] aliases ) {
        if ( (oid == null) || (oid.trim().length() < 1) ) {
            throw new IllegalArgumentException( "OID required" );
        }
        this.name = name;
        this.oid = oid;
        this.description = description;
        attrName = "matchingrules";
        syntaxElement.syntax = syntaxElement.syntaxCheck( syntaxString );
        syntaxElement.syntaxString = syntaxString;
        setQualifier( SYNTAX, syntaxElement.syntaxString );
        this.attributes = new String[attributes.length];
        for( int i = 0; i < attributes.length; i++ ) {
            this.attributes[i] = attributes[i];
        }
        if ( (aliases != null) && (aliases.length > 0) ) {
            this.aliases = aliases;
        }
    }

    /**
     * Constructs a matching rule definition based on descriptions in
     * the MatchingRuleDescription format and MatchingRuleUseDescription
     * format. For information on this format,
     * (see <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
     * Attribute Syntax Definitions</A>. This is the format that LDAP servers
     * and clients use to exchange schema information. For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with attributes that include "matchingrule" and "matchingruleuse".
     * The values of these attributes are matching rule descriptions
     * in this format.
     * <P>
     *
     * @param raw definition of the matching rule in the
     * MatchingRuleDescription format
     * @param use definition of the use of the matching rule in the
     * MatchingRuleUseDescription format
     */
    public LDAPMatchingRuleSchema( String raw, String use ) {
        attrName = "matchingrules";
        if ( raw != null ) {
            parseValue( raw );
        }
        if ( use != null ) {
            parseValue( use );
        }
	Object p = properties.get( "APPLIES" );
	if ( p instanceof Vector ) {
		Vector v = (Vector) p;
		if ( v != null ) {
			attributes = new String[v.size()];
			v.copyInto( attributes );
			v.removeAllElements();
		}
        }
	else if ( p instanceof String ) {
		attributes = new String[1];
		attributes[0] = (String) p;
	}
        String val = (String)properties.get( "SYNTAX" );
        if ( val != null ) {
            syntaxElement.syntaxString = val;
            syntaxElement.syntax = syntaxElement.syntaxCheck( val );
        }
    }

    /**
     * Gets the list of the OIDs of the attribute types which can be used
     * with the matching rule.
     * @return array of the OIDs of the attribute types which can be used
     * with the matching rule.
     */
    public String[] getAttributes() {
        return attributes;
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
        if ( syntaxElement.syntaxString != null ) {
            s += "SYNTAX ";
            if ( quotingBug ) {
                s += '\'';
            }
            s += syntaxElement.syntaxString;
            if ( quotingBug ) {
                s += '\'';
            }
            s += ' ';
        }
        String val = getCustomValues();
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        s += ')';
        return s;
    }

    /**
     * Gets the matching rule definition in the string representation
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
     * @return a string in a format that can be used as the value of
     * the <CODE>matchingrule</CODE> attribute (which describes
     * a matching rule in the schema) of a <CODE>subschema</CODE> object
     */
    public String getValue() {
        return getValue( false );
    }

    /**
     * Gets the matching rule use definition in the string representation
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
     * @return a string in a format that can be used as the value of
     * the <CODE>matchingruleuse</CODE> attribute (which describes the use of
     * a matching rule in the schema) of a <CODE>subschema</CODE> object
     */
    public String getUseValue() {
        String s = getValuePrefix();
        if ( (attributes != null) && (attributes.length > 0) ) {
            s += "APPLIES ( ";
            for( int i = 0; i < attributes.length; i++ ) {
                if ( i > 0 )
                    s += " $ ";
                s += attributes[i];
            }
            s += " ) ";
        }
        s += ')';
        return s;
    }

    /**
     * Adds, removes or modifies the definition from a Directory.
     * @param ld an open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op type of modification to make
     * @param name name of attribute in the schema entry to modify. This
     * is ignored here.
     * @param dn the entry at which to update the schema
     * @exception LDAPException if the definition can't be added/removed
     */
    protected void update( LDAPConnection ld, int op, String name, String dn )
                            throws LDAPException {
        LDAPAttribute[] attrs = new LDAPAttribute[2];
        attrs[0] = new LDAPAttribute( "matchingRules",
                                      getValue() );
        /* Must update the matchingRuleUse value as well */
        attrs[1] = new LDAPAttribute( "matchingRuleUse",
                                      getUseValue() );
        update( ld, op, attrs, dn );
    }

    /**
     * Gets the definition of the matching rule in a user friendly format.
     * This is the format that the matching rule definition uses when
     * you print the matching rule or the schema.
     * @return definition of the matching rule in a user friendly format.
     */
    public String toString() {
        String s = "Name: " + name + "; OID: " + oid + "; Type: ";
        s += syntaxElement.syntaxToString();
        s += "; Description: " + description;
        if ( attributes != null ) {
            s += "; Applies to: ";
            for( int i = 0; i < attributes.length; i++ ) {
                if ( i > 0 )
                    s += ", ";
                s += attributes[i];
            }
        }
        s += getQualifierString( EXPLICIT );
        s += getAliasString();
        return s;
    }

    // Qualifiers tracked explicitly
    static final String[] EXPLICIT = { OBSOLETE,
                                       SYNTAX };
    
    private String[] attributes = null;
}
