/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.m5ozilla.org/NPL/
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

import netscape.ldap.*;
import netscape.ldap.util.*;
import netscape.ldap.controls.*;

/**
 * LDAPTool
 * Base class for LDAP command-line tools
 *
 * @version 1.0
 * @author Rob Weltman
 **/
class LDAPTool {

	/**
	 * This function is to extract specified parameters from the
	 * arguments list.
	 * @param args list of args
	 */
    protected static GetOpt extractParameters(String privateOpts, String args[]) { 

		GetOpt options = new GetOpt("vnRMD:h:O:p:w:d:V:y:" + privateOpts, args);

		if (options.hasOption('n'))
			m_justShow = true;

		if (options.hasOption('v'))
			m_verbose = true;

		if (options.hasOption('R'))
			m_referrals = false;

		/* -D bind DN */
		if (options.hasOption('D'))
			m_binddn = options.getOptionParam('D');

		/* -h ldap host */
		if (options.hasOption('h'))
			m_ldaphost = options.getOptionParam('h');
      
		/* -p ldap port */
		if (options.hasOption('p')) { /* if the option is -p */
			try {
				m_ldapport = Integer.parseInt(options.getOptionParam('p'));
			} catch (NumberFormatException e) {
				m_ldapport = 389;
			}
		} /* if the option is -p */

		/* -O hop limit */
		if (options.hasOption('O')) { /* if the option is -O */
			try {
				m_hopLimit = Integer.parseInt(options.getOptionParam('O'));
			} catch (NumberFormatException e) {
				m_hopLimit = 10;
			}
		} /* if the option is -O */

		/* -d debug level */
		if (options.hasOption('d')) { /* if the option is -d */
			try {
				m_debugLevel = Integer.parseInt(options.getOptionParam('d'));
			} catch (NumberFormatException e) {
				m_debugLevel = 0;
			}
		} /* if the option is -d */

		/* -V ldap protocol version */
		if (options.hasOption('V')) { /* if the option is -V */
			try {
				m_version = Integer.parseInt(options.getOptionParam('V'));
			} catch (NumberFormatException e) {
				m_version = 3;
			}
		} /* if the option is -V */

		/* -w bind password */
		if (options.hasOption('w'))
			m_passwd = options.getOptionParam('w');

        /* -y proxy DN */
        if (options.hasOption('y'))
            m_proxyControl = new LDAPProxiedAuthControl(
                options.getOptionParam('y'), true );

		/* -M treat ref attribute as ordinary entry */
		if (options.hasOption('M'))
			m_ordinary = true;
        return options;
	}

    protected static void setDefaultReferralCredentials(
		LDAPConstraints cons ) {
		LDAPRebind rebind = new LDAPRebind() {
			public LDAPRebindAuth getRebindAuthentication(
				String host,
				int port ) {
					return new LDAPRebindAuth( 
						m_client.getAuthenticationDN(),
						m_client.getAuthenticationPassword() );
				}
		};
		cons.setReferrals( true );
		cons.setRebindProc( rebind );
	}

  protected static int m_ldapport = 389;
  protected static String m_binddn = null;
  protected static String m_ldaphost = "localhost";
  protected static String m_passwd = null;
  protected static int m_version = 3;
  protected static int m_debugLevel = 0;
  protected static int m_hopLimit = 10;
  protected static boolean m_referrals = true;
  protected static LDAPConnection m_client = null;
  protected static boolean m_justShow = false;
  protected static boolean m_verbose = false;
  protected static boolean m_ordinary = false;
  protected static LDAPControl m_proxyControl = null;
}
