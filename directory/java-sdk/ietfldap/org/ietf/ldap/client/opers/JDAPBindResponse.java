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

/**
 * This class implements the bind response. This object
 * is sent from the ldap server to the interface.
 * <pre>
 * BindResponse = [APPLICATION 1] LDAPResult
 * </pre>
 *
 * Note that LDAPv3.0 Bind Response is structured as follows:
 * <pre>
 * BindResponse ::= [APPLICATION 1] SEQUENCE {
 *   COMPONENTS OF LDAPResult,
 *   serverCreds [7] SaslCredentials OPTIONAL
 * }
 * </pre>
 *
 */
public class JDAPBindResponse extends JDAPResult implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected byte[] m_credentials = null;

    /**
     * Constructs bind response.
     * @param element ber element of bind response
     */
    public JDAPBindResponse(BERElement element) throws IOException {
        super(((BERTag)element).getValue());
        /* LDAPv3 Sasl Credentials support */
        BERSequence s = (BERSequence)((BERTag)element).getValue();
        if (s.size() <= 3) {
            return;
        }
        BERElement e = s.elementAt(3);
        if (e.getType() == BERElement.TAG) {
			BERElement el = ((BERTag)e).getValue();
			if (el instanceof BERSequence) {
				el = ((BERSequence)el).elementAt(0);
			}
            BEROctetString str = (BEROctetString)el;
            try{
                m_credentials = str.getValue();
            } catch(Exception ex)
            {}
        }
    }

    /**
     * Retrieves Sasl Credentials. LDAPv3 support.
     * @return credentials
     */
    public byte[] getCredentials() {
        return m_credentials;
    }

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.BIND_RESPONSE;
    }

    /**
     * Retrieve the string representation.
     * @return string representation
     */
    public String toString() {
        StringBuffer sb = new StringBuffer("BindResponse {resultCode=");
        sb.append(m_result_code);
        sb.append(", serverSaslCreds=" +
                  ((m_credentials == null) ? "null" : /*"********"*/new String(m_credentials)));
        if (m_matched_dn != null) {
            sb.append(", matchedDN=");
            sb.append(m_matched_dn);
        }
        if (m_error_message != null) {
            sb.append(", errorMessage=");
            sb.append(m_error_message);
        }
        if (m_referrals != null && m_referrals.length > 0) {
            sb.append(", referrals=");
            for (int i=0; i < m_referrals.length; i++) {
                sb.append((i==0 ? "" : " "));
                sb.append(m_referrals[i]);
            }
        }
        sb.append("}");
        return sb.toString();
    }
}
