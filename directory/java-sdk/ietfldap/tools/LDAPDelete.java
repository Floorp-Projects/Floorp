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

import java.io.*;
import java.net.*;
import java.util.*;
import org.ietf.ldap.*;
import org.ietf.ldap.util.*;

/**
 * Executes the delete command to delete an LDAP entry.
 * This class is implemented based on the java LDAP classes.
 *
 * <pre>
 * usage       : java LDAPDelete [options] DN
 * for example : java -D "dn" -w password -h ds.internic.net -p 389
 *               "cn=Johnny James,o=Ace Industry"
 *
 * options: {np = no parameters, p = requires parameters}
 *  'D' bind DN --------------------------------------------- p
 *  'h' LDAP host ------------------------------------------- p
 *  'p' LDAP port ------------------------------------------- p
 *  'w' bind password --------------------------------------- p
 *
 * note: '-' or '/' is used to distinct the option field.
 *       e.g. -a -b /c /d parameter -e parameter
 *
 * </pre>
 *
 * @version 1.0
 */

public class LDAPDelete extends LDAPTool { /* LDAPDelete */
 
    public static void main(String args[]) { /* main */

		if ( args.length < 1 ) {
			doUsage();
			System.exit(1);
		}

		/* extract parameters from the arguments list */
		extractParameters(args);

		if (!m_justShow) {

			/* perform an LDAP client connection operation */
			try {
				m_client = new LDAPConnection();
				m_client.connect( m_ldaphost, m_ldapport );
			} catch(Exception e) {
				System.err.println("Error: client connection failed!");
				System.exit(1);
			}
	
			/* perform an LDAP bind operation */
			if ( (m_binddn != null) && (m_passwd != null) ) {
			    try {
                    m_client.bind( m_version, m_binddn, m_passwd.getBytes() );
			    } catch (Exception e) {
                    System.err.println( e.toString() );
                    System.exit(0);
			    }
			}

			/* perform a delete operation */
			dodelete();

			/* disconnect */
			try {
				m_client.disconnect();
			} catch (Exception e) {
				System.err.println( e.toString() );
			}
		} else
			dodelete(null);
		System.exit(0);
  } /* main */

	/**
	 * Prints usage.
	 */
    private static void doUsage() {
		System.err.println( "usage: LDAPDelete [options] dn" );
		System.err.println("options");
		System.err.println("  -h host       LDAP server name or IP address");
		System.err.println("  -p port       LDAP server TCP port number");
		System.err.println("  -V version    LDAP protocol version " +
						   "number (default is 3)");
		System.err.println("  -D binddn     bind dn");
		System.err.println("  -w password   bind passwd (for simple " +
						   "authentication)");
		System.err.println("  -d level      set LDAP debugging level " +
						   "to \'level\'");
		System.err.println("  -f file      read DNs to delete from file");
		System.err.println("  -R            do not automatically follow " +
						   "referrals");
		System.err.println("  -O hop limit  maximum number of referral " +
						   "hops to traverse");
		System.err.println("  -H            display usage information");
		System.err.println("  -c            continuous mode (do not "+
						   "stop on errors)");

		System.err.println("  -M            manage references (treat them "+
						   "as regular entries)");
		System.err.println("  -y proxy-DN   DN to use for access control");
	}

	/**
	 * This class-method is used to extract specified parameters from the
	 * arguments list.
	 */
    protected static void extractParameters(String args[]) {
		GetOpt options = LDAPTool.extractParameters( "Hcf:", args );

		/* -H Help */
		if (options.hasOption('H')) {
			doUsage();
			System.exit(0);
		} /* Help */
      
		/* -c continuous mode */
		if (options.hasOption('c')) {
			m_cont = true;
		} /* continous mode */

		/* -f file */
		if (options.hasOption('f')) {
			String filename = options.getOptionParam('f');
			if (filename == null) {
				doUsage();
				System.exit(0);
			}
				
			try {
				FileInputStream fs = new FileInputStream(filename);
				DataInputStream ds = new DataInputStream(fs);
				m_reader = new BufferedReader(new InputStreamReader(ds));
			} catch (FileNotFoundException e) {
				System.err.println("File "+filename+" not found");
			} catch (IOException e) {
				System.err.println("Error in opening the file "+filename);
			}
		} /* input file */

		if (m_reader == null) { 
			Enumeration pa = options.getParameters().elements();
			Vector vec = new Vector();
			while (pa.hasMoreElements()) { /* while */
				vec.addElement(pa.nextElement());
			}
			
			if (vec.size() <= 0) {
				doUsage();
				System.exit(0);
			}
			m_delete_dn = new String[vec.size()];
			vec.copyInto(m_delete_dn);
		}
	} /* extract parameters */

	/**
	 * This class-method is used to call the LDAP Delete Operation with the
	 * specified options, and/or parameters.
	 */
    private static void dodelete() { /* dodelete */
		int msgid = 0;
		LDAPConstraints cons = m_client.getConstraints();

		Vector controlVector = new Vector();
		if (m_proxyControl != null)
			controlVector.addElement(m_proxyControl);
		if (m_ordinary) {
			controlVector.addElement( new LDAPControl(
				LDAPControl.MANAGEDSAIT, true, null) );
		}
		if (controlVector.size() > 0) {
			LDAPControl[] controls = new LDAPControl[controlVector.size()];
			controlVector.copyInto(controls);
			cons.setControls(controls);
		}
		cons.setReferralFollowing( m_referrals );
		if ( m_referrals ) {
			setDefaultReferralCredentials( cons );
		}
		cons.setHopLimit( m_hopLimit );
		dodelete(cons);
	} /* dodelete */

	private static void dodelete(LDAPConstraints cons) {
		try {
		  if (m_reader == null) {
			for (int i=0; i<m_delete_dn.length; i++) 
				if (!deleteEntry(m_delete_dn[i], cons) && !m_cont)
					return;
		  } else {
			String dn = null;
			while ((dn=m_reader.readLine()) != null) {
				if (!deleteEntry(dn, cons) && !m_cont)
					return;
			}
		  }
		} catch (IOException e) {
			System.err.println("Error in reading input");
		}
	}

	private static boolean deleteEntry(String dn, LDAPConstraints cons) {
		if (m_verbose) 
			System.err.println("Deleting entry: "+dn);
		if (!m_justShow) {
			try {
				m_client.delete(dn, cons);
			} catch (LDAPException ee) {
				System.err.println("Delete " + dn + " failed ");
				System.err.println("\t"+ee.resultCodeToString());
				System.err.println("\tmatched "+ee.getMatchedDN()+"\n");
				return false;
			}
		}
		return true;
	}

  private static String[] m_delete_dn = null;
  private static boolean m_cont = false;
  private static BufferedReader m_reader = null;
} /* LDAPDelete */

