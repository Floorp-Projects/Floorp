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
 * Represents an LDAP v3 server control that specifies a persistent
 * search (an ongoing search operation), which allows your LDAP client
 * to get notification of changes to the directory.  (The OID for this
 * control is 2.16.840.1.113730.3.4.3.)  You can use this control in
 * conjunction with an "entry change notification" control.
 * <P>
 *
 * To use persistent searching for change notification, you create a
 * "persistent search" control that specifies the types of changes that
 * you want to track.  You include the control in a search request.
 * If an entry in the directory is changed, the server determines if
 * the entry matches the search criteria in your request and if the
 * change is the type of change that you are tracking.  If both of
 * these are true, the server sends the entry to your client.
 * <P>
 *
 * The server can also include an "entry change notification" control
 * with the entry.  (The OID for this control is 2.16.840.1.113730.3.4.7.)
 * This control contains additional information about the
 * change made to the entry, including the type of change made,
 * the change number (which corresponds to an item in the server's
 * change log, if the server supports a change log), and, if the
 * entry was renamed, the old DN of the entry.
 * <P>
 *
 * When constructing an <CODE>LDAPPersistSearchControl</CODE> object,
 * you can specify the following information:
 * <P>
 *
 * <UL>
 * <LI>the type of change you want to track (added, modified, deleted,
 * or renamed entries)
 * <LI>a preference indicating whether or not you want the server to
 * return all entries that initially matched the search criteria
 * (rather than only the entries that change)
 * <LI>a preference indicating whether or not you want entry change
 * notification controls included with every entry returned by the
 * server
 * </UL>
 * <P>
 * @see com.netscape.jndi.ldap.controls.LdapEntryChangeControl
 */

public class LdapPersistSearchControl extends LDAPPersistSearchControl implements Control {

    /**
     * Change type specifying that you want to track additions of new
     * entries to the directory.  You can either specify this change type
     * when constructing an <CODE>LdapPersistSearchControl</CODE> or
     * by using the <CODE>setChangeTypes</CODE> method.
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#getChangeTypes
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#setChangeTypes
     */
    public static final int ADD = LDAPPersistSearchControl.ADD;

    /**
     * Change type specifying that you want to track removals of
     * entries from the directory.  You can either specify this change type
     * when constructing an <CODE>LdapPersistSearchControl</CODE> or
     * by using the <CODE>setChangeTypes</CODE> method.
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#getChangeTypes
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#setChangeTypes
     */
    public static final int DELETE = LDAPPersistSearchControl.DELETE;

    /**
     * Change type specifying that you want to track modifications of
     * entries in the directory.  You can either specify this change type
     * when constructing an <CODE>LdapPersistSearchControl</CODE> or
     * by using the <CODE>setChangeTypes</CODE> method.
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#getChangeTypes
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#setChangeTypes
     */
    public static final int MODIFY = LDAPPersistSearchControl.MODIFY;

    /**
     * Change type specifying that you want to track modifications of the
     * DNs of entries in the directory.  You can either specify this change type
     * when constructing an <CODE>LdapPersistSearchControl</CODE> or
     * by using the <CODE>setChangeTypes</CODE> method.
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#getChangeTypes
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#setChangeTypes
     */
    public static final int MODDN = LDAPPersistSearchControl.MODDN;
    
    /**
     * Constructs an <CODE>LdapPersistSearchControl</CODE> object
     * that specifies a persistent search.
     *
     * @param changeTypes The change types to be monitored. You can perform
     * a bitwise OR on any of the following values and specify the result as
     * the change types:
     * <UL>
     * <LI><CODE>LdapPersistSearchControl.ADD</CODE> (to track new entries
     * added to the directory)
     * <LI><CODE>LdapPersistSearchControl.DELETE</CODE> (to track entries
     * removed from the directory)
     * <LI><CODE>LdapPersistSearchControl.MODIFY</CODE> (to track entries
     * that have been modified)
     * <LI><CODE>LdapPersistSearchControl.MODDN</CODE> (to track entries
     * that have been renamed)
     * </UL>
     * @param changesOnly <code>true</code> if you do not want the server
     * to return all existing entries in the directory that match the
     * search criteria.  (You just want the changed entries to be returned.)
     * @param returnControls <code>true</code> you want the server to return
     * entry change controls with each entry in the search results.
     * @param isCritical <code>true</code> if this control is critical to
     * the search operation (for example, if the server does not support
     * this control, you may not want the server to perform the search
     * at all.)
     * @see com.netscape.jndi.ldap.controls.LdapEntryChangeControl
     */
    public LdapPersistSearchControl(int changeTypes, boolean changesOnly,
        boolean returnControls, boolean isCritical) {
        super(changeTypes, changesOnly, returnControls, isCritical);
    }

    /**
     * Gets the change types monitored by this control.
     * @return Integer representing the change types that you want monitored.
     * This value can be the bitwise OR of <code>ADD, DELETE, MODIFY,</code>
     * and/or <code>MODDN</code>. If the change type is unknown,
     * this method returns -1.
     */
    public int getChangeTypes() {
        return super.getChangeTypes();
    }

    /**
     * Indicates whether you want the server to send any existing
     * entries that already match the search criteria or only the
     * entries that have changed.
     * @return If <code>true</code>, the server returns only the
     * entries that have changed.  If <code>false</code>, the server
     * also returns any existing entries that match the search criteria
     * but have not changed.
     */
    public boolean getChangesOnly() {
        return super.getChangesOnly();
    }

    /**
     * Indicates whether or not the server includes an "entry change
     * notification" control with each entry it sends back to the client
     * during the persistent search.
     * @return <code>true</code> if the server includes "entry change
     * notification" controls with the entries it sends during the
     * persistent search.
     * @see com.netscape.jndi.ldap.controls.LdapEntryChangeControl
     */
    public boolean getReturnControls() {
        return super.getReturnControls();
    }

    /**
     * Sets the change types that you want monitored by this control.
     * @param types Integer representing the change types that you want monitored.
     * This value can be the bitwise OR of <code>ADD, DELETE, MODIFY,</code>
     * and/or <code>MODDN</code>.
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#getChangeTypes
     */
    public void setChangeTypes(int types) {
        super.setChangeTypes(types);
    }

    /**
     * Specifies whether you want the server to send any existing
     * entries that already match the search criteria or only the
     * entries that have changed.
     * @param changesOnly If <code>true</code>, the server returns only the
     * entries that have changed.  If <code>false</code>, the server
     * also returns any existing entries that match the search criteria
     * but have not changed.
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#getChangesOnly
     */
    public void setChangesOnly(boolean changesOnly) {
        super.setChangesOnly(changesOnly);
    }

    /**
     * Specifies whether you want the server to include an "entry change
     * notification" control with each entry it sends back to the client
     * during the persistent search.
     * @param returnControls If <code>true</code>, the server includes
     * "entry change notification" controls with the entries it sends
     * during the persistent search.
     * @see com.netscape.jndi.ldap.controls.LdapEntryChangeControl
     * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl#setReturnControls
     */
    public void setReturnControls(boolean returnControls) {
        super.setReturnControls(returnControls);
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
