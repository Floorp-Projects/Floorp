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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap;

import java.util.*;
import java.io.*;

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
 * private LDAPRebindAuth myRebindInfo;
 * ...
 * public LDAPRebindAuth getRebindAuthentication( String host, int port ) {
 *  if ( host.equalsIgnoreCase( "alway.mcom.com" ) && ( port == 389 ) ) {
 *      myRebindInfo = new LDAPRebindAuth( myDN, myPW );
 *  } else {
 *      myRebindInfo = new LDAPRebindAuth( "", "" );
 *  }
 *  return myRebindInfo;
 * } </PRE>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPRebind
 */
public class LDAPRebindAuth {

    private String m_dn;
    private String m_password;

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
    public LDAPRebindAuth(String dn, String password) {
        m_dn = dn;
        m_password = password;
    }

    /**
     * Returns the distinguished name to be used for reauthentication,
     * if the client is set up to follow referrals automatically.
     * @return distinguished name to use when authenticating to
     * other LDAP servers during referrals.
     */
    public String getDN() {
        return m_dn;
    }

    /**
     * Returns the password to be used for reauthentication,
     * if the client is set up to follow referrals automatically.
     * @return password to use when authenticating to other
     * LDAP servers during referrals.
     */
    public String getPassword() {
        return m_password;
    }
}
