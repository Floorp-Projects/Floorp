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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap.client.opers;

import java.util.*;
import org.ietf.ldap.client.*;
import org.ietf.ldap.ber.stream.*;
import java.io.*;
import java.net.*;

/**
 * This class implements the delete request. This object
 * is sent to the ldap server.
 * <pre>
 * DelRequest ::= [APPLICATION 10] LDAPDN
 * </pre>
 *
 * @version 1.0
 */
public class JDAPDeleteRequest extends JDAPBaseDNRequest
    implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected String m_dn = null;

    /**
     * Constructs the delete request.
     * @param dn Distinguished name to delete
     */
    public JDAPDeleteRequest(String dn) {
        m_dn = dn;
    }

    /**
     * Retrieves the protocol operation type.
     * @return operation type
     */
    public int getType() {
        return JDAPProtocolOp.DEL_REQUEST;
    }

    /**
     * Sets the base dn.
     * @param basedn base dn
     */
    public void setBaseDN(String basedn) {
        m_dn = basedn;
    }

    /**
     * Gets the base dn.
     * @return base dn
     */
    public String getBaseDN() {
        return m_dn;
    }

    /**
     * Gets the ber representation of the delete request.
     * @return ber representation
     */
    public BERElement getBERElement() {
        /* Assumed that deleteing "cn=patrick,o=ncware,c=ca"
         * [*] umich-ldap-v3.3
         *     0x4a 0x18  (implicit OctetString)
         *     c n = p a t r i c k , o = n c w a r e , c = c a
         */
        BEROctetString s = new BEROctetString(m_dn);
        BERTag element = new BERTag(BERTag.APPLICATION|10,
          s, true);
        return element;
    }

    /**
     * Retrieves the string representation of the delete request.
     * @return string representation
     */
    public String toString() {
        return "DeleteRequest {entry=" + m_dn + "}";
    }
}
