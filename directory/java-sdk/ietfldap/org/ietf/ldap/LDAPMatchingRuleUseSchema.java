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
 * The definition of a matching rule use in the schema.
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of a matching rule use.
 * According to the RFC, the description of a matching rule use can
 * include the following information:
 * <P>
 *
 * <UL>
 * <LI>an OID identifying the matching rule
 * <LI>a name identifying the matching rule use
 * <LI>a description of the matching rule use
 * <LI>a list of attributes the matching rule applies to
 * </UL>
 * <P>
 *
 * When you construct an <CODE>LDAPMatchingRuleUseSchema</CODE> object, you can
 * specify these types of information as arguments to the constructor or
 * in the MatchingRuleUseDescription format specified in RFC 2252.
 * When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with values in this
 * format.
 * <P>
 *
 * You can get the name, OID, and description of this matching rule use
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
 * RFC 2252 defines MatchingRuleUseDescription follows:
 * <P>
 * <PRE>
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
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPSchemaElement
 * @see org.ietf.ldap.LDAPMatchingRuleSchema
 **/

public class LDAPMatchingRuleUseSchema extends LDAPAttributeSchema {



    /**
     * Constructs a matching rule use definition, using the specified
     * information.
     * @param names names of the matching rule
     * @param oid object identifier (OID) of the matching rule
     * in dotted-decimal format (for example, "1.2.3.4")
     * @param description description of the matching rule
     * @param obsolete <CODE>true</CODE> if the element is obsolete
     * @param attributes array of the OIDs of the attributes for which
     * the matching rule is applicable
     */
    public LDAPMatchingRuleUseSchema( String[] names,
                                      String oid,
                                      String description,
                                      boolean obsolete,
                                      String[] attributes ) {
        if ( (oid == null) || (oid.trim().length() < 1) ) {
            throw new IllegalArgumentException( "OID required" );
        }
        this.names = names;
        this.oid = oid;
        this.description = description;
        attrName = "matchingruleuse";
        _attributes = new String[attributes.length];
        for( int i = 0; i < attributes.length; i++ ) {
            _attributes[i] = attributes[i];
        }
        if ( obsolete ) {
            setQualifier( OBSOLETE, "" );
        }
    }

    /**
     * Constructs a matching rule use definition based on descriptions in
     * the MatchingRuleUseDescription format. For information on this format,
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
     * @param use definition of the use of the matching rule in the
     * MatchingRuleUseDescription format
     */
    public LDAPMatchingRuleUseSchema( String use ) {
        attrName = "matchingruleuse";
        parseValue( use );
        Vector v = (Vector)properties.get( "APPLIES" );
        if ( v != null ) {
            _attributes = new String[v.size()];
            v.copyInto( _attributes );
            v.removeAllElements();
        }
    }

    /**
     * Gets the list of the OIDs of the attribute types which can be used
     * with the matching rule.
     * @return array of the OIDs of the attribute types which can be used
     * with the matching rule.
     */
    public String[] getApplicableAttributes() {
        return _attributes;
    }

    /**
     * Gets the attribute name for a schema element
     *
     * @return The attribute name of the element
     */
    String getAttributeName() {
        return "matchingruleuse";
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
    public String getValue() {
        String s = getValuePrefix();
        if ( (_attributes != null) && (_attributes.length > 0) ) {
            s += "APPLIES ( ";
            for( int i = 0; i < _attributes.length; i++ ) {
                if ( i > 0 )
                    s += " $ ";
                s += _attributes[i];
            }
            s += " ) ";
        }
        s += ')';
        return s;
    }

    /**
     * Gets the definition of the matching rule use in Directory format
     *
     * @return definition of the matching rule use in Directory format
     */
    public String toString() {
        return getValue();
    }

    // Qualifiers tracked explicitly
    static final String[] EXPLICIT = { OBSOLETE };
    
    private String[] _attributes = null;
}
