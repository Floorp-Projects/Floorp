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
import netscape.ldap.client.*;
import netscape.ldap.client.opers.*;

/**
 * Represents a set of attributes (for example, the set of attributes
 * in an entry).
 *
 * @version 1.0
 * @see netscape.ldap.LDAPAttribute
 */
public class LDAPAttributeSet implements Cloneable
{
    Hashtable attrHash = null;
    LDAPAttribute[] attrs = new LDAPAttribute[0];
    /* If there are less attributes than this in the set, it's not worth
     creating a Hashtable - faster and cheaper most likely to string
     comparisons. Most applications fetch attributes once only, anyway */
    static final int ATTR_COUNT_REQUIRES_HASH = 5;

    /**
     * Constructs a new set of attributes.  This set is initially empty.
     */
    public LDAPAttributeSet() {
    }

    /**
     * Constructs an attribute set.
     * @param attrs The list of attributes.
     */
    public LDAPAttributeSet( LDAPAttribute[] attrs ) {
        this.attrs = attrs;
    }

    public synchronized Object clone() {
        try {
            LDAPAttributeSet attributeSet = new LDAPAttributeSet();
            attributeSet.attrs = new LDAPAttribute[attrs.length];
            for (int i = 0; i < attrs.length; i++) {
                attributeSet.attrs[i] = new LDAPAttribute(attrs[i]);
            }
             return attributeSet;
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Returns an enumeration of the attributes in this attribute set.
     * @return Enumeration of the attributes in this set.
     */
    public Enumeration getAttributes () {
        Vector v = new Vector();
        synchronized(this) {
            for (int i=0; i<attrs.length; i++) {
                v.addElement(attrs[i]);
            }
        }
        return v.elements();
    }

    /**
     * Creates a new attribute set containing only the attributes
     * that have the specified subtypes.
     * <P>
     *
     * For example, suppose an attribute set contains the following attributes:
     * <P>
     *
     * <PRE>
     * cn
     * cn;lang-ja
     * sn;phonetic;lang-ja
     * sn;lang-us
     * </PRE>
     *
     * If you call the <CODE>getSubset</CODE> method and pass
     * <CODE>lang-ja</CODE> as the argument, the method returns
     * an attribute set containing the following attributes:
     * <P>
     *
     * <PRE>
     * cn;lang-ja
     * sn;phonetic;lang-ja
     * </PRE>
     *
     * @param subtype Semi-colon delimited list of subtypes
     * that you want to find in attribute names.
     * For example:
     *<PRE>
     *     "lang-ja"        // Only Japanese language subtypes
     *     "binary"         // Only binary subtypes
     *     "binary;lang-ja" // Only Japanese language subtypes
     *                         which also are binary
     *</PRE>
     * @return Attribute set containing the attributes that have
     * the specified subtypes
     * @see netscape.ldap.LDAPAttribute
     * @see netscape.ldap.LDAPAttributeSet#getAttribute
     * @see netscape.ldap.LDAPEntry#getAttributeSet
     */
    public LDAPAttributeSet getSubset(String subtype) {
        LDAPAttributeSet attrs = new LDAPAttributeSet();
        if ( subtype == null )
            return attrs;
        StringTokenizer st = new StringTokenizer(subtype, ";");
        if( st.countTokens() < 1 )
            return attrs;
        String[] searchTypes = new String[st.countTokens()];
        int i = 0;
        while( st.hasMoreElements() ) {
            searchTypes[i] = (String)st.nextElement();
            i++;
        }
        Enumeration attrEnum = getAttributes();
        while( attrEnum.hasMoreElements() ) {
            LDAPAttribute attr = (LDAPAttribute)attrEnum.nextElement();
            if( attr.hasSubtypes( searchTypes ) )
                attrs.add( new LDAPAttribute( attr ) );
        }
        return attrs;
    }

    /**
     * Returns a single attribute that exactly matches the specified attribute
     * name.
     * @param attrName Name of attribute to return.
     * For example:
     *<PRE>
     *     "cn"            // Only a non-subtyped version of cn
     *     "cn;lang-ja"    // Only a Japanese version of cn
     *</PRE>
     * @return Attribute that has exactly the same name, or null
     * (if no attribute in the set matches the specified name).
     * @see netscape.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute( String attrName ) {
        prepareHashtable();
        if (attrHash != null) {
            return (LDAPAttribute)attrHash.get( attrName.toLowerCase() );
        } else {
            for (int i = 0; i < attrs.length; i++) {
                if (attrName.equalsIgnoreCase(attrs[i].getName())) {
                    return attrs[i];
                }
            }
            return null;
        }
    }

    /**
     * Prepare hashtable for fast attribute lookups
     */
    private void prepareHashtable() {
        if ((attrHash == null) && (attrs.length >= ATTR_COUNT_REQUIRES_HASH)) {
            attrHash = new Hashtable();
            for (int j = 0; j < attrs.length; j++) {
                attrHash.put( attrs[j].getName().toLowerCase(), attrs[j] );
            }
        }
    }

    /**
     * Returns the subtype that matches the attribute name specified
     * by <CODE>attrName</CODE> and the language specificaton identified
     * by <CODE>lang</CODE>.
     * <P>
     *
     * If no attribute in the set has the specified name and subtype,
     * the method returns <CODE>null</CODE>.
     *
     * Attributes containing subtypes other than <CODE>lang</CODE>
     * (for example, <CODE>cn;binary</CODE>) are returned only if
     * they contain the specified <CODE>lang</CODE> subtype and if
     * the set contains no attribute having only the <CODE>lang</CODE>
     * subtype.  (For example, <CODE>getAttribute( "cn", "lang-ja" )</CODE>
     * returns the <CODE>cn;lang-ja;phonetic</CODE> attribute only if
     * the <CODE>cn;lang-ja</CODE> attribute does not exist.)
     * <P>
     *
     * If null is specified for the <CODE>lang</CODE> argument,
     * calling this method is the same as calling the
     * <CODE>getAttribute(attrName)</CODE> method.
     * <P>
     *
     * For example, suppose an entry contains only the following attributes:
     * <P>
     * <UL>
     * <LI><CODE>cn;lang-en</CODE>
     * <LI><CODE>cn;lang-ja-JP-kanji</CODE>
     * <LI><CODE>sn</CODE>
     * </UL>
     * <P>
     *
     * Calling the following methods will return the following values:
     * <P>
     * <UL>
     * <LI><CODE>getAttribute( "cn" )</CODE> returns <CODE>null</CODE>.
     * <LI><CODE>getAttribute( "sn" )</CODE> returns the "<CODE>sn</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-en-us" )</CODE> returns the "<CODE>cn;lang-en</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-en" )</CODE> returns the "<CODE>cn;lang-en</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-ja" )</CODE> returns <CODE>null</CODE>.
     * <LI><CODE>getAttribute( "sn", "lang-en" )</CODE> returns the "<CODE>sn</CODE>" attribute.
     *</UL>
     * <P>
     * @param attrName Name of attribute to find in the entry.
     * @param lang A language specification.
     * @return The attribute that matches the base name and that best
     * matches any specified language subtype.
     * @see netscape.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute( String attrName, String lang ) {
        if ( (lang == null) || (lang.length() < 1) )
            return getAttribute( attrName );

        String langLower = lang.toLowerCase();
        if ((langLower.length() < 5) ||
            ( !langLower.substring( 0, 5 ).equals( "lang-" ) )) {
            return null;
        }
        StringTokenizer st = new StringTokenizer( langLower, "-" );
        // Skip first token, which is "lang-"
        st.nextToken();
        String[] langComponents = new String[st.countTokens()];
        int i = 0;
        while ( st.hasMoreTokens() ) {
            langComponents[i] = st.nextToken();
            i++;
        }

        String searchBasename = LDAPAttribute.getBaseName(attrName);
        String[] searchTypes = LDAPAttribute.getSubtypes(attrName);
        LDAPAttribute found = null;
        int matchCount = 0;
        for( i = 0; i < attrs.length; i++ ) {
            boolean isCandidate = false;
            LDAPAttribute attr = attrs[i];
            // Same base name?
            if ( attr.getBaseName().equalsIgnoreCase(searchBasename) ) {
                // Accept any subtypes?
                if( (searchTypes == null) || (searchTypes.length < 1) ) {
                    isCandidate = true;
                } else {
                    // No, have to check each subtype for inclusion
                    if( attr.hasSubtypes( searchTypes ) )
                    isCandidate = true;
                }
            }
            String attrLang = null;
            if ( isCandidate ) {
                attrLang = attr.getLangSubtype();
                // At this point, the base name and subtypes are okay
                if ( attrLang == null ) {
                    // If there are no language attributes, this one is okay
                    found = attr;
                } else {
                    // We just have to check for language match
                    st = new StringTokenizer( attrLang.toLowerCase(), "-" );
                    // Skip first token, which is "lang-"
                    st.nextToken();
                    // No match if the attribute's language spec is longer
                    // than the target one
                    if ( st.countTokens() > langComponents.length )
                        continue;

                    // How many subcomponents of the language match?
                    int j = 0;
                    while ( st.hasMoreTokens() ) {
                        if ( !langComponents[j].equals( st.nextToken() ) ) {
                            j = 0;
                            break;
                        }
                        j++;
                    }
                    if ( j > matchCount ) {
                        found = attr;
                        matchCount = j;
                    }
                }
            }
        }
        return found;
    }

    /**
     * Returns the attribute at the position specified by the index
     * (for example, if you specify the index 0, the method returns the
     * first attribute in the set).  The index is 0-based.
     *
     * @param index Index of the attribute that you want to get.
     * @return Attribute at the position specified by the index.
     */
    public LDAPAttribute elementAt (int index) {
        return attrs[index];
    }

    /**
     * Removes attribute at the position specified by the index
     * (for example, if you specify the index 0, the method removes the
     * first attribute in the set).  The index is 0-based.
     *
     * @param index Index of the attribute that you want to remove.
     */
    public void removeElementAt (int index) {
        if ((index >= 0) && (index < attrs.length)) {
            synchronized(this) {
                LDAPAttribute[] vals = new LDAPAttribute[attrs.length-1];
                int j = 0;
                for (int i = 0; i < attrs.length; i++) {
                    if (i != index) {
                        vals[j++] = attrs[i];
                    }
                }
                attrs = vals;
            }
        }
    }

    /**
     * Returns the number of attributes in this set.
     * @return Number of attributes in this attribute set.
     */
    public int size () {
        return attrs.length;
    }

    /**
     * Adds the specified attribute to this attribute set.
     * @param attr Attribute that you want to add to this set.
     */
    public synchronized void add( LDAPAttribute attr ) {
        if (attr != null) {
            LDAPAttribute[] vals = new LDAPAttribute[attrs.length+1];
            for (int i = 0; i < attrs.length; i++)
                vals[i] = attrs[i];
            vals[attrs.length] = attr;
            attrs = vals;
            if (attrHash != null) {
                attrHash.put( attr.getName().toLowerCase(), attr );
            }
        }
    }

    /**
     * Removes the specified attribute from the set.
     * @param name Name of the attribute that you want removed.
     */
    public synchronized void remove( String name ) {
        for( int i = 0; i < attrs.length; i++ ) {
            if ( name.equalsIgnoreCase( attrs[i].getName() ) ) {
                removeElementAt(i);
                break;
            }
        }
    }

    /**
     * Retrieves the string representation of all attributes
     * in the attribute set.  For example:
     *
     * <PRE>
     * LDAPAttributeSet: LDAPAttribute {type='cn', values='Barbara Jensen,Babs
     * Jensen'}LDAPAttribute {type='sn', values='Jensen'}LDAPAttribute {type='
     * givenname', values='Barbara'}LDAPAttribute {type='objectclass', values=
     * 'top,person,organizationalPerson,inetOrgPerson'}LDAPAttribute {type='ou',
     * values='Product Development,People'}
     * </PRE>
     *
     * @return String representation of all attributes in the set.
     */
    public String toString() {
        StringBuffer sb = new StringBuffer("LDAPAttributeSet: ");
        for( int i = 0; i < attrs.length; i++ ) {
            if (i != 0) {
                sb.append(" ");
            }            
            sb.append(attrs[i].toString());
        }
        return sb.toString();
    }
}
