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
package netscape.ldap;

import java.util.*;
import netscape.ldap.client.*;
import netscape.ldap.client.opers.*;
import java.io.*;

/**
 * Represents the situation in which the LDAP server refers the client to
 * another LDAP server.  This exception constructs a list of referral URLs from
 * the LDAP error message returned by the server.  You can get this list by
 * using the <CODE>getURLs</CODE> method.
 *
 * @version 1.0
 * @see netscape.ldap.LDAPException
 */
public class LDAPReferralException extends LDAPException {

    static final long serialVersionUID = 1771536577344289897L;
    private String m_referrals[] = null; /* modified for LDAPv3 */

    /**
     * Constructs a default exception with no specific error information.
     */
    public LDAPReferralException() {
    }

    /**
     * Constructs a default exception with a specified string as
     * additional information. This form is used for lower-level errors.
     * @param message the additional error information
     */
    public LDAPReferralException( String message ) {
        super( message );
    }

    /**
     * Constructs a default exception with a specified string as
     * additional information. This form is used for higher-level LDAP
     * operational errors.
     * @param message the additional error information
     * @param resultCode result code
     * @param serverErrorMessage error message
     */
    public LDAPReferralException( String message, int resultCode,
        String serverErrorMessage ) {
        super(message, resultCode, serverErrorMessage);
    }

    /**
     * Constructs an exception with a list of LDAP URLs to other LDAP servers.
     * This list of referrals points the client to LDAP servers that may
     * contain the requested entries.
     * @param message the additional error information
     * @param resultCode result code
     * @param referrals array of LDAP URLs identifying other LDAP servers that
     * may contain the requested entries
     */
    public LDAPReferralException( String message, int resultCode,
        String referrals[] ) {
        super(message, resultCode, null);
        m_referrals = referrals;
    }

    /**
     * Gets the list of referrals (LDAP URLs to other servers) returned by the LDAP server.
     * You can use this list to find the LDAP server that can fulfill your request.
     *
     * If you have set up your search constraints (or the <CODE>LDAPConnection</CODE> object)
     * to follow referrals automatically, any operation that results in a referral will use
     * this list to create new connections to the LDAP servers in this list.
     *
     * @return list of LDAP URLs to other LDAP servers.
     */
    public LDAPUrl[] getURLs() {
        if (getLDAPErrorMessage() == null) {
            return constructsURL(m_referrals);
        } else {
            return constructsURL(extractReferrals(getLDAPErrorMessage()));
        }
    }

    private LDAPUrl[] constructsURL(String referrals[]) {
        if (referrals == null) {
            return null;
        }
        LDAPUrl u[] = new LDAPUrl[referrals.length];
        if (u == null) {
            return null;
        }
        for (int i = 0; i < referrals.length; i++) {
            try {
                u[i] = new LDAPUrl(referrals[i]);
            } catch (Exception e) {
                return null;
            }
        }
        return u;
    }

    /**
     * Extract referral string from the error message. The
     * error string is based on "Referrals Within the
     * LDAPv2 Protocol".
     * @param error string
     */
    private String[] extractReferrals(String error) {
        if (error == null)
            return null;
        StringTokenizer st = new StringTokenizer(error, "\n");
        Vector v = new Vector();
        boolean referrals = false;
        while (st.hasMoreTokens()) {
            String token = st.nextToken();
            if (referrals) {
                v.addElement(token);
            } else {
                if (token.startsWith("Referral:"))
                    referrals = true;
            }
        }

        if (v.size() == 0)
            return null;
        String res[] = new String[v.size()];
        for (int i = 0; i < v.size(); i++) {
            res[i] = (String)v.elementAt(i);
        }
        return res;
    }
    
   /**
     * Gets the string representation of the referral exception,
     * which includes the result code, the message sent back
     *  from the LDAP server and the list of referrals.
     *
     * @return string representation of exception.
     * @see netscape.ldap.LDAPException#errorCodeToString(int)
     */
    public String toString() {
        String str = super.toString();
        for (int i=0; i < m_referrals.length; i++) {
            str += "\n" + m_referrals[i];
        }
        return str;
    }
}
