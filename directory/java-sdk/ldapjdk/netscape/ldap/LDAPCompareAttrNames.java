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
import java.text.*;
import netscape.ldap.client.*;

/**
 * Compares LDAP entries based on one or more attribute values.
 * <P>
 *
 * To use this comparison for sorting search results, pass
 * an object of this class to the <CODE>sort</CODE> method in
 * <CODE>LDAPSearchResults</CODE>.
 * <P>
 *
 * @version 1.0
 * @see LDAPEntryComparator
 * @see LDAPSearchResults#sort
 */

public class LDAPCompareAttrNames implements LDAPEntryComparator {

    String m_attrs[];
    boolean m_ascending[];
    Locale m_locale = null;
    Collator m_collator = null;

    /**
     * Constructs a comparator that compares the string values of
     * a named attribute in LDAP entries and sorts the entries in
     * ascending order.
     * <P>
     *
     * @param attribute Name of attribute for comparisons.
     */
    public LDAPCompareAttrNames (String attribute) {
        m_attrs = new String[1];
        m_attrs[0] = attribute;
        m_ascending = new boolean[1];
        m_ascending[0] = true;
    }

    /**
     * Constructs a comparator that compares the string values in
     * a named attribute in LDAP entries and that allows you to sort
     * entries either in ascending or descending order.
     * <P>
     *
     * @param attribute Name of attribute for comparisons.
     * @param ascendingFlag If <CODE>true</CODE>, sort in ascending order.
     */
    public LDAPCompareAttrNames (String attribute,
                                 boolean ascendingFlag) {
        m_attrs = new String[1];
        m_attrs[0] = attribute;
        m_ascending = new boolean[1];
        m_ascending[0] = ascendingFlag;
    }

    /**
     * Constructs a comparator that compares the string values in
     * a set of named attributes in LDAP entries and that sort
     * the entries in ascending order.
     * <P>
     *
     * Use an array of strings to specify the set of attributes
     * that you want to use for sorting.  If the values of the
     * first attribute (the name specified in <CODE>attribute[0]</CODE>)
     * are equal, then the values of the next attribute are compared.
     * <P>
     *
     * For example, if <CODE>attributes[0] = "cn"</CODE> and
         * <CODE>attributes[1]="uid"</CODE>, results are first sorted
     * by the <CODE>cn</CODE> attribute.  If two entries have the
     * same value for the <CODE>cn</CODE>, then the <CODE>uid</CODE>
     * attribute is used to sort the entries.
     * <P>
     *
     * @param attributes Array of the attribute names used for comparisons.
     */
    public LDAPCompareAttrNames (String[] attributes) {
        m_attrs = attributes;
        m_ascending = new boolean[attributes.length];
        for( int i = 0; i < attributes.length; i++ )
            m_ascending[i] = true;
    }

    /**
     * Constructs a comparator that compares the string values in
     * a set of named attributes in LDAP entries and that allows you
     * to sort the entries in ascending or descending order.
     * <P>
     *
     * Use an array of strings to specify the set of attributes
     * that you want to use for sorting.  If the values of the
     * first attribute (the name specified in <CODE>attribute[0]</CODE>)
     * are equal, then the values of the next attribute are compared.
     * <P>
     *
     * For example, if <CODE>attributes[0] = "cn"</CODE> and
     * <CODE>attributes[1]="uid"</CODE>, results are first sorted
     * by the <CODE>cn</CODE> attribute.  If two entries have the
     * same value for <CODE>cn</CODE>, then the <CODE>uid</CODE>
     * attribute is used to sort the entries.
     * <P>
     *
     * Use an array of boolean values to specify whether each attribute
     * should be sorted in ascending or descending order.  For example,
     * suppose that <CODE>attributes[0] = "cn"</CODE> and
     * <CODE>attributes[1]="roomNumber"</CODE>.  If
     * <CODE>ascendingFlags[0]=true</CODE> and
     * <CODE>ascendingFlags[1]=false</CODE>, attributes are sorted first by
     * <CODE>cn</CODE> in ascending order, then by <CODE>roomNumber</CODE>
     * in descending order.
     * <P>
     *
     * If the size of the array of attribute names is not the same as
     * the size of the array of boolean values, an
     * <CODE>LDAPException</CODE> is thrown.
     * <P>
     *
     * @param attribute Array of the attribute names to use for comparisons.
     * @param ascendingFlags Array of boolean values specifying ascending
     * or descending order to use for each attribute name. If
     * <CODE>true</CODE>, sort the attributes in ascending order.
     */
    public LDAPCompareAttrNames (String[] attributes,
                                 boolean[] ascendingFlags) {
        m_attrs = attributes;
        m_ascending = ascendingFlags;
        if ( m_ascending == null ) {
            m_ascending = new boolean[attributes.length];
            for( int i = 0; i < attributes.length; i++ )
                m_ascending[i] = true;
        }
    }

