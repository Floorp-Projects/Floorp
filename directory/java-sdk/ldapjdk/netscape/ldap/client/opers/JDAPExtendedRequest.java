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
package netscape.ldap.client.opers;

import java.util.*;
import netscape.ldap.client.*;
import netscape.ldap.ber.stream.*;
import java.io.*;

/**
 * This class implements the ExtendedRequest request. This object
 * is sent to the ldap server and is a v3 request.
 * <pre>
 * ExtendedRequest ::= [APPLICATION 23] SEQUENCE {
 *   requestName  [0] LDAPOID,
 *   requestValue [1] OCTET STRING OPTIONAL
 * }
 * </pre>
 *
 * @version 1.0
 * @see RFC1777
 */
public class JDAPExtendedRequest implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected String m_oid = null;
    protected byte m_value[] = null;

    /**
     * Constructs extended request.
     * @param oid object identifier
     * @param value request value
     */
    public JDAPExtendedRequest(String oid, byte value[]) {
        m_oid = oid;
        m_value = value;
    }

    /**
     * Retrieves protocol operation type.
     * @return protcol type
     */
    public int getType() {
        return JDAPProtocolOp.EXTENDED_REQUEST;
    }

    /**
     * Gets the ber representation of extended request.
     * @return ber representation of request
     */
    public BERElement getBERElement() {
        BERSequence seq = new BERSequence();
        seq.addElement(new BERTag(BERTag.CONTEXT|0,
          new BEROctetString (m_oid), true));
        if (m_value != null) {
            seq.addElement(new BERTag(BERTag.CONTEXT|1,
              new BEROctetString (m_value, 0, m_value.length), true));
        }
        BERTag element = new BERTag(BERTag.APPLICATION|BERTag.CONSTRUCTED|23,
          seq, true);
        return element;
    }

    /**
     * Retrieves the string representation of add request parameters.
     * @return string representation of add request parameters
     */
    public String getParamString() {
        String s = "";
        if (m_value != null) {
            try{
                s = new String(m_value, "UTF8");
            } catch(Throwable x) {}
        }
        return "{OID='" + m_oid + "', value='" + s + "'}";
    }

    /**
     * Retrieves the string representation of add request.
     * @return string representation of add request
     */
    public String toString() {
        return "ExtendedRequest " + getParamString();
    }
}
