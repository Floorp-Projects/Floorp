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

import java.io.Serializable;

/**
 * Represents information used to authenticate the client in cases where
 * the client follows referrals automatically.  If you are defining a class
 * that implements the <CODE>LDAPRebind</CODE> interface, your implementation
 * of the <CODE>LDAPRebind.getRebindAuthentication</CODE> method needs to
 * construct and return an object of this class.
 * <P>
 *
 * For example, the following method sets up authentication information based
 * on the LDAP server identified in the referral.  Ideally, this method would be
 * defined as part of a class implementing the <CODE>LDAPRebind</CODE> interface.
 *
 * <PRE>
 * private String myDN = "cn=Directory Manager,o=Ace Industry,c=US";
 * private String myPW = "alway4444";
 * private LDAPAuthProvider myRebindInfo;
 * ...
 * public LDAPAuthProvider getRebindAuthentication( String host, int port ) {
 *  if ( host.equalsIgnoreCase( "alway.mcom.com" ) && ( port == 389 ) ) {
 *      myRebindInfo = new LDAPAuthProvider( myDN, myPW );
 *  } else {
 *      myRebindInfo = new LDAPAuthProvider( "", "" );
 *  }
 *  return myRebindInfo;
 * } </PRE>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPAuthHandler
 */
public class LDAPAuthProvider implements Serializable {

    static final long serialVersionUID = 7161655313564756294L;
    private String _dn;
    private byte[] _password;

    /**
     * Constructs information that is used by the client
     * for authentication when following referrals automatically.
     * @param dn distinguished name to use for authenticating to
     * the LDAP server during an automatic referral (if the client
     * is set up to follow referrals automatically)
     * @param password password to use for authenticating to
     * the LDAP server during an automatic referral (if the client
     * is set up to follow referrals automatically)
     */
    public LDAPAuthProvider( String dn, byte[] password ) {
        _dn = dn;
        _password = password;
    }

    /**
     * Returns the distinguished name to be used for reauthentication,
     * if the client is set up to follow referrals automatically.
     * @return distinguished name to use when authenticating to
     * other LDAP servers during referrals.
     */
    public String getDN() {
        return _dn;
    }

    /**
     * Returns the password to be used for reauthentication,
     * if the client is set up to follow referrals automatically.
     * @return password to use when authenticating to other
     * LDAP servers during referrals.
     */
    public byte[] getPassword() {
        return _password;
    }
}
