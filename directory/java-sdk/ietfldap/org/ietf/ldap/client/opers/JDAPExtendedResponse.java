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
 * This class implements the extended response. This object
 * is sent from the ldap server to the interface and is
 * a v3 response.
 * <P>See RFC 1777.
 *
 * <pre>
 * ExtendedResponse :: [APPLICATION 23] SEQUENCE {
 *   COMPONENTS OF LDAPResult,
 *   responseName [10] LDAPOID OPTIONAL,
 *   response     [11] OCTET STRING OPTIONAL
 * }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPExtendedResponse extends JDAPResult
    implements JDAPProtocolOp {

    /**
     * Internal variables
     */
    protected String m_oid = null;
    protected byte[] m_value = null;

    /**
     * Constructs extended response.
     * @param element ber element of add response
     */
    public JDAPExtendedResponse(BERElement element) throws IOException {
        super(((BERTag)element).getValue());
        BERSequence seq = (BERSequence)((BERTag)element).getValue();
        for (int i = 0; i < seq.size(); i++) {
            try {
                BERElement el = seq.elementAt(i);
                if (el.getType() != BERElement.TAG)
                    continue;
                BERTag t = (BERTag)el;
                switch (t.getTag()&0x0f) {
                    case 10:
                        BEROctetString oid = (BEROctetString)t.getValue();
                        try{
                            m_oid = new String(oid.getValue(), "UTF8");
                        } catch(Throwable x)
                        {}
                        break;
                    case 11:
                        BEROctetString value = (BEROctetString)t.getValue();
                        m_value = value.getValue();
                        break;
                }
            } catch (ClassCastException e) {
            }
        }
    }

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.EXTENDED_RESPONSE;
    }

    /**
     * Retrieves the results of the extended operation.
     * @return extended operation results as byte array
     */
    public byte[] getValue() {
        return m_value;
    }

    /**
     * Retrieves the oid of the extended operation.
     * @return extended operation oid.
     */
    public String getID() {
        return m_oid;
    }

    /**
     * Retrieve the string representation.
     * @return string representation
     */
    public String toString() {
        return "ExtendedResponse " + super.getParamString();
    }
}
