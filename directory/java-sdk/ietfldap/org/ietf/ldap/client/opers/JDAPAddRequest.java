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
import org.ietf.ldap.*;
import org.ietf.ldap.client.*;
import org.ietf.ldap.ber.stream.*;
import java.io.*;

/**
 * This class implements the add request. This object
 * is sent to the ldap server.
 * <P>See RFC 1777.
 *
 * <pre>
 * AddRequest ::= [APPLICATION 8] SEQUENCE {
 *   entry LDAPDN,
 *   attrs SEQUENCE OF SEQUENCE {
 *     type AttributeType,
 *     values SET OF AttributeValue
 *   }
 * }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPAddRequest extends JDAPBaseDNRequest
    implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected String m_dn = null;
    protected LDAPAttribute m_attrs[] = null;

    /**
     * Constructs add request.
     * @param dn distinguished name of adding entry
     * @param attrs list of attribute associated with entry
     */
    public JDAPAddRequest(String dn, LDAPAttribute attrs[]) {
        m_dn = dn;
        m_attrs = attrs;
    }

    /**
     * Retrieves protocol operation type.
     * @return protcol type
     */
    public int getType() {
        return JDAPProtocolOp.ADD_REQUEST;
    }

    /**
     * Sets the base dn component.
     * @param basedn base dn
     */
    public void setBaseDN(String basedn) {
        m_dn = basedn;
    }

    /**
     * Gets the base dn component.
     * @return base dn
     */
    public String getBaseDN() {
        return m_dn;
    }

    /**
     * Gets the ber representation of add request.
     * @return ber representation of request
     */
    public BERElement getBERElement() {
        /* Assumed that adding cn=patrick,o=ncware,c=ca with
         * following attributes:
         *     cn: patrick
         *     title: programmer
         * [*] umich-ldap-v3.3:
         *     0x68 0x46      ([APPLICATION8])
         *     0x04 0x1a c n = p a t r i c k , 0x20
         *               o = n c w a r e , 0x20 c =
         *               c a  (entry - OctetString)
         *     0x30 0x28      (SEQUENCE)
         *     0x30 0x0f      (SEQUENCE)
         *     0x04 0x02 c n  (attribute type - OctetString)
         *     0x31 0x09      (SET OF)
         *     0x04 0x07 p a t r i c k (attribute value - OctetString)
         *     0x30 0x15
         *     0x04 0x05 t i t l e
         *     0x31 0x0c      (SET OF)
         *     0x04 0x0a p r o g r a m m e r
         */
        BERSequence seq = new BERSequence();
        seq.addElement(new BEROctetString (m_dn));
        BERSequence attrs_list = new BERSequence();
        for (int i = 0; i < m_attrs.length; i++) {
            attrs_list.addElement(m_attrs[i].getBERElement());
        }
        seq.addElement(attrs_list);
        BERTag element = new BERTag(BERTag.APPLICATION|BERTag.CONSTRUCTED|8,
          seq, true);
        return element;
    }

    /**
     * Retrieves the string representation of add request parameters.
     * @return string representation of add request parameters
     */
    public String getParamString() {
        String s = "";
        for (int i = 0; i < m_attrs.length; i++) {
            if (i != 0)
                s = s + " ";
            s = s + m_attrs[i].toString();
        }
        return "{entry='" + m_dn + "', attrs='" + s + "'}";
    }

    /**
     * Retrieves the string representation of add request.
     * @return string representation of add request
     */
    public String toString() {
        return "AddRequest " + getParamString();
    }
}
