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
 * This class implements the modify rdn request. This
 * object is sent to the ldap server.
 * <pre>
 * ModifyRDNRequest ::= [APPLICATION 12] SEQUENCE {
 *   entry LDAPDN,
 *   newrdn RelativeLDAPDN,
 *   deleteoldrdn BOOLEAN
 * }
 * </pre>
 *
 * Note that LDAPv3 rename this object to JDAPModifyDNRequest
 * and has the following defintion:
 * <pre>
 * ModifyDNRequest ::= [APPLICATION 12] SEQUENCE {
 *   entry LDAPDN,
 *   newrdn RelativeLDAPDN,
 *   deleteoldrdn BOOLEAN,
 *   newSuperior [0] LDAPDN OPTIONAL
 * }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPModifyRDNRequest extends JDAPBaseDNRequest
    implements JDAPProtocolOp {
    /**
     * Internal variables
     */
    protected String m_old_dn = null;
    protected String m_new_rdn = null;
    protected boolean m_delete_old_dn;
    protected String m_new_superior;

    /**
     * Constructs modify RDN request.
     * @param old_dn old distinguished name
     * @param new_dn new distinguished name
     * @param delete_old_dn delete the old distinguished name
     */
    public JDAPModifyRDNRequest(String old_dn, String new_rdn,
      boolean delete_old_dn) {
        m_old_dn = old_dn;
        m_new_rdn = new_rdn;
        m_delete_old_dn = delete_old_dn;
        m_new_superior = null;
    }

    /**
     * Constructs modify DN request.
     * @param old_dn old distinguished name
     * @param new_dn new distinguished name
     * @param delete_old_dn delete the old distinguished name
     * @param new_superior parent dn
     */
    public JDAPModifyRDNRequest(String old_dn, String new_rdn,
      boolean delete_old_dn, String new_superior) {
        m_old_dn = old_dn;
        m_new_rdn = new_rdn;
        m_delete_old_dn = delete_old_dn;
        m_new_superior = new_superior;
    }

    /**
     * Retrieves the protocol operation type.
     * @return protocol type
     */
    public int getType() {
        return JDAPProtocolOp.MODIFY_RDN_REQUEST;
    }

    /**
     * Sets the base dn.
     * @param basedn base dn
     */
    public void setBaseDN(String basedn) {
        m_old_dn = basedn;
    }

    /**
     * Gets the base dn component.
     * @return base dn
     */
    public String getBaseDN() {
        return m_old_dn;
    }

    /**
     * Gets the ber representation of the request.
     * @return ber representation
     */
    public BERElement getBERElement() {
        BERSequence seq = new BERSequence();
        seq.addElement(new BEROctetString(m_old_dn));
        seq.addElement(new BEROctetString(m_new_rdn));
        seq.addElement(new BERBoolean(m_delete_old_dn));
        /* LDAPv3 new parent dn feature support */
        if (m_new_superior != null)
            seq.addElement(new BERTag(BERTag.CONTEXT|0,
              new BEROctetString (m_new_superior), true));
        BERTag element = new BERTag(BERTag.APPLICATION|BERTag.CONSTRUCTED|12,
          seq, true);
        return element;
    }

    /**
     * Gets the string representation of the request.
     * @return string representation
     */
    public String toString() {
        return "ModifyRDNRequest {entry=" + m_old_dn + ", newrdn=" +
          m_new_rdn + ", deleteoldrdn=" + m_delete_old_dn + "}";
    }
}
