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
package com.netscape.jndi.ldap.controls;

import javax.naming.ldap.Control;
import netscape.ldap.controls.*;

/**
 * Represents control data for returning paged results from a search.
 *
 * <PRE>
 *      VirtualListViewRequest ::= SEQUENCE {
 *                      beforeCount    INTEGER,
 *                      afterCount     INTEGER,
 *                      CHOICE {
 *                      byIndex [0] SEQUENCE {
 *                          index           INTEGER,
 *                          contentCount    INTEGER }
 *                      byFilter [1] jumpTo    Substring }
 * </PRE>
 *
 */

public class LdapVirtualListControl extends LDAPVirtualListControl implements Control {

    /**
     * Constructs a new <CODE>LDAPVirtualListControl</CODE> object. Use this
     * constructor on an initial search operation, specifying the first
     * entry to be matched, or the initial part of it.
     * @param jumpTo An LDAP search expression defining the result set.
     * @param beforeCount The number of results before the top/center to
     * return per page.
     * @param afterCount The number of results after the top/center to
     * return per page.
     */
    public LdapVirtualListControl( String jumpTo, int beforeCount,
                                   int afterCount  ) {
        super( jumpTo, beforeCount, afterCount);
    }

    /**
     * Constructs a new <CODE>LDAPVirtualListControl</CODE> object. Use this
     * constructor on a subsquent search operation, after we know the
     * size of the virtual list, to fetch a subset.
     * @param startIndex The index into the virtual list of an entry to
     * return.
     * @param beforeCount The number of results before the top/center to
     * return per page.
     * @param afterCount The number of results after the top/center to
     * return per page.
     */
    public LdapVirtualListControl( int startIndex, int beforeCount,
                                   int afterCount, int contentCount  ) {
        super( startIndex, beforeCount, afterCount, contentCount );
    }

    /**
     * Sets the starting index, and the number of entries before and after
     * to return. Apply this method to a control returned from a previous
     * search, to specify what result range to return on the next search.
     * @param startIndex The index into the virtual list of an entry to
     * return.
     * @param beforeCount The number of results before startIndex to
     * return per page.
     * @param afterCount The number of results after startIndex to
     * return per page.
     */
    public void setRange( int startIndex, int beforeCount, int afterCount  ) {
        super.setRange(startIndex, beforeCount, afterCount);
    }

    /**
     * Sets the search expression, and the number of entries before and after
     * to return.
     * @param jumpTo An LDAP search expression defining the result set.
     * return.
     * @param beforeCount The number of results before startIndex to
     * return per page.
     * @param afterCount The number of results after startIndex to
     * return per page.
     */
    public void setRange( String jumpTo, int beforeCount, int afterCount  ) {
        super.setRange(jumpTo, beforeCount, afterCount);
    }

    /**
     * Gets the size of the virtual result set.
     * @return The size of the virtual result set, or -1 if not known.
     */
    public int getIndex() {
        return super.getIndex();
    }

    /**
     * Gets the size of the virtual result set.
     * @return The size of the virtual result set, or -1 if not known.
     */
    public int getListSize() {
        return super.getListSize();
    }

    /**
     * Sets the size of the virtual result set.
     * @param listSize The virtual result set size.
     */
    public void setListSize( int listSize ) {
        super.setListSize(listSize);
    }

    /**
     * Gets the number of results before the top/center to return per page.
     * @return The number of results before the top/center to return per page.
     */
    public int getBeforeCount() {
        return super.getBeforeCount();
    }

    /**
     * Gets the number of results after the top/center to return per page.
     * @return The number of results after the top/center to return per page.
     */
    public int getAfterCount() {
        return super.getAfterCount();
    }
    
    /**
     * Retrieves the ASN.1 BER encoded value of the LDAP control.
     * Null is returned if the value is absent.
     * @return A possibly null byte array representing the ASN.1 BER
     * encoded value of the LDAP control.
     */
    public byte[] getEncodedValue() {
        return getValue();
    }     
}
