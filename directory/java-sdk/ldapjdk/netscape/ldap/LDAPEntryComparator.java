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

/**
 * The <CODE>LDAPEntryComparator</CODE> interface represents the
 * algorithm used to sort the search results. This interface specifies
 * one method, <CODE>isGreater</CODE>, which compares two entries and
 * determines the order in which the two entries should be sorted.
 * <P>
 *
 * The <CODE>netscape.ldap</CODE> package includes a class that
 * implements this interface.  The <CODE>LDAPCompareAttrNames</CODE>
 * class represents a comparator that sorts the two entries alphabetically,
 * based on the value of one or more attributes.
 * <P>
 *
 * When calling the <CODE>sort</CODE> method of the
 * <CODE>LDAPSearchResults</CODE> class, you need to specify
 * a class that implements the <CODE>LDAPEntryComparator</CODE>
 * interface.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPCompareAttrNames
 * @see netscape.ldap.LDAPSearchResults#sort
 */
public interface LDAPEntryComparator {

     /**
     * Specifies the algorithm used to
     * compare entries when sorting search results.
     * <P>
     *
     * <CODE>isGreater</CODE> returns <CODE>true</CODE>
     * if the entry specified in the first argument should
     * be sorted before the entry specified in the second
     * argument.
     * <P>
     *
     * @version 1.0
     * @see netscape.ldap.LDAPCompareAttrNames
     * @see netscape.ldap.LDAPSearchResults#sort
     */
    public boolean isGreater (LDAPEntry greater, LDAPEntry less);

}
