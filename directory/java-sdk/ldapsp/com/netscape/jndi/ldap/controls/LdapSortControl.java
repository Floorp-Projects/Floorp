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
 * Represents an LDAP v3 server control that specifies that you want
 * the server to return sorted search results.  (The OID for this
 * control is 1.2.840.113556.1.4.473.)
 * <P>
 *
 * When constructing an <CODE>LDAPSortControl</CODE> object, you can
 * specify the order in which you want the results sorted.
 * You can also specify whether or not this control is critical
 * to the search operation.
 * <P>
 *
 * To specify the sort order, you construct an <CODE>LdapSortKey</CODE>
 * object and pass it to the <CODE>LdapSortControl</CODE> constructor.
 * The <CODE>LdapSortKey</CODE> object represents a list of the attribute
 * types used for sorting (a "sort key list").
 *
 * The LDAP server sends back a sort response control to indicate
 * the result of the sorting operation. (The OID for this control
 * is 1.2.840.113556.1.4.474.)
 *
 * @see com.netscape.jndi.ldap.LdapSortKey
 * @see com.netscape.jndi.ldap.LdapSortResponseControl
 */
public class LdapSortControl extends LDAPSortControl implements Control{
    /**
     * Constructs an <CODE>LDAPSortControl</CODE> object with a single
     * sorting key.
     * @param key A single attribute to sort by.
     * @param critical <CODE>true</CODE> if the LDAP operation should be
     * discarded when the server does not support this control (in other
     * words, this control is critical to the LDAP operation).
     * @see com.netscape.jndi.ldap.LdapSortKey
     */
    public LdapSortControl(LdapSortKey key,
                           boolean critical) {
        super (key, critical);
    }

    /**
     * Constructs an <CODE>LDAPSortControl</CODE> object with an array of
     * sorting keys.
     * @param keys The attributes to sort by.
     * @param critical <CODE>true</CODE> if the LDAP operation should be
     * discarded when the server does not support this control (in other
     * words, this control is critical to the LDAP operation).
     * @see com.netscape.jndi.ldap.LdapSortKey
     */
    public LdapSortControl(LdapSortKey[] keys,
                           boolean critical) {
        super(keys, critical);
    }


    static LdapSortKey[] toSortKey(String[] keysIn) {
        LdapSortKey[] keysOut = new LdapSortKey[keysIn.length];
        for (int i=0; i < keysIn.length; i++) {
            keysOut[i] = new LdapSortKey(keysIn[i]);
        }
        return keysOut;
    }

    /**
     * Constructs an <CODE>LDAPSortControl</CODE> object with an array of
     * sorting keys.
     * @param keys The attributes to sort by.
     * @param critical <CODE>true</CODE> if the LDAP operation should be
     * discarded when the server does not support this control (in other
     * words, this control is critical to the LDAP operation).
     * @see com.netscape.jndi.ldap.LdapSortKey
     */
    public LdapSortControl(String[] keys,
                           boolean critical) {
        super(toSortKey(keys), critical);
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

