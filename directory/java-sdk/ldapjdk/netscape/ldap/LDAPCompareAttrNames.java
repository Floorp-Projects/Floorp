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
package netscape.ldap;

import java.util.*;
import java.text.*;

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

public class LDAPCompareAttrNames
             implements LDAPEntryComparator, java.io.Serializable {

    static final long serialVersionUID = -2567450425231175944L;
    private String m_attrs[];
    private boolean m_ascending[];
    private Locale m_locale = null;
    private Collator m_collator = null;
    private boolean m_sensitive = true;

    /**
     * Constructs a comparator that compares the string values of
     * a named attribute in LDAP entries and sorts the entries in
     * ascending order.
     * <P>
     *
     * @param attribute name of attribute for comparisons
     */
    public LDAPCompareAttrNames (String attribute) {
        m_attrs = new String[1];
        m_attrs[0] = attribute;
        m_ascending = new boolean[1];
        m_ascending[0] = true;
    }

    /**
     * Constructs a comparator that compares the string values of
     * a named attribute in LDAP entries and that allows you to sort
     * entries either in ascending or descending order.
     * <P>
     *
     * @param attribute name of attribute for comparisons
     * @param ascendingFlag if <CODE>true</CODE>, sort in ascending order
     */
    public LDAPCompareAttrNames (String attribute,
                                 boolean ascendingFlag) {
        m_attrs = new String[1];
        m_attrs[0] = attribute;
        m_ascending = new boolean[1];
        m_ascending[0] = ascendingFlag;
    }

    /**
     * Constructs a comparator that compares the string values of
     * a set of named attributes in LDAP entries and that sort
     * the entries in ascending order.
     * <P>
     *
     * Use an array of strings to specify the set of attributes
     * to use for sorting.  If the values of the first attribute 
     * (the name specified in <CODE>attribute[0]</CODE>) are equal, 
     * then the values of the next attribute are compared.
     * <P>
     *
     * For example, if <CODE>attributes[0] = "cn"</CODE> and
     * <CODE>attributes[1]="uid"</CODE>, results are first sorted
     * by the <CODE>cn</CODE> attribute.  If two entries have the
     * same value for <CODE>cn</CODE>, then the <CODE>uid</CODE>
     * attribute is used to sort the entries.
     * <P>
     *
     * @param attributes array of the attribute names used for comparisons
     */
    public LDAPCompareAttrNames (String[] attributes) {
        m_attrs = attributes;
        m_ascending = new boolean[attributes.length];
        for( int i = 0; i < attributes.length; i++ )
            m_ascending[i] = true;
    }

    /**
     * Constructs a comparator that compares the string values of
     * a set of named attributes in LDAP entries and allows you
     * to sort the entries in ascending or descending order.
     * <P>
     *
     * Use an array of strings to specify the set of attributes
     * to use for sorting.  If the values of the first attribute 
     * (the name specified in <CODE>attribute[0]</CODE>)
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
     * @param attribute array of the attribute names to use for comparisons
     * @param ascendingFlags array of boolean values specifying ascending
     * or descending order to use for each attribute name. If
     * <CODE>true</CODE>, the attributes are sorted in ascending order.
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
     * Gets the locale, if any, used for collation. If the locale is null,
     * an ordinary string comparison is used for sorting.
     *
     * @return the locale used for collation, or null.
     */
    public Locale getLocale() {
        return m_locale;
    }

    /**
     * Set the locale, if any, used for collation. If the locale is null,
     * an ordinary string comparison is used for sorting. If sorting
     * has been set to case-insensitive, the collation strength is set
     * to Collator.PRIMARY, otherwise to Collator.IDENTICAL. If a
     * different collation strength setting is required, use the signature
     * that takes a collation strength parameter.
     *
     * @param locale the locale used for collation, or null.
     */
    public void setLocale( Locale locale ) {
        if ( m_sensitive ) {
            setLocale( locale, Collator.IDENTICAL );
        } else {
            setLocale( locale, Collator.PRIMARY );
        }
    }

    /**
     * Sets the locale, if any, used for collation. If the locale is null,
     * an ordinary string comparison is used for sorting.
     *
     * @param locale the locale used for collation, or null.
     * @param strength collation strength: Collator.PRIMARY,
     * Collator.SECONDARY, Collator.TERTIARY, or Collator.IDENTICAL
     */
    public void setLocale( Locale locale, int strength ) {
        m_locale = locale;
        if ( m_locale == null ) {
            m_collator = null;
        } else {
            m_collator = Collator.getInstance( m_locale );
            m_collator.setStrength(strength);
        }
    }

    /**
     * Gets the state of the case-sensitivity flag. This only applies to
     * Unicode sort order; for locale-specific sorting, case-sensitivity
     * is controlled by the collation strength.
     *
     * @return <code>true</code> for case-sensitive sorting; this is
     * the default
     */
    public boolean getCaseSensitive() {
        return m_sensitive;
    }

    /**
     * Sets the state of the case-sensitivity flag. This only applies to
     * Unicode sort order; for locale-specific sorting, case-sensitivity
     * is controlled by the collation strength.
     *
     * @param sensitive <code>true</code> for case-sensitive sorting;
     * this is the default
     */
    public void setCaseSensitive( boolean sensitive ) {
        m_sensitive = sensitive;
    }

    /**
     * Returns <CODE>true</CODE> if the value of the attribute in the first entry is greater
     * than the value of the attribute in the second entry.
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
     * @param greater entry against which to test
     * @param less entry to test
     * @return <CODE>true</CODE> if (<CODE>greater &gt; less</CODE>).
     */
    public boolean isGreater (LDAPEntry greater, LDAPEntry less) {
        if (greater.equals (less)) return false;
        return attrGreater (greater, less, 0);
    }

    /**
     * Compares a particular attribute in both entries. If equal,
     * moves on to the next.
     * @param greater greater arg from isGreater
     * @param less less arg from isGreater
     * @param attrPos the index in an array of attributes, indicating
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

        // Check for equality
        if ( (lessValue == null) ||
             ((m_collator != null) &&
              (m_collator.compare( greaterValue, lessValue ) == 0) ) ||
             ((m_collator == null) && m_sensitive &&
              lessValue.equals(greaterValue)) ||
             ((m_collator == null) && !m_sensitive &&
              lessValue.equalsIgnoreCase(greaterValue)) ) {

            if (attrPos == m_attrs.length - 1) {
                return false;
            } else {
                return attrGreater (greater, less, attrPos+1);
            }
        }

        // Not equal, check for order
        if ( ascending ) {
            if ( m_collator != null ) {
                return ( m_collator.compare( greaterValue, lessValue ) > 0 );
            } else if ( m_sensitive ) {
                return (greaterValue.compareTo (lessValue) > 0);
            } else {
                return (greaterValue.toLowerCase().compareTo (
                    lessValue.toLowerCase()) > 0);
            }
        } else {
            if ( m_collator != null ) {
                return ( m_collator.compare( greaterValue, lessValue ) < 0 );
            } else if ( m_sensitive ) {
                return (greaterValue.compareTo (lessValue) < 0);
            } else {
                return (greaterValue.toLowerCase().compareTo (
                    lessValue.toLowerCase()) < 0);
            }
        }
    }
}
