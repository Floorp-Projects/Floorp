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

import java.io.Serializable;
import java.util.*;

/**
 * Represents an entry in the directory.
 *
 * @version 1.0
 */
public class LDAPEntry implements Serializable {

    static final long serialVersionUID = -5563306228920012807L;
    private String dn = null;
    private LDAPAttributeSet attrSet = null;

    /**
     * Constructs an empty entry.
     */
    public LDAPEntry() {
        dn = null;
        attrSet = new LDAPAttributeSet();
    }

    /**
     * Constructs a new entry with the specified distinguished name and with
     * an empty attribute set.
     * @param distinguishedName the distinguished name of the new entry
     */
    public LDAPEntry( String distinguishedName ) {
        dn = distinguishedName;
        attrSet = new LDAPAttributeSet();
    }

    /**
     * Constructs a new entry with the specified distinguished name and
     * set of attributes.
     * @param distinguishedName the distinguished name of the new entry
     * @param attrs the set of attributes to assign to the new entry
     * @see org.ietf.ldap.LDAPAttributeSet
     */
    public LDAPEntry( String distinguishedName, LDAPAttributeSet attrs ) {
        dn = distinguishedName;
        attrSet = attrs;
    }

    /**
     * Returns the distinguished name of the current entry.
     * @return distinguished name of the current entry.
     */
    public String getDN() {
        return dn;
    }

    void setDN(String name) {
        dn = name;
    }

    /**
     * In an entry, returns the single attribute that exactly matches the
     * specified attribute name.
     * @param attrName name of attribute to return
     * For example:
     *<PRE>
     *     "cn"            // Only a non-subtyped version of cn
     *     "cn;lang-ja"    // Only a Japanese version of cn, will not
     *                     // return "cn;lang-ja-JP-kanji", for example
     *</PRE>
     * @return attribute in the current entry that has exactly the same name,
     * or null (if no attribute in the entry matches the specified name).
     * @see org.ietf.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute(String attrName) {
        return attrSet.getAttribute(attrName);
    }

    /**
     * Returns the subtype that matches "attrName" and that best matches
     * a language specification "lang". If there are subtypes other than
     * "lang" subtypes included in attrName, e.g. "cn;binary", only
     * attributes with all of those subtypes are returned. If lang is
     * null or empty, the method behaves as getAttribute(attrName). If
     * there are no matching attributes, null is returned.
     *
     * Example:<PRE>
     *  Assume the entry contains only the following attributes:
     *     <CODE>cn;lang-en</CODE>
     *     <CODE>cn;lang-ja-JP-kanji</CODE>
     *     <CODE>sn</CODE>
     *  getAttribute( "cn" ) returns <CODE>null</CODE>.
     *  getAttribute( "sn" ) returns the "<CODE>sn</CODE>" attribute.
     *  getAttribute( "cn", "lang-en-us" ) returns the "<CODE>cn;lang-en</CODE>" attribute.
     *  getAttribute( "cn", "lang-en" ) returns the "<CODE>cn;lang-en</CODE>" attribute.
     *  getAttribute( "cn", "lang-ja" ) returns <CODE>null</CODE>.
     *  getAttribute( "sn", "lang-en" ) returns the "<CODE>sn</CODE>" attribute.
     *</PRE>
     * <P>
     * @param attrName name of attribute to find in the entry
     * @param lang a language specification (for example, <CODE>lang-en</CODE>)
     * @return the attribute that matches the base name and that best
     * matches any specified language subtype.
     * @see org.ietf.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute( String attrName, String lang ) {
        return attrSet.getAttribute( attrName, lang );
    }

    /**
     * Returns the attribute set of the entry.
     * @return set of attributes in the entry.
     * @see org.ietf.ldap.LDAPAttributeSet
     */
    public LDAPAttributeSet getAttributeSet() {
        return attrSet;
    }

    /**
     * Creates a new attribute set containing only the attributes
     * that have the specified subtypes.
     * <P>
     *
     * For example, suppose an entry contains the following attributes:
     * <P>
     *
     * <PRE>
     * cn
     * cn;lang-ja
     * sn;phonetic;lang-ja
     * sn;lang-us
     * </PRE>
     *
     * If you call the <CODE>getAttributeSet</CODE> method and pass
     * <CODE>lang-ja</CODE> as the argument, the method returns
     * an attribute set containing the following attributes:
     * <P>
     *
     * <PRE>
     * cn;lang-ja
     * sn;phonetic;lang-ja
     * </PRE>
     *
     * @param subtype semi-colon delimited list of subtypes
     * that you want to find in attribute names.
     *<PRE>
     *     "lang-ja"        // Only Japanese language subtypes
     *     "binary"         // Only binary subtypes
     *     "binary;lang-ja" // Only Japanese language subtypes
     *                         which also are binary
     *</PRE>
     * @return attribute set containing the attributes that have
     * the specified subtypes.
     * @see org.ietf.ldap.LDAPAttributeSet
     * @see org.ietf.ldap.LDAPAttributeSet#getSubset
     */
    public LDAPAttributeSet getAttributeSet(String subtype) {
        return attrSet.getSubset(subtype);
    }

    /**
     * Retrieves the string representation of the entry's
     * distinguished name (DN) and its attributes.
     * For example:
     *
     * <PRE>
     * LDAPEntry: uid=bjensen, ou=People, o=airius.com; LDAPAttributeSet:
     * LDAPAttribute {type='cn', values='Barbara Jensen,Babs Jensen'}
     * LDAPAttribute {type='sn', values='Jensen'}LDAPAttribute {type='givenname',
     * values='Barbara'}LDAPAttribute {type='objectclass', values='top,person,
     * organizationalPerson,inetOrgPerson'}LDAPAttribute {type='ou',
     * values='Product Development,People'}
     * </PRE>
     *
     * @return string representation of the entry's DN and its attributes.
     */
    public String toString() {
        StringBuffer sb = new StringBuffer("LDAPEntry: ");
        if ( dn != null ) {
            sb.append(dn);
            sb.append("; ");
        }            
        if ( attrSet != null ) {
            sb.append(attrSet.toString());
        }
        return sb.toString();
    }
}
