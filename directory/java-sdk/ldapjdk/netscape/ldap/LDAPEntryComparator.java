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
