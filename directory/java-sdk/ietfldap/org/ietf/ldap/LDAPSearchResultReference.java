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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import org.ietf.ldap.client.opers.JDAPSearchResultReference;
/**
 * An LDAPSearchResultReference object encapsulates a continuation
 * reference from a search operation.
 * 
 * @version 1.0
 */
public class LDAPSearchResultReference extends LDAPMessage {

    static final long serialVersionUID = -7816778029315223117L;

    /**
     * A list of LDAP URLs that are referred to.
     */
    private String _URLs[];
    
    /**
     * Constructor
     * 
     * @param msgid message identifier
     * @param resRef search result reference response
     * @param controls array of controls or null
     * @see org.ietf.ldap.LDAPEntry
     */
    LDAPSearchResultReference( int msgid,
                               JDAPSearchResultReference resRef,
                               LDAPControl[]controls ) {
        super( msgid, resRef, controls );
        _URLs = resRef.getUrls();
    }
    
    /**
     * Returns a list of LDAP URLs that are referred to
     *
     * @return a list of URLs.
     */
    public String[] getReferrals() {
        return _URLs;
    }
}
