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
 * Represents control data for returning paged results from a search.
 *
 *
 *<PRE>
 *      VirtualListViewResponse ::= SEQUENCE {
 *               firstPosition    INTEGER,
 *               contentCount     INTEGER,
 *               virtualListViewResult ENUMERATED {
 *                 success                  (0),
 *                 unwillingToPerform       (53),
 *                 insufficientAccessRights (50),
 *                 operationsError          (1),
 *                 busy                     (51),
 *                 timeLimitExceeded        (3),
 *                 adminLimitExceeded       (11),
 *                 sortControlMissing       (60),
 *                 indexRangeError          (?),
 *               }
 *     }
 *</PRE>
 */

public class LdapVirtualListResponseControl extends LDAPVirtualListResponse implements Control{

    /**
     * Constructs a new <CODE>LDAPVirtualListResponse</CODE> object.
     * @param value A BER encoded byte array.
     * This constructor is used by the NetscapeControlFactory
     */
    LdapVirtualListResponseControl(boolean critical, byte[] value)throws Exception  {
        super(VIRTUALLISTRESPONSE, critical, value);
    }

    /**
     * Gets the size of the virtual result set.
     * @return The size of the virtual result set, or -1 if not known.
     */
    public int getContentCount() {
        return super.getContentCount();
    }

    /**
     * Gets the index of the first entry returned.
     * @return The index of the first entry returned.
     */
    public int getFirstPosition() {
        return super.getFirstPosition();
    }

    /**
     * Gets the result code.
     * @return The result code.
     */
    public int getResultCode() {
        return super.getResultCode();
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
