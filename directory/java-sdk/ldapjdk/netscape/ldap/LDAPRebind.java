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
 * Specifies how to retrieve authentication information automatically
 * for referrals. If you have set up the search constraints (or the options
 * in the <CODE>LDAPConnection</CODE> object) to use automatic referral,
 * you must define a class that implements this interface.
 * <P>
 *
 * If no class implements this interface, clients that follow automatic
 * referrals are authenticated anonymously to subsequent LDAP servers.
 * The following example is a simple class that implements this interface.
 * Objects of the myLDAPRebind class check the host and port of the 
 * referred LDAP server.  If the host and port are "alway.mcom.com:389", 
 * the directory manager's name and password are used to authenticate.
 * For all other LDAP servers, anonymous authentication is used.
 *
 * <PRE>
 * public class myLDAPRebind implements netscape.ldap.LDAPRebind
 * {
 *  private String myDN;
 *  private String myPW;
 *  private LDAPRebindAuth myRebindInfo;

 *  public myLDAPRebind () {
 *    myDN = "c=Directory Manager,o=Universal Exports,c=UK";
 *    myPW = "alway4444";
 *  }
 *
 *  public LDAPRebindAuth getRebindAuthentication( String host, int port ) {
 *    if ( host.equalsIgnoreCase( "alway.mcom.com" ) && ( port == 389 ) ) {
 *      myRebindInfo = new LDAPRebindAuth( myDN, myPW );
 *    } else {
 *      myRebindInfo = new LDAPRebindAuth( "", "" );
 *    }
 *    return myRebindInfo;
 *  }
 * } </PRE>
 *
 *
 * @version 1.0
 */
public interface LDAPRebind {

    /**
     * Returns an <CODE>LDAPRebindAuth</CODE> object, which the calling function
     * can use to get the DN and password to use for authentication (if the client
     * is set up to follow referrals automatically).
     * @return LDAPRebindAuth object containing authentication information.
     * @see netscape.ldap.LDAPRebindAuth
     */
    public LDAPRebindAuth getRebindAuthentication(String host,
      int port);
}
