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
package netscape.ldap;

import netscape.ldap.client.opers.JDAPSearchResponse;

/**
 * A LDAPSearchResult object encapsulates a single search result.
 *
 * @version 1.0
 */
public class LDAPSearchResult extends LDAPMessage {

    /**
     * LDAPEntry 
     */
    private LDAPEntry m_entry;
    
    /**
     * Constructor
     * 
     * @param msgid message identifier
     * @param rsp Search operation response
     * @param controls Array of controls of null
     * @see netscape.ldap.LDAPEntry
     */
    LDAPSearchResult(int msgid, JDAPSearchResponse rsp, LDAPControl[]controls) {
        super(msgid, rsp, controls);
    }
    
    /**
     * Returns the entry of a server search response.
     * @return  An entry returned by the server in response to a search
     * request
     * @see netscape.ldap.LDAPEntry
     */
    public LDAPEntry getEntry() {
        if (m_entry == null) {
            JDAPSearchResponse rsp = (JDAPSearchResponse)getProtocolOp();
            LDAPAttribute[] lattrs = rsp.getAttributes();
            LDAPAttributeSet attrs;
            if ( lattrs != null ) {
                attrs = new LDAPAttributeSet( lattrs );
            }            
            else {
                attrs = new LDAPAttributeSet();
            }
            String dn = rsp.getObjectName();
            m_entry = new LDAPEntry( dn, attrs );
        }
        return m_entry;
    }
}
