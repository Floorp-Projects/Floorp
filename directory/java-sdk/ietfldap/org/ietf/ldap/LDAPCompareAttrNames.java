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

import java.io.Serializable;
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
 * @see LDAPSearchResults#sort
 */

public class LDAPCompareAttrNames
             implements Comparator, Serializable {

    static final long serialVersionUID = -2567450425231175944L;
    private String _attrs[];
    private boolean _ascending[];
    private Locale _locale = null;
    private Collator _collator = null;
    private boolean _sensitive = true;

    /**
     * Constructs a comparator that compares the string values of
     * a named attribute in LDAP entries and sorts the entries in
     * ascending order.
     * <P>
     *
     * @param attribute name of attribute for comparisons
     */
    public LDAPCompareAttrNames (String attribute) {
        _attrs = new String[1];
        _attrs[0] = attribute;
        _ascending = new boolean[1];
        _ascending[0] = true;
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
        _attrs = new String[1];
        _attrs[0] = attribute;
        _ascending = new boolean[1];
        _ascending[0] = ascendingFlag;
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
        _attrs = attributes;
        _ascending = new boolean[attributes.length];
        for( int i = 0; i < attributes.length; i++ )
            _ascending[i] = true;
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
        _attrs = attributes;
        _ascending = ascendingFlags;
        if ( _ascending == null ) {
            _ascending = new boolean[attributes.length];
            for( int i = 0; i < attributes.length; i++ )
                _ascending[i] = true;
        }
    }

    /**
     * Indicates if some other object is "equal to" this Comparator
     *
     * @param obj the object to compare
     * @return true if the object equals this object
     */
    public boolean equals( Object obj ) {
        return super.equals( obj );
    }
       
    /**
     * Gets the locale, if any, used for collation. If the locale is null,
     * an ordinary string comparison is used for sorting.
     *
     * @return the locale used for collation, or null.
     */
    public Locale getLocale() {
        return _locale;
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
        if ( _sensitive ) {
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
        _locale = locale;
        if ( _locale == null ) {
            _collator = null;
        } else {
            _collator = Collator.getInstance( _locale );
            _collator.setStrength(strength);
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
        return _sensitive;
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
        _sensitive = sensitive;
    }

    /**
     * Compares its two arguments for order
     *
     * @param entry1 the first entry to be compared
     * @param entry2 the second entry to be compared
     * @return a negative integer, zero, or a positive integer if the first
     * argument is less than, equal to, or greater than the second
     * @exception ClassCastException if either of the arguments is not
     * an LDAPEntry
     */
     public int compare( Object entry1, Object entry2 ) {
         if ( !(entry1 instanceof LDAPEntry) ||
              !(entry2 instanceof LDAPEntry) ) {
             throw new ClassCastException( "Must compare LDAPEntry objects" );
         }

         return attrGreater( (LDAPEntry)entry1, (LDAPEntry)entry2, 0 );
     }

    /**
     * Compares a particular attribute in both entries. If equal,
     * moves on to the next.
     * @param entry1 the first entry to be compared
     * @param entry2 the second entry to be compared
     * @return a negative integer, zero, or a positive integer if the first
     * argument is less than, equal to, or greater than the second
     */
    int attrGreater( LDAPEntry entry1, LDAPEntry entry2,
                     int attrPos ) {
        Iterator attrSet1 =
            entry1.getAttributeSet().iterator();
        Iterator attrSet2 =
            entry2.getAttributeSet().iterator();

        String value1 = null;
        String value2 = null;
        String attrName = _attrs[attrPos];
        boolean ascending = _ascending[attrPos];

        while ( attrSet2.hasNext() ) {
            LDAPAttribute currAttr = (LDAPAttribute)(attrSet2.next());
            if ( !attrName.equalsIgnoreCase (currAttr.getName()) ) {
                continue;
            }
            value2 = (String)(currAttr.getStringValues().nextElement());
            break;
        }

        while ( attrSet1.hasNext() ) {
            LDAPAttribute currAttr = (LDAPAttribute)(attrSet1.next());
            if ( !attrName.equalsIgnoreCase(currAttr.getName()) ) {
                continue;
            }
            value1 = (String)(currAttr.getStringValues().nextElement());
            break;
        }

        if ( (value2 == null) ^ (value1 == null) ) {
            return (value1 != null) ? 1 : -1;
        }

        // Check for equality
        if ( (value2 == null) ||
             ((_collator != null) &&
              (_collator.compare( value1, value2 ) == 0) ) ||
             ((_collator == null) && _sensitive &&
              value2.equals(value1)) ||
             ((_collator == null) && !_sensitive &&
              value2.equalsIgnoreCase(value1)) ) {

            if ( attrPos == _attrs.length - 1 ) {
                return -1;
            } else {
                return attrGreater ( entry1, entry2, attrPos+1 );
            }
        }

        // Not equal, check for order
        if ( ascending ) {
            if ( _collator != null ) {
                return _collator.compare( value1, value2 );
            } else if ( _sensitive ) {
                return value1.compareTo( value2 );
            } else {
                return value1.toLowerCase().compareTo( value2.toLowerCase() );
            }
        } else {
            if ( _collator != null ) {
                return _collator.compare( value1, value2 );
            } else if ( _sensitive ) {
                return value1.compareTo( value2 );
            } else {
                return value1.toLowerCase().compareTo( value2.toLowerCase() );
            }
        }
     }
}
