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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.util.*;

/**
 * The definition of a DIT content rule in the schema.
 * <A HREF="http://www.ietf.org/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * DIT Content Rule Description</A> covers the types of information
 * to specify when defining a DIT content rule. According to the RFC, 
 * the description of a DIT content rule can include the following:
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
 * When you construct an <CODE>LDAPDITContentRuleSchema</CODE> object, you can
 * specify these types of information as arguments to the constructor or
 * in the AttributeTypeDescription format specified in RFC 2252.
 * When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.
 * <P>
 *
 * There are a number of additional optional description fields which
 * are not explicitly accessible through LDAPDITContentRuleSchema, but which
 * can be managed with setQualifier, getQualifier, and getQualifierNames:
 * <P>
 *
 * <UL>
 * <LI>OBSOLETE
 * </UL>
 * <P>
 *
 * To get the name, OID, and description of this DIT content rule
 * , use the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
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
 * RFC 2252 defines DITContentRuleDescription as follows:
 * <P>
 * <PRE>
 *    DITContentRuleDescription = "("
 *        numericoid   ; Structural ObjectClass identifier
 *        [ "NAME" qdescrs ]
 *        [ "DESC" qdstring ]
 *        [ "OBSOLETE" ]
 *        [ "AUX" oids ]    ; Auxiliary ObjectClasses
 *        [ "MUST" oids ]   ; AttributeType identifiers
 *        [ "MAY" oids ]    ; AttributeType identifiers
 *        [ "NOT" oids ]    ; AttributeType identifiers
 *       ")"
 * </PRE>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPSchemaElement
 **/

public class LDAPDITContentRuleSchema extends LDAPSchemaElement {

    static final long serialVersionUID = -8588488481097270056L;

    /**
     * Constructs a blank element.
     */
    protected LDAPDITContentRuleSchema() {
        super();
    }

    /**
     * Constructs a DIT content rule definition, using the specified
     * information.
     * @param names names of the attribute type
     * @param oid object identifier (OID) of the attribute type
     * in dotted-string format (for example, "1.2.3.4")
     * @param description description of attribute type
     * @param obsolete <code>true</code> if the rule is obsolete
     * @param auxiliary a list of auxiliary object classes
     * allowed for an entry to which this content rule applies.
     * These may either be specified by name or numeric oid.
     * @param required a list of user attribute types that an entry
     * to which this content rule applies must contain in addition to
     * its normal set of mandatory attributes. These may either be
     * specified by name or numeric oid.
     * @param optional a list of user attribute types that an entry
     * to which this content rule applies may contain in addition to
     * its normal set of optional attributes. These may either be
     * specified by name or numeric oid.
     * @param precluded a list consisting of a subset of the optional
     * user attribute types of the structural and auxiliary object
     * classes which are precluded from an entry to which this content rule
     * applies. These may either be specified by name or numeric oid.
     */
    public LDAPDITContentRuleSchema( String[] names,
                                     String oid,
                                     String description,
                                     boolean obsolete,
                                     String[] auxiliary,
                                     String[] required,
                                     String[] optional,
                                     String[] precluded ) {
        super( names, oid, description );
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
        if ( auxiliary != null ) {
            for( int i = 0; i < auxiliary.length; i++ ) {
                aux.addElement( auxiliary[i] );
            }
        }
        if ( precluded != null ) {
            for( int i = 0; i < precluded.length; i++ ) {
                not.addElement( precluded[i] );
            }
        }
        if ( obsolete ) {
            setQualifier( OBSOLETE, "" );
        }
    }

    /**
     * Constructs a DIT content rule definition based on a description in
     * the DITContentRuleDescription format. For information on this format,
     * (see <A HREF="http://www.ietf.org/rfc/rfc2252.txt"
     * >RFC 2252, Lightweight Directory Access Protocol (v3):
     * DIT Content Rule Description</A>.  This is the format that LDAP servers
     * and clients use to exchange schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of "attributetypes" are attribute type descriptions
     * in this format.)
     * <P>
     *
     * @param raw definition of the DIT content rule in the
     * DITContentRuleDescription format
     */
    public LDAPDITContentRuleSchema( String raw ) {
        attrName = "ditContentRules";
        parseValue( raw );
        Object o = properties.get( MAY );
        if ( o != null ) {
            if ( o instanceof Vector ) {
                may = (Vector)o;
            } else {
                may.addElement( o );
            }
        }
        o = properties.get( MUST );
        if ( o != null ) {
            if ( o instanceof Vector ) {
                must = (Vector)o;
            } else {
                must.addElement( o );
            }
        }
        o = properties.get( NOT );
        if ( o != null ) {
            if ( o instanceof Vector ) {
                not = (Vector)o;
            } else {
                not.addElement( o );
            }
        }
        o = properties.get( AUX );
        if ( o != null ) {
            if ( o instanceof Vector ) {
                aux = (Vector)o;
            } else {
                aux.addElement( o );
            }
        }
    }

    /**
     * Gets the names of the auxiliary object classes allowed
     * in this content rule.
     * @return the names of auxiliary object classes
     * allowed in this content rule.
     */
    public String[] getAuxiliaryClasses() {
        String[] vals = new String[aux.size()];
        aux.copyInto( vals );
        return vals;
    }

    /**
     * Gets the names of optional attributes allowed
     * in this content rule.
     * @return the names of optional attributes
     * allowed in this content rule.
     */
    public String[] getOptionalAttributes() {
        String[] vals = new String[may.size()];
        may.copyInto( vals );
        return vals;
    }

    /**
     * Gets the names of the precluded attributes for
     * this content rule.
     * @return the names of the precluded attributes
     * for this content rule.
     */
    public String[] getPrecludedAttributes() {
        String[] vals = new String[not.size()];
        not.copyInto( vals );
        return vals;
    }

    /**
     * Gets the names of the required attributes for
     * this content rule.
     * @return the names of the required attributes
     * for this content rule.
     */
    public String[] getRequiredAttributes() {
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
        return "ldapditcontentrules";
    }

    /**
     * Prepares a value in RFC 2252 format for submission to a server
     *
     * @return a String ready for submission to an LDAP server.
     */
    String getValue() {
        String s = getValuePrefix();
        String val = getOptionalValues( NOVALS );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        if ( aux.size() > 0 ) {
            s += AUX + " " + vectorToList( aux );
            s += ' ';
        }
        if ( must.size() > 0 ) {
            s += MUST + " " + vectorToList( must );
            s += ' ';
        }
        if ( may.size() > 0 ) {
            s += MAY + " " + vectorToList( may );
            s += ' ';
        }
        if ( not.size() > 0 ) {
            s += NOT + " " + vectorToList( not );
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
     * Gets the definition of the rule in Directory format
     *
     * @return definition of the rule in Directory format
     */
    public String toString() {
        return getValue();
    }

    public final static String AUX = "AUX";
    public final static String MUST = "MUST";
    public final static String MAY = "MAY";
    public final static String NOT = "NOT";

    // Qualifiers known to not have values; prepare a Hashtable
    static final String[] NOVALS = { "OBSOLETE" };
    static {
        for( int i = 0; i < NOVALS.length; i++ ) {
            novalsTable.put( NOVALS[i], NOVALS[i] );
        }
    }

    // Qualifiers which we output explicitly in toString()
    static final String[] IGNOREVALS = { OBSOLETE,
                                         AUX,
                                         MUST,
                                         MAY,
                                         NOT
                                       };

    private Vector must = new Vector();
    private Vector may = new Vector();
    private Vector aux = new Vector();
    private Vector not = new Vector();
}
