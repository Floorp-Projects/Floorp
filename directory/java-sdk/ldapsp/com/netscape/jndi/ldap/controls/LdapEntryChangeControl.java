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
package com.netscape.jndi.ldap.controls;

import javax.naming.ldap.Control;
import netscape.ldap.controls.*;

/**
 * Represents an LDAP v3 server control that specifies information
 * about a change to an entry in the directory.  (The OID for this
 * control is 2.16.840.1.113730.3.4.7.)  You need to use this control in
 * conjunction with a "persistent search" control (represented
 * by <CODE>LdapPersistentSearchControl</CODE> object.
 * <P>
 *
 * To use persistent searching for change notification, you create a
 * "persistent search" control that specifies the types of changes that
 * you want to track.  When an entry is changed, the server sends that
 * entry back to your client and may include an "entry change notification"
 * control that specifies additional information about the change.
 * <P>
 *
 * Once you retrieve an <CODE>LdapEntryChangeControl</CODE> object from
 * the server, you can get the following additional information about
 * the change made to the entry:
 * <P>
 *
 * <UL>
 * <LI>The type of change made (add, modify, delete, or modify DN)
 * <LI>The change number identifying the record of the change in the
 * change log (if the server supports change logs)
 * <LI>If the entry was renamed, the old DN of the entry
 * </UL>
 * <P>
 *
 * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl
 */
public class LdapEntryChangeControl extends LDAPEntryChangeControl implements Control {

    /**
     * Constructs a new <CODE>LdapEntryChangeControl</CODE> object.
     * This constructor is used by the NetscapeControlFactory
     *
     */
    LdapEntryChangeControl(boolean critical, byte[] value) throws Exception {
        super(ENTRYCHANGED, critical, value);
    }

    /**
     * Gets the change number, which identifies the record of the change
     * in the server's change log.
     * @return Change number identifying the change made.
     */
    public int getChangeNumber() {
        return super.getChangeNumber();
    }

    /**
     * Gets the change type, which identifies the type of change
     * that occurred.
     * @returns Change type identifying the type of change that
     * occurred.  This can be one of the following values:
     * <P>
     *
     * <UL>
     * <LI><CODE>LdapPersistSearchControl.ADD</CODE> (a new entry was
     * added to the directory)
     * <LI><CODE>LdapPersistSearchControl.DELETE</CODE> (an entry was
     * removed from the directory)
     * <LI><CODE>LdapPersistSearchControl.MODIFY</CODE> (an entry was
     * modified)
     * <LI><CODE>LdapPersistSearchControl.MODDN</CODE> (an entry was
     * renamed)
     * </UL>
     * <P>
     */
    public int getChangeType() {
        return super.getChangeType();
    }

    /**
     * Gets the previous DN of the entry (if the entry was renamed).
     * @returns The previous distinguished name of the entry.
     */
    public String getPreviousDN() {
        return super.getPreviousDN();
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

