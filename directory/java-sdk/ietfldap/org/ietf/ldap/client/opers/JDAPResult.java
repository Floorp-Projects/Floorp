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
import org.ietf.ldap.ber.stream.*;
import java.io.*;
import java.net.*;

/**
 * This class implements the ldap result where stores
 * the request status. It is the base class for all
 * the response except search response. This object is
 * sent from the server to the client interface.
 * <pre>
 * LDAPResult ::= SEQUENCE {
 *   resultCode ENUMERATED {
 *     success (0),
 *     ...
 *   },
 *   matchedDN LDAPDN,
 *   errorMessage LDAPString
 * }
 * </pre>
 *
 * Note that LDAPv3 supports referral within the LDAP
 * Result. The added component is:
 *
 * <pre>
 * LDAPResult ::= SEQUENCE {
 *   ...
 *   errorMessage LDAPString,
 *   referral [3] Referral OPTIONAL
 * }
 * </pre>
 *
 */
public class JDAPResult {
    /**
     * Result code based on RFC1777
     */
    public final static int SUCCESS                      = 0;
    public final static int OPERATION_ERROR              = 1;
    public final static int PROTOCOL_ERROR               = 2;
    public final static int TIME_LIMIT_EXCEEDED          = 3;
    public final static int SIZE_LIMIT_EXCEEDED          = 4;
    public final static int COMPARE_FALSE                = 5;
    public final static int COMPARE_TRUE                 = 6;
    public final static int AUTH_METHOD_NOT_SUPPORTED    = 7;
    public final static int STRONG_AUTH_REQUIRED         = 8;
    /* Referrals Within the LDAPv2 Protocol */
    public final static int LDAP_PARTIAL_RESULTS         = 9;
    /* Added for LDAPv3 - BEGIN */
    public final static int REFERRAL                     = 10;
    public final static int ADMIN_LIMIT_EXCEEDED         = 11;
    public final static int UNAVAILABLE_CRITICAL_EXTENSION = 12;
    public final static int CONFIDENTIALITY_REQUIRED     = 13;
    public final static int SASL_BIND_IN_PROGRESS        = 14;
    /* Added for LDAPv3 - END */
    public final static int NO_SUCH_ATTRIBUTE            = 16;
    public final static int UNDEFINED_ATTRIBUTE_TYPE     = 17;
    public final static int INAPPROPRIATE_MATCHING       = 18;
    public final static int CONSTRAINT_VIOLATION         = 19;
    public final static int ATTRIBUTE_OR_VALUE_EXISTS    = 20;
    public final static int INVALID_ATTRIBUTE_SYNTAX     = 21;
    public final static int NO_SUCH_OBJECT               = 32;
    public final static int ALIAS_PROBLEM                = 33;
    public final static int INVALID_DN_SYNTAX            = 34;
    public final static int IS_LEAF                      = 35;
    public final static int ALIAS_DEREFERENCING_PROBLEM  = 36;
    public final static int INAPPROPRIATE_AUTHENTICATION = 48;
    public final static int INVALID_CREDENTIALS          = 49;
    public final static int INSUFFICIENT_ACCESS_RIGHTS   = 50;
    public final static int BUSY                         = 51;
    public final static int UNAVAILABLE                  = 52;
    public final static int UNWILLING_TO_PERFORM         = 53;
    public final static int LOOP_DETECT                  = 54;
    public final static int NAMING_VIOLATION             = 64;
    public final static int OBJECT_CLASS_VIOLATION       = 65;
    public final static int NOT_ALLOWED_ON_NONLEAF       = 66;
    public final static int NOT_ALLOWED_ON_RDN           = 67;
    public final static int ENTRY_ALREADY_EXISTS         = 68;
    public final static int OBJECT_CLASS_MODS_PROHIBITED = 69;
    public final static int AFFECTS_MULTIPLE_DSAS        = 71;
    public final static int OTHER                        = 80;
    public final static int SERVER_DOWN                  = 81;
    public final static int PARAM_ERROR                  = 89;
    public final static int CONNECT_ERROR                = 91;
    public final static int LDAP_NOT_SUPPORTED           = 92;
    public final static int CONTROL_NOT_FOUND            = 93;
    public final static int NO_RESULTS_RETURNED          = 94;
    public final static int MORE_RESULTS_TO_RETURN       = 95;
    public final static int CLIENT_LOOP                  = 96;
    public final static int REFERRAL_LIMIT_EXCEEDED      = 97;

