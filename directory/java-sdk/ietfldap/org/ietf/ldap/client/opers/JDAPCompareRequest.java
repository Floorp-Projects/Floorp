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
 * This class implements the compare request. This object
 * is sent to the ldap server.
 * <pre>
 *   CompareRequest ::= [APPLICATION 14] SEQUENCE {
 *     entry LDAPDN,
 *     ava AttributeValueAssertion
 *   }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPCompareRequest extends JDAPBaseDNRequest
    implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected String m_dn = null;
    protected JDAPAVA m_ava = null;

    /**
     * Constructs the compare request.
     * @param dn distinguished name of the entry to be compared
     * @param ava attribut value assertion
     */
    public JDAPCompareRequest(String dn, JDAPAVA ava) {
        m_dn = dn;
        m_ava = ava;
    }

    /**
     * Retrieves the protocol operation type.
     * @return operation type
     */
    public int getType() {
        return JDAPProtocolOp.COMPARE_REQUEST;
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
     * Retrieves the ber representation of the request.
     * @return ber request
     */
    public BERElement getBERElement() {
        BERSequence seq = new BERSequence();
        seq.addElement(new BEROctetString(m_dn));
        seq.addElement(m_ava.getBERElement());
        BERTag element = new BERTag(BERTag.APPLICATION|BERTag.CONSTRUCTED|14,
          seq, true);
        return element;
    }

    /**
     * Retrieves the string representation of the request.
     * @return string representation
     */
    public String toString() {
        return "CompareRequest {entry=" + m_dn + ", ava=" + m_ava.toString() +
          "}";
    }
}
