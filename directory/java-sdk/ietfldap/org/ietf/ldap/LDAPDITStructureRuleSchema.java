/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The structures of this file are subject to the Netscape Public
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
 * The definition of a DIT structure rule in the schema.
 * <A HREF="http://www.ietf.org/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * DIT Structure Rule Description</A> covers the types of information
 * to specify when defining a DIT structure rule. According to the RFC, 
 * the description of a DIT structure rule can include the following:
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
 * When you construct an <CODE>LDAPDITStructureRuleSchema</CODE> object, you can
 * specify these types of information as arguments to the constructor or
 * in the AttributeTypeDescription format specified in RFC 2252.
 * When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.
 * <P>
 *
 * There are a number of additional optional description fields which
 * are not explicitly accessible through LDAPDITStructureRuleSchema, but which
 * can be managed with setQualifier, getQualifier, and getQualifierNames:
 * <P>
 *
 * <UL>
 * <LI>OBSOLETE
 * </UL>
 * <P>
 *
 * To get the name, OID, and description of this DIT structure rule
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
 * RFC 2252 defines DITStructureRuleDescription as follows:
 * <P>
 * <PRE>
 *      DITStructureRuleDescription = "(" whsp
 *        ruleidentifier whsp            ; DITStructureRule identifier
 *        [ "NAME" qdescrs ]
 *        [ "DESC" qdstring ]
 *        [ "OBSOLETE" whsp ]
 *        "FORM" woid whsp               ; NameForm
 *        [ "SUP" ruleidentifiers whsp ] ; superior DITStructureRules
 *    ")"
 * </PRE>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPSchemaElement
 **/

public class LDAPDITStructureRuleSchema extends LDAPSchemaElement {

    static final long serialVersionUID = -2823317246039655811L;

    /**
     * Constructs a blank element.
     */
    protected LDAPDITStructureRuleSchema() {
        super();
    }

    /**
     * Constructs a DIT structure rule definition, using the specified
     * information.
     * @param names names of the element
     * @param ruleID unique identifier of the structure rule.<BR>
     * NOTE: this is an integer, not a dotted numerical identifier.
     * Structure rules aren't identified by OID.
     * @param description description of attribute type
     * @param obsolete <code>true</code> if the rule is obsolete
     * @param nameForm either the identifier or name of a name form.
     * This is used to indirectly refer to the object class that this
     * structure rule applies to.
     * @param superiors list of superior structure rules - specified
     * by their integer ID. The object class specified by this structure
     * rule (via the nameForm parameter) may only be subordinate in
     * the DIT to object classes of those represented by the structure
     * rules here.
     */
    public LDAPDITStructureRuleSchema( String[] names,
                                       int ruleID,
                                       String description,
                                       boolean obsolete,
                                       String nameForm,
                                       String[] superiors ) {
        super( names, "", description );
        this.nameForm = nameForm;
        this.ruleID = ruleID;
        if ( obsolete ) {
            setQualifier( OBSOLETE, "" );
        }
        if ( (superiors != null) && (superiors.length > 0) ) {
            setQualifier( SUPERIOR, superiors );
        }
    }

    /**
     * Constructs a DIT structure rule definition based on a description in
     * the DITStructureRuleDescription format. For information on this format,
     * (see <A HREF="http://www.ietf.org/rfc/rfc2252.txt"
     * >RFC 2252, Lightweight Directory Access Protocol (v3):
     * DIT Structure Rule Description</A>.  This is the format that
     * LDAP servers
     * and clients use to exchange schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of "attributetypes" are attribute type descriptions
     * in this format.)
     * <P>
     *
     * @param raw definition of the DIT structure rule in the
     * DITStructureRuleDescription format
     */
    public LDAPDITStructureRuleSchema( String raw ) {
        attrName = "ditStructureRules";
        parseValue( raw );
        Object o = properties.get( FORM );
        if ( o != null ) {
            nameForm = (String)o;
        }
        try {
            ruleID = Integer.parseInt( oid );
        } catch ( Exception e ) {
        }
    }

    /**
     * Returns the NameForm that this structure rule controls. You can get
     * the actual object class that this structure rule controls by calling
     * getNameForm().getObjectClass().
     *
     * @return the NameForm that this structure rule controls.
     */
    public String getNameForm() {
        return nameForm;
    }

    /**
     * Returns the rule ID for this structure rule. Note that this returns
     * an integer rather than a dotted decimal OID. Objects of this class do
     * not have an OID, thus getID will return null.
     *
     * @return the rule ID for this structure rule.
     */
    public int getRuleID() {
        return ruleID;
    }

    /**
     * Returns a list of all structure rules that are superior to this
     * structure rule. To resolve to an object class, you need to first
     * resolve the superior id to another structure rule, then call
     * getNameForm().getObjectClass() on that structure rule.
     * @return the structure rules that are superior to this
     * structure rule.
     */
    public String[] getSuperiors() {
        return getQualifier( SUPERIOR );
    }

    /**
     * Prepares a value in RFC 2252 format for submission to a server
     *
     * @return a String ready for submission to an LDAP server.
     */
    String getValue() {
        String s = "( " + ruleID + ' ';
        s += getNameField();
        if ( description != null ) {
            s += "DESC \'" + description + "\' ";
        }
        if ( isObsolete() ) {
            s += OBSOLETE + ' ';
        }

        s += FORM + " " + nameForm + ' ';
        String val = getValue( SUPERIOR, false );
        if ( (val != null) && (val.length() > 1) ) {
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
     * Gets the definition of the rule in Directory format
     *
     * @return definition of the rule in Directory format
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
        return "ldapditstructurerules";
    }

    public final static String FORM = "FORM";

    // Qualifiers known to not have values; prepare a Hashtable
    static final String[] NOVALS = { "OBSOLETE" };
    static {
        for( int i = 0; i < NOVALS.length; i++ ) {
            novalsTable.put( NOVALS[i], NOVALS[i] );
        }
    }

    // Qualifiers which we output explicitly in toString()
    static final String[] IGNOREVALS = { OBSOLETE,
                                         FORM,
                                         "SUP"
                                       };

    private String nameForm = null;
    private int ruleID = 0;
}