    /**
     * Private variable
     */
    protected BERElement m_element = null;
    protected int m_result_code;
    protected String m_matched_dn = null;
    protected String m_error_message = null;
    protected String m_referrals[] = null;

    /**
     * Constructs ldap result.
     * @param element ber element
     */
    public JDAPResult(BERElement element) throws IOException {
        /* Result from a successful bind request.
         * [*] umich-ldap-v3.3
         *     0x0a 0x07      (implicit [APPLICATION 1] OctetString)
         *     0x0a 0x01 0x00 (result code)
         *     0x04 0x00      (matched dn)
         *     0x04 0x00      (error message)
         * Referrals in ldap-v2.0
         * [*] umich-ldap-v3.3
         *     0x65 0x2a
         *     0x0a 0x01 0x09
         *     0x04 0x00
         *     0x04 0x23 R e f e r r a l : 0x0a
         *               l d a p : / / l d a p .
         *               i t d . u m i c h . e d u
         */
        m_element = element;
        BERSequence seq = (BERSequence)element;
        /*
         * Possible return from [x500.arc.nasa.gov]:
         * SEQUENCE {SEQUENCE {Enumerated{2} ... }}
         */
        BERElement e = seq.elementAt(0);
        if (e.getType() == BERElement.SEQUENCE)
            seq = (BERSequence)e;
        m_result_code = ((BEREnumerated)seq.elementAt(0)).getValue();
        byte buf[] = null;
        buf = ((BEROctetString)seq.elementAt(1)).getValue();
        if (buf == null)
            m_matched_dn = null;
        else {
            try{
                m_matched_dn = new String(buf, "UTF8");
            } catch(Throwable x)
            {}
        }
        buf = ((BEROctetString)seq.elementAt(2)).getValue();
        if (buf == null)
            m_error_message = null;
        else {
            try {
                m_error_message = new String(buf, "UTF8");
            } catch(Throwable x)
            {}
        }
        /* 12/05/97 extract LDAPv3 referrals */
        if (seq.size() >= 4) {
            BERTag t = (BERTag)seq.elementAt(3);
            BERElement v = t.getValue();
            if (v.getType() == BERElement.INTEGER) {
                /* MS - [CONTEXT-5] Integer {3} */
                /* TBD */

            // else if this element is BERSequence which contains referral information
            } else if (v instanceof BERSequence) {
                /* Netscape */
                BERSequence rseq = (BERSequence)v;
                if (rseq.size() > 0) {
                    m_referrals = new String[rseq.size()];
                    for (int i = 0; i < rseq.size(); i++) {
                        try{
                            m_referrals[i] = new
                            String(((BEROctetString)rseq.elementAt(i)).getValue(), "UTF8");
                        } catch(Throwable x)
                        {}
                    }
                }
            }
        }
    }

    /**
     * Gets the result code.
     * @return result code
     */
    public int getResultCode() {
        return m_result_code;
    }

    /**
     * Gets the matched dn.
     * @return matched dn
     */
    public String getMatchedDN() {
        return m_matched_dn;
    }

    /**
     * Gets the error message.
     * @return error message
     */
    public String getErrorMessage() {
        return m_error_message;
    }

    /**
     * Retrieves referrals from the LDAP Result.
     * @return list of referrals in URL format
     */
    public String[] getReferrals() {
        return m_referrals;
    }

    /**
     * Retrieves the ber representation of the result.
     * @return ber representation of the result
     */
    public BERElement getBERElement() {
        return m_element;
    }

    /**
     * Retrieves string representation of the result. Usually,
     * the inherited class calls this to retrieve the parameter
     * string.
     * @return string representation
     */
    public String getParamString() {
        StringBuffer sb = new StringBuffer("{resultCode=");
        sb.append(m_result_code);
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

    /**
     * Retrieves string representation of the result.
     * @return string representation
     */
    public String toString() {
        return "Result " + getParamString();
    }
}
