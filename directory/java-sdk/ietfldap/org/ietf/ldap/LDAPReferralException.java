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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.util.*;
//import org.ietf.ldap.client.*;
//import org.ietf.ldap.client.opers.*;
//import java.io.*;

/**
 * Represents the situation in which the LDAP server refers the client to
 * another LDAP server.  This exception constructs a list of referral URLs from
 * the LDAP error message returned by the server.  You can get this list by
 * using the <CODE>getURLs</CODE> method.
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPException
 */
public class LDAPReferralException extends LDAPException {

    static final long serialVersionUID = 1771536577344289897L;
    private String _referrals[] = null;

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
    public LDAPReferralException( String message,
                                  int resultCode,
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
    public LDAPReferralException( String message,
                                  int resultCode,
                                  String referrals[] ) {
        super(message, resultCode, (String)null);
        _referrals = referrals;
    }

    /**
     * Constructs an exception with a result code, a specified
     * string of additional information, a string containing
     * information passed back from the server, and a possible root
     * exception.
     * <P>
     *
     * After you construct the <CODE>LDAPException</CODE> object,
     * the result code and messages will be accessible through the
     * following ways:
     * <P>
     * <UL>
     * <LI>This string of additional information appears if you
     * call the <CODE>toString()</CODE> method. <P>
     * <LI>The result code that you set is accessible through the
     * <CODE>getLDAPResultCode()</CODE> method. <P>
     * <LI>The string of server error information that you set
     * is accessible through the <CODE>getLDAPErrorMessage</CODE>
     * method. <P>
     * </UL>
     * <P>
     *
     * This form is used for higher-level LDAP operational errors.
     * @param message the additional error information
     * @param resultCode the result code returned
     * @param serverErrorMessage error message specifying additional information
     * returned from the server
     * @param rootException An exception which caused the failure, if any
     * @see org.ietf.ldap.LDAPException#toString()
     * @see org.ietf.ldap.LDAPException#getResultCode()
     * @see org.ietf.ldap.LDAPException#getLDAPErrorMessage()
     * @see org.ietf.ldap.LDAPException#getMatchedDN()
     */
    public LDAPReferralException( String message,
                                  int resultCode,
                                  String serverErrorMessage,
                                  Throwable rootException ) {
        super( message, resultCode, serverErrorMessage, rootException );
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
    public String[] getReferrals() {
        if ( getLDAPErrorMessage() == null ) {
            return (_referrals != null) ? _referrals : new String[0];
        } else {
            return extractReferrals( getLDAPErrorMessage() );
        }
    }

    /**
     * Gets the referral URL that could not be followed. If multiple URLs 
     * are in the list, and none could be followed, the method returns one 
     * of them.
     *
     * @return the referral URL that could not be followed
     */
    public String getFailedReferral() {
        String[] urls = getReferrals();
        return (urls.length > 0) ? urls[0] : "";
    }

    /**
     * Sets the referral URL that could not be followed
     *
     * @param referral the referral URL that could not be followed
     */
    public void setFailedReferral( String referral ) {
        _referrals = new String[] { referral };
    }

    /**
     * Extract referral string from the error message. The
     * error string is based on "Referrals Within the
     * LDAPConnection Protocol".
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
     * @see org.ietf.ldap.LDAPException#resultCodeToString(int)
     */
    public String toString() {
        String str = super.toString();
        for (int i=0; i < _referrals.length; i++) {
            str += "\n" + _referrals[i];
        }
        return str;
    }
}