    /**
     * Get the locale used for collation, if any. If it is null,
     * an ordinary string comparison will be used for sorting.
     *
     * @return The locale used for collation, or null.
     */
    public Locale getLocale() {
        return m_locale;
    }

    /**
     * Set the locale used for collation, if any. If it is null,
     * an ordinary string comparison will be used for sorting.
     *
     * @param locale The locale used for collation, or null.
     */
    public void setLocale( Locale locale ) {
        m_locale = locale;
        if ( m_locale == null ) {
            m_collator = null;
        } else {
            m_collator = Collator.getInstance( m_locale );
        }
    }

    /**
     * If the value of the attribute in the first entry is greater
     * than the attribute in the second entry, returns <CODE>true</CODE>.
     * <P>
     *
     * If one of the entries is missing the attribute, the other is
     * considered greater.  By default, the first entry is greater.
     * <P>
     *
     * If either entry contains multiple values, only the first value
     * is used for comparisons.
     * <P>
     *
     * @param greater Entry to be tested against.
     * @param less Entry to test.
     * @return <CODE>true</CODE> if (<CODE>greater &gt; less</CODE>).
     */
    public boolean isGreater (LDAPEntry greater, LDAPEntry less) {
        if (greater.equals (less)) return false;
        return attrGreater (greater, less, 0);
    }

    /**
     * Compares a particular attribute in both entries. If equal,
     * moves on to the next.
     * @param greater Greater arg from isGreater
     * @param less Less param is isGreater
     * @param attrPos Index into array of attributes, indicating
     * the attribute to compare
     * @return (greater > less)
     */
    boolean attrGreater (LDAPEntry greater, LDAPEntry less,
                         int attrPos) {
        Enumeration greaterAttrSet =
            greater.getAttributeSet().getAttributes();
        Enumeration lessAttrSet =
            less.getAttributeSet().getAttributes();

        String greaterValue = null;
        String lessValue = null;
        String attrName = m_attrs[attrPos];
        boolean ascending = m_ascending[attrPos];

        try {
            while (lessAttrSet.hasMoreElements()) {
                LDAPAttribute currAttr =
                    (LDAPAttribute)(lessAttrSet.nextElement());
                if (!attrName.equalsIgnoreCase (currAttr.getName()))
                    continue;
                lessValue =
                    (String)(currAttr.getStringValues().nextElement());
                break;
            }

            while (greaterAttrSet.hasMoreElements()) {
                LDAPAttribute currAttr =
                    (LDAPAttribute)(greaterAttrSet.nextElement());
                if (!attrName.equalsIgnoreCase (currAttr.getName()))
                    continue;
                greaterValue =
                    (String)(currAttr.getStringValues().nextElement());
                break;
            }
        }
        catch (ClassCastException cce) {
            // i.e. one of the enumerations did not contain the
            // right type !?
            return false;
        }
        catch (NoSuchElementException nse) {
            // i.e. one of the attributes had no values !?
            return false;
        }

        if ((lessValue == null) ^ (greaterValue == null))
            return greaterValue != null;

        if (lessValue == null ||
            lessValue.equalsIgnoreCase (greaterValue))
            if (attrPos == m_attrs.length - 1)
                return false;
            else
                return attrGreater (greater, less, attrPos+1);

        if ( m_collator != null ) {
            if ( ascending )
                return ( m_collator.compare( greaterValue, lessValue ) > 0 );
            else
                return ( m_collator.compare( greaterValue, lessValue ) < 0 );
        } else {
            if ( ascending )
                return (greaterValue.compareTo (lessValue) > 0);
            else
                return (greaterValue.compareTo (lessValue) < 0);
        }
    }

}
