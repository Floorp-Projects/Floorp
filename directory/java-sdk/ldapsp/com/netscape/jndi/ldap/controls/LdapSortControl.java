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
 * To specify the sort order, you construct an <CODE>LDAPSortKey</CODE>
 * object and pass it to the <CODE>LDAPSortControl</CODE> constructor.
 * The <CODE>LDAPSortKey</CODE> object represents a list of the attribute
 * types used for sorting (a "sort key list").
 * <P>
 *
 * You can include the control in a search request by constructing
 * an <CODE>LDAPSearchConstraints</CODE> object and calling the
 * <CODE>setServerControls</CODE> method.  You can then pass this
 * <CODE>LDAPSearchConstraints</CODE> object to the <CODE>search</CODE>
 * method of an <CODE>LDAPConnection</CODE> object.
 * <P>
 *
 * For example:
 * <PRE>
 * ...
 * LDAPConnection ld = new LDAPConnection();
 * try {
 *     // Connect to server.
 *     ld.connect( 3, hostname, portnumber, "", "" );
 *
 *     // Create a sort key that specifies the sort order.
 *     LDAPSortKey sortOrder = new LDAPSortKey( attrname );
 *
 *     // Create a "critical" server control using that sort key.
 *     LDAPSortControl sortCtrl = new LDAPSortControl(sortOrder,true);
 *
 *     // Create search constraints to use that control.
 *     LDAPSearchConstraints cons = new LDAPSearchConstraints();
 *     cons.setServerControls( sortCtrl );
 *
 *     // Send the search request.
 *     LDAPSearchResults res = ld.search( "o=Airius.com",
 *        LDAPv3.SCOPE_SUB, "(cn=Barbara*)", null, false, cons );
 *
 *     ...
 *
 * </PRE>
 *
 * The LDAP server sends back a sort response control to indicate
 * the result of the sorting operation. (The OID for this control
 * is 1.2.840.113556.1.4.474.)
 * <P>
 *
 * This control contains:
 * <P>
 *
 * <UL>
 * <LI>the result code from the sorting operation
 * <LI>optionally, the first attribute type in the sort key list
 * that resulted in an error (for example, if the attribute does
 * not exist)
 * </UL>
 * <P>
 *
 * To parse this control, use the <CODE>parseResponse</CODE> method.
 * <P>
 *
 * The following table lists what kinds of results to expect from the
 * LDAP server under different situations:
 * <P>
 *
 * <TABLE BORDER=1 COLS=4>
 * <TR VALIGN=BASELINE>
 *     <TH>Does the Server Support the Sorting Control?</TH>
 *     <TH>Is the Sorting Control Marked As Critical?</TH>
 *     <TH>Other Conditions</TH>
 *     <TH>Results from LDAP Server</TH>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD ROWSPAN=2>
 *     No
 *     </TD>
 *     <TD>
 *     Yes
 *     </TD>
 *     <TD>
 *     None
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server does not send back any entries.
 *     <LI>An LDAPException.UNAVAILABLE_CRITICAL_EXTENSION
 *     exception is thrown.
 *     </UL>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD>
 *     No
 *     </TD>
 *     <TD>
 *     None
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server ignores the sorting control and
 *     returns the entries unsorted.
 *     </UL>
 *     <P>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD ROWSPAN=4>
 *     Yes
 *     </TD>
 *     <TD>
 *     Yes
 *     </TD>
 *     <TD ROWSPAN=2>
 *     The server cannot sort the results using the specified
 *     sort key list.
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server does not send back any entries.
 *     <LI>An LDAPException.UNAVAILABLE_CRITICAL_EXTENSION
 *     exception is thrown.
 *     <LI>The server sends back the sorting response control, which
 *     specifies the result code of the sort attempt and (optionally)
 *     the attribute type that caused the error.
 *     </UL>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD>
 *     No
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server returns the entries unsorted.
 *     <LI>The server sends back the sorting response control, which
 *     specifies the result code of the sort attempt and (optionally)
 *     the attribute type that caused the error.
 *     </UL>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD ROWSPAN=2>
 *     N/A (could either be marked as critical or not)
 *     </TD>
 *     <TD>
 *     The server successfully sorted the entries.
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server sends back the sorted entries.
 *     <LI>The server sends back the sorting response control, which
 *     specifies the result code of the sort attempt
 *     (LDAPException.SUCCESS).
 *     </UL>
 *     </TD>
 * </TR>
 * <TR VALIGN=BASELINE>
 *     <TD>
 *     The search itself failed (for any reason).
 *     </TD>
 *     <TD>
 *     <UL>
 *     <LI>The server sends back a result code for the search
 *     operation.
 *     <LI>The server does not send back the sorting response control.
 *     </UL>
 *     </TD>
 * </TR>
 * </TABLE>
 * <P>
 * @see com.netscape.jndi.ldap.LdapSortKey
 * @see com.netscape.jndi.ldap.LdapSearchConstraints
 * @see com.netscape.jndi.ldap.LdapSearchConstraints#setServerControls(LDAPControl)
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

    public LdapSortControl(String[] keys,
                           boolean critical) {
        super(toSortKey(keys), critical);
    }


    /**
     * Implements Control interface
     */
    public byte[] getEncodedValue() {
        return getValue();
    }    
}

