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
import java.net.*;

/**
 * This class implements the modify request.
 * <pre>
 *   ModifyRequest ::= [APPLICATION 6] SEQUENCE {
 *     object LDAPDN,
 *     modification SEQUENCE OF SEQUENCE {
 *       operation ENUMERATED {
 *         add (0),
 *         delete (1),
 *         replace (2)
 *       },
 *       modification SEQUENCE {
 *         type AttributeType,
 *         values SET OF AttributeValue
 *       }
 *     }
 *   }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPModifyRequest extends JDAPBaseDNRequest
    implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected String m_dn = null;
    protected LDAPModification m_mod[] = null;

    /**
     * Constructs the modify request
     * @param dn distinguished name of modifying
     * @param mod list of modifications
     */
    public JDAPModifyRequest(String dn, LDAPModification mod[]) {
        m_dn = dn;
        m_mod = mod;
    }

    /**
     * Retrieves protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.MODIFY_REQUEST;
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
     * Gets the ber representation of modify request.
     * @return ber representation of modify request
     */
    public BERElement getBERElement() {
        BERSequence seq = new BERSequence();
        seq.addElement(new BEROctetString(m_dn));
        BERSequence mod_list = new BERSequence();
        for (int i = 0; i < m_mod.length; i++) {
            mod_list.addElement(m_mod[i].getBERElement());
        }
        seq.addElement(mod_list);
        BERTag element = new BERTag(BERTag.APPLICATION|BERTag.CONSTRUCTED|6,
          seq, true);
        return element;
    }

    /**
     * Retrieves string representation of modify request.
     * @return string representation of request
     */
    public String toString() {
        String s = "";
        for (int i = 0; i < m_mod.length; i++) {
            if (i != 0)
                s = s + "+";
            s = s + m_mod[i].toString();
        }
        return "ModifyRequest {object=" + m_dn + ", modification=" + s + "}";
    }
}
