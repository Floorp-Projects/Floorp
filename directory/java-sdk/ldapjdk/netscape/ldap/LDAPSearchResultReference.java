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

import netscape.ldap.client.opers.JDAPSearchResultReference;
/**
 * An LDAPSearchResultReference object encapsulates a continuation
 * reference from a search operation.
 * 
 * @version 1.0
 */
public class LDAPSearchResultReference extends LDAPMessage {

    /**
     * A list of LDAP URLs that are referred to.
     */
    private String m_URLs[];
    
    /**
     * Constructor
     * 
     * @param msgid message identifier
     * @param resRef search result reference response
     * @param controls array of controls or null
     * @see netscape.ldap.LDAPEntry
     */
    LDAPSearchResultReference(int msgid, JDAPSearchResultReference resRef, LDAPControl[]controls) {
        super(msgid, resRef, controls);    
        m_URLs = resRef.getUrls();
    }
    
    /**
     * Returns a list of LDAP URLs that are referred to.
     * @return a list of URLs.
     */
    public String[] getUrls() {
        return m_URLs;
    }
}
