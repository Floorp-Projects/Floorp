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

import javax.naming.NamingException;
import javax.naming.ldap.Control;
import netscape.ldap.LDAPControl;
import netscape.ldap.LDAPException;
import netscape.ldap.controls.*;
import com.netscape.jndi.ldap.common.ExceptionMapper;

/**
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
 * @see com.netscape.jndi.ldap.LdapSortControl
 */
public class LdapSortResponseControl extends LDAPSortControl implements Control {

    /**
     * Constructs a new <CODE>LdapEntryChangeControl</CODE> object.
     * This constructor is used by the NetscapeControlFactory
     *
     */
    LdapSortResponseControl(boolean critical, byte[] value) throws Exception{
        super(SORTRESPONSE, critical, value);
    }
    
    /**
     * Get the first attribute type from the sort key list that 
     * resulted in an error
     * @return Attribute name that resulted in an error
     */
    public String getFailedAttribute() {
        return super.getFailedAttribute();
    }
    
    /**
     * Return the sort result code
     * @return The sort result code
     */
    public int getResultCode() {
        return super.getResultCode();
    }

    /**
     * Return corresponding NamingException for the sort error code
     * @return NamingException for the sort error code
     */
    public NamingException getSortException() {
        if (getResultCode() == 0) { // success
            return null;
        }
        return ExceptionMapper.getNamingException(
            new LDAPException("Server Sort Failed", getResultCode()));
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

