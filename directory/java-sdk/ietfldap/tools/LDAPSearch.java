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
import org.ietf.ldap.controls.*;

/**
 * Execute Search operations through the LDAP client interface. 
 * This class is implemented based on the LDAP class library.
 *
 * <pre>
 * usage       : java LDAPSearch -b baseDN [options] filter [attributes...]
 * for example : java LDAPSearch -b "c=us" -h ds.internic.net -p 389 
 *               "(objectClass=*)"
 *     
 *   note: '-' or '/' is used to distinct the option field.
 *         e.g. -a -b /c /d parameter -e parameter
 *
 * filter:
 *   Any string in RFC1558 specification.
 *    e.g. "(objectClass=*)"
 *
 * attributes: {0..n}
 *   All the string parameters follows with the filter.
 *     e.g. filter attrsA attrsB attrsC
 * </pre>
 *
 * @version 1.0
 */
public class LDAPSearch extends LDAPTool { 
	/**
	 * This is the main function.
	 * @param args list of arguments
	 */
    public static void main(String args[]) { 

		/* extract parameters from the arguments list */
		extractParameters(args);

		if (!m_justShow) {
			/* perform an LDAP client connection operation */
			try {
				m_client = new LDAPConnection();
				m_client.connect( m_ldaphost, m_ldapport );
			} catch(Exception e) {
				System.err.println("Error: client connection failed!");
				System.exit(0);
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

			/* perform a search operation */
			dosearch();

			m_pw.flush();
			m_pw.close();

			/* disconnect */
			try {
				m_client.disconnect();
			} catch (Exception e) {
				System.err.println( e.toString() );
			}
		}
		System.exit(0);
	}

	/**
	 * Prints usage.
	 */
    private static void doUsage() {
		System.err.println("usage: LDAPSearch -b basedn [options] " +
						   "filter [attributes...]");
		System.err.println("options");
		System.err.println("  -h host       LDAP server name or IP address");
		System.err.println("  -p port       LDAP server TCP port number");
		System.err.println("  -V version    LDAP protocol version " +
						   "number (default is 3)");
		System.err.println("  -D binddn     bind dn");
		System.err.println("  -w password   bind passwd (for simple " +
						   "authentication)");
		System.err.println("  -v            run in verbose mode");
        System.err.println("  -n            show what would be done but "+
							"don't actually do it");
		System.err.println("  -d level      set LDAP debugging level " +
						   "to \'level\'");
		System.err.println("  -R            do not automatically follow " +
						   "referrals");
		System.err.println("  -O hop limit  maximum number of referral " +
						   "hops to traverse");
		System.err.println("  -H            display usage information");
		System.err.println("  -t            write values to files");
		System.err.println("  -A            retrieve attribute names only");
		System.err.println("  -F sep        print \'sep\' instead of " +
						   "\'=\' between attribute names and values");
		System.err.println("  -S attr       sort the results by attribute " +
						   "\'attr\'");
		System.err.println("  -s scope      one of base, one, or sub " +
						   "(search scope)");
		System.err.println("  -a deref      one of never, always, search, " +
						   "or find (alias dereferencing)");
		System.err.println("  -l timelimit  time limit (in seconds) for " +
						   "search");
		System.err.println("  -T            do not fold (wrap) long lines "+
		                                    "(default is to fold)");
		System.err.println("  -x            perform sorting on server");
		System.err.println("  -M            manage references (treat them "+
							"as regular entries)");
		System.err.println("  -z sizelimit  size limit (in entries) for " +
						   "search");
		System.err.println("  -G before:after:index:count | before:after:value "+
						   "where 'before' and 'after' are the number of "+
						   "entries surrounding 'index'. 'count' is the "+
						   "content count, 'value' is the search value.");
		System.err.println("  -y proxy-DN   DN to use for access control");
		System.err.println("  -X            output DSML instead of LDIF");
	}

	/**
	 * This function is to extract specified parameters from the
	 * arguments list.
	 * @param args list of args
	 */
    protected static void extractParameters(String args[]) { 

		String privateOpts = "HATtxvnXa:b:F:l:s:S:z:G:";

		GetOpt options = LDAPTool.extractParameters( privateOpts, args );

		/* -H Help */
		if (options.hasOption('H')) {
			doUsage();
			System.exit(0);
		} /* Help */
  
		/* -A retrieve attribute name only == no values */
		if (options.hasOption('A'))
			m_attrsonly = true;

		if (options.hasOption('x'))
			m_sortOnServer = true;

		if (options.hasOption('t'))
			m_tempFiles = true;

		/* -F separator */
		if (options.hasOption('F'))
			m_sep = options.getOptionParam('F');

		/* -a set alias deref option */
		if (options.hasOption('a')) { /* has option a */
    
			String param = options.getOptionParam('a');
      
			if (param.equalsIgnoreCase("never"))
				m_deref = 0;
			else if (param.equalsIgnoreCase("search"))
				m_deref = 1;
			else if (param.equalsIgnoreCase("find"))
				m_deref = 2;
			else if (param.equalsIgnoreCase("always"))
				m_deref = 3;
			else
				System.err.println("Error: alias deref option " +
								   "should be never, search, find, " +
								   "or always.");
		} /* has option a */

		/* -b searchbase */
		if (options.hasOption('b'))
			m_base = options.getOptionParam('b');

		/* -S sort attribute */
		if (options.hasOption('S'))
			m_sort.addElement( options.getOptionParam('S') );
      
		/* -l time limit */
		if (options.hasOption('l')) { /* if the option is -l */
			try { 
				m_timelimit = Integer.parseInt(options.getOptionParam('l'));
			} catch (NumberFormatException e) { 
				m_timelimit = 0;
			}
		} /* if the option is -l */

		/* -s search scope */
		if (options.hasOption('s')) { /* has option s */

			String param = options.getOptionParam('s');
      
			if (param.equalsIgnoreCase("base"))
				m_scope = 0;
			else if (param.equalsIgnoreCase("one"))
				m_scope = 1;
			else if (param.equalsIgnoreCase("sub"))
				m_scope = 2;
			else
				System.err.println("Error: scope should be base, " +
								   "one or sub.");
		} /* has option s */
    
		/* -z size limit */
		if (options.hasOption('z')) { /* if the option is -z */
			try { 
				m_sizelimit = Integer.parseInt(options.getOptionParam('z'));
			} catch (NumberFormatException e) {
				m_sizelimit = 0;
			} 
		} /* if the option is -z */

		/* -T fold line */
		if (options.hasOption('T')) { /* if the option is -T */
			m_foldLine = false;
		}

		if (options.hasOption('X'))
			m_printDSML = true;

		parseVlv(options);

		/* extract the filter string and attributes */
		Enumeration pa = options.getParameters().elements();
		Vector vec = new Vector();

		while (pa.hasMoreElements()) { /* while */
			vec.addElement(pa.nextElement()); 
		} /* while */

		int counter = vec.size();
       
		if (counter <= 0) { /* No filter */
			System.err.println("Error: must supply filter string!");
			doUsage();
			System.exit(0);
		} /* No filter */
  
		if (counter == 1) { /* Has filter but no attributes */

			/* gets filter string */
			m_filter = (String)vec.elementAt(0);
			if (m_verbose)
				System.err.println("filter pattern: "+m_filter);

			/* no attributes */
			m_attrs = null;
			if (m_verbose) {
				System.err.println("returning: ALL");
				System.err.println("filter is: ("+m_filter+")");
			}
		} /* Has filter but no attributes */
    
		if (counter > 1) { /* Has filter and attributes */

			/* gets filter string */
			m_filter = (String)vec.elementAt(0);
			if (m_verbose) {
				System.err.println("filter pattern: "+m_filter);
				System.err.print("returning:");
			}

			/* gets attributes */
			m_attrs = new String[counter];
			for (int j = 1; j < counter; j++) {
				m_attrs[j-1] = (String)vec.elementAt(j);
                if (m_verbose)
					System.err.print(" "+m_attrs[j-1]);
			}  
			if (m_verbose) {
				System.err.println();
				System.err.println("filter is: ("+m_filter+")");
			}
		} /* Has filter and attributes */ 
	}

	private static void parseVlv(GetOpt options) {

		/* -G virtual list */
		if (options.hasOption('G')) { /* if the option is -G */
			String val = options.getOptionParam('G');
			StringTokenizer tokenizer = new StringTokenizer(val, ":");
			m_vlvTokens = tokenizer.countTokens();
			if (m_vlvTokens < 3) {
				doUsage();
				System.exit(0);
			}
            
			try {
				m_beforeCount = Integer.parseInt((String)tokenizer.nextElement());
			} catch (NumberFormatException e) {
				m_beforeCount = 0;
			}
            
			try {
				m_afterCount = Integer.parseInt((String)tokenizer.nextElement());
			} catch (NumberFormatException e) {
				m_afterCount = 0;
			}
            
			if (m_vlvTokens == 3) {
				m_searchVal = (String)tokenizer.nextElement();
			} else if (m_vlvTokens > 3) {
				try {
					m_index = Integer.parseInt((String)tokenizer.nextElement());
				} catch (NumberFormatException e) {
					m_index = 0;
				}

				try {
					m_count = Integer.parseInt((String)tokenizer.nextElement());
				} catch (NumberFormatException e) {
					m_count = 0;
				}
			}
		} /* if the option is -G */
	}

	/**
	 * This class-method is used to call the LDAP Search Operation with the
	 * specified options, parameters, filters and/or attributes.
	 */
    private static void dosearch() {

        LDAPControl[] controls = null;
		try {
        	Vector cons = new Vector();
			LDAPSortControl sort = null;
			if ( m_sortOnServer && (m_sort.size() > 0) ) {
				LDAPSortKey[] keys = new LDAPSortKey[m_sort.size()];
				for( int i = 0; i < keys.length; i++ ) {
					keys[i] = new LDAPSortKey( (String)m_sort.elementAt(i) );
				}
				sort = new LDAPSortControl( keys, true );
				cons.addElement(sort);
			}

			if ((sort == null) && (m_vlvTokens >= 3)) {
				System.err.println("Server-side sorting is required for "+
					"virtual list option");
				doUsage();
				System.exit(0);
			}

            LDAPVirtualListControl vControl = null;
			if (m_vlvTokens == 3) {
				vControl = new LDAPVirtualListControl(m_searchVal, 
			    	m_beforeCount, m_afterCount);
			} else if (m_vlvTokens > 3) {
				vControl = new LDAPVirtualListControl(m_index, m_beforeCount,
					m_afterCount, m_count);
            }

			if (vControl != null)
				cons.addElement(vControl);

			if (m_proxyControl != null)
				cons.addElement(m_proxyControl);

			if (m_ordinary) {
				LDAPControl manageDSAITControl = new LDAPControl(
					LDAPControl.MANAGEDSAIT, true, null);
				cons.addElement(manageDSAITControl);
            }

			if (cons.size() > 0) {
				controls = new LDAPControl[cons.size()];
				cons.copyInto(controls);
			}
		} catch (Exception e) {
			System.err.println( e.toString() );
			System.exit(0);
		}

		/* perform an LDAP Search Operation */
		LDAPSearchResults res = null;
		try {
			LDAPSearchConstraints cons = 
			        m_client.getSearchConstraints();
			cons.setControls(controls);
			cons.setDereference( m_deref );
			cons.setMaxResults( m_sizelimit );
			cons.setServerTimeLimit( m_timelimit );
			cons.setReferralFollowing( m_referrals );
			if ( m_referrals ) {
				setDefaultReferralCredentials( cons );
			}
			cons.setHopLimit( m_hopLimit );
			res = m_client.search( m_base, m_scope,
								   m_filter, m_attrs,
								   m_attrsonly, cons );
		} catch (Exception e) {
			System.err.println( e.toString() );
			System.exit(1);
		}

		/* Sort? */
		if ( (m_sort.size() > 0) && !m_sortOnServer ) {
			String[] sortAttrs = new String[m_sort.size()];
			for( int i = 0; i < sortAttrs.length; i++ )
				sortAttrs[i] = (String)m_sort.elementAt( i );
			res.sort( new LDAPCompareAttrNames( sortAttrs ) );
		}

		/* print out the values of the entries */
		printResults( res );

		/* Any sort control responses? */
		showControls( m_client.getResponseControls() );
	}

	/**
	 * Print the result entries. 
	 * @param res Search results
	 */
    private static void printResults( LDAPSearchResults res ) { 
		boolean isSchema = false;
		boolean didEntryIntro = false;
		LDAPWriter writer;
		if ( m_printDSML ) {
			printString( DSML_INTRO );
			writer = new DSMLWriter( m_pw );
		} else {
			writer = new LDIFWriter( m_pw, m_attrsonly, m_sep, m_foldLine,
									 m_tempFiles );
		}

		/* print out the retrieved entries */
		try {
			/* Loop on results until finished */
			while ( res.hasMore() ) {

				LDAPEntry entry = null;
				try {
					/* Next directory entry */
					entry = res.next();
				} catch (LDAPReferralException ee) {
					String[] urls= ee.getReferrals();
					System.err.println("Referral entries: ");
					for (int i=0; i<urls.length; i++)
						System.err.println("\t"+urls[i]);
					continue;
				} catch (Exception e) {
					m_pw.flush();
					System.err.println( e.toString() );
					continue;
				}

				if ( isSchemaEntry( entry ) ) {
					writer.printSchema( entry );
				} else {
					if ( m_printDSML && !didEntryIntro ) {
						printString( DSML_RESULTS_INTRO );
						didEntryIntro = true;
					}
					writer.printEntry( entry );
				}
			}
		} catch (Exception e) {
			m_pw.flush();
			System.err.println( e.toString() );
			System.exit(0);
		}
		if ( m_printDSML ) {
			if ( didEntryIntro ) {
				printString( DSML_RESULTS_END );
			}
			printString( DSML_END );
		}
	}

    protected static void printString( String value ) {
		m_pw.print( value );
		m_pw.print( '\n' );
    }

	protected static boolean isSchemaEntry( LDAPEntry entry ) {
        LDAPAttribute attr = entry.getAttribute( "objectclass" );
		if ( attr != null ) {
			String[] vals = attr.getStringValueArray();
			for( int i = 0; i < vals.length; i++ ) {
				if ( vals[i].equalsIgnoreCase( "subschema" ) ) {
					return true;
				}
			}
        }
        return false;
    }

    /**
     * If there was a sort control returned, and the result code was
	 * not zero, show it.
     * @param controls Any server controls returned.
     **/
    private static void showControls( LDAPControl[] controls ) {
                if ( controls == null ) {
			return;
		}

		LDAPSortControl sControl = null;
		LDAPVirtualListResponse vResponse = null;
		for ( int i = 0; i < controls.length; i++ ) {
		    if ( controls[i] instanceof LDAPSortControl ) {
		        sControl = (LDAPSortControl)controls[i];
		    } else if ( controls[i] instanceof LDAPVirtualListResponse ) {
		        vResponse = (LDAPVirtualListResponse)controls[i];
		    }
		}

		if (sControl != null ) {
		    String bad = sControl.getFailedAttribute();
		    int result = sControl.getResultCode();
		    if ( result != LDAPException.SUCCESS ) {
		        System.err.println( "Error code: " + result );
			if ( bad != null ) {
			    System.err.println( "Offending attribute: " + bad );
			} else {
			     System.err.println( "No offending attribute returned" );
			}
		    } else {
			m_pw.println("Server indicated results sorted OK");
		    }
		}

		if (vResponse != null) {
			int resultCode = vResponse.getResultCode();
			if (resultCode == LDAPException.SUCCESS) {
				m_pw.println("Server indicated virtual list positioning OK");
				m_pw.println("index "+vResponse.getFirstPosition()+
					" content count "+vResponse.getContentCount());
			} else
				System.err.println("Virtual List Error: "+
					LDAPException.resultCodeToString(resultCode));
		}
    }

	/**
	 * Internal variables
	 */
	private static final String DSML_INTRO =
	    "<dsml:dsml xmlns:dsml=\"http://www.dsml.org/DSML\">";
	private static final String DSML_END = "</dsml:dsml>";
	private static final String DSML_RESULTS_INTRO =
  	    " <dsml:directory-entries>";
	private static final String DSML_RESULTS_END =
	    " </dsml:directory-entries>";
  private static boolean m_attrsonly = false;
  private static int m_deref = 0;
  private static int m_scope = 2; // default is sub search
  private static int m_sizelimit = 0;
  private static int m_timelimit = 0;
  private static int verbose = 0;
  private static String m_attrs[] = null; 
  private static String m_base = "o=ace industry,c=us";
  private static String m_filter = null;
  private static String m_sep = ":";
  private static Vector m_sort = new Vector();
  private static boolean m_sortOnServer = false;
  private static boolean m_tempFiles = false;
  private static int m_beforeCount = 0;
  private static int m_afterCount = 0;
  private static int m_index = 0;
  private static int m_count = 0;
  private static int m_vlvTokens = 0;
  private static String m_searchVal = null;
  private static boolean m_foldLine = true;
  private static final int MAX_LINE = 77;
  private static PrintWriter m_pw = new PrintWriter(System.out);
  private static MimeBase64Encoder m_encoder = new MimeBase64Encoder();
  private static boolean m_printDSML = false;
}
