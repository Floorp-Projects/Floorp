/* ======================================================================
 * Copyright (c) 1997 Netscape Communications Corporation
 * This file contains proprietary information of Netscape Communications.
 * Copying or reproduction without prior written approval is prohibited.
 * ======================================================================
 */

import java.io.*;
import java.net.*;
import java.util.*;
import netscape.ldap.*;
import netscape.ldap.util.*;
import netscape.ldap.controls.*;

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
			try {
				m_client.authenticate( m_version, m_binddn, m_passwd );
			} catch (Exception e) {
				System.err.println( e.toString() );
				System.exit(0);
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
	}

	/**
	 * This function is to extract specified parameters from the
	 * arguments list.
	 * @param args list of args
	 */
    protected static void extractParameters(String args[]) { 

		String privateOpts = "HATtxvna:b:F:l:s:S:z:G:";

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
			cons.setServerControls(controls);
			cons.setDereference( m_deref );
			cons.setMaxResults( m_sizelimit );
			cons.setTimeLimit( m_timelimit );
			cons.setReferrals( m_referrals );
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
		/* print out the retrieved entries */
		try {
			/* Loop on results until finished */
			while ( res.hasMoreElements() ) {

				LDAPEntry findEntry = null;
				try {
					/* Next directory entry */
					findEntry = (LDAPEntry)res.next();
				} catch (LDAPReferralException ee) {
					LDAPUrl[] urls= ee.getURLs();
					System.err.println("Referral entries: ");
					for (int i=0; i<urls.length; i++)
						System.err.println("\t"+urls[i].getUrl().toString());
					continue;
				} catch (Exception e) {
					System.err.println( e.toString() );
					continue;
				}

				String dn = findEntry.getDN();
				if (dn != null)
					printString("dn" + m_sep + " " + dn);
				else
					printString("dn" + m_sep);

				/* Get the attributes of the entry */
				LDAPAttributeSet findAttrs = findEntry.getAttributeSet();
				Enumeration enumAttrs = findAttrs.getAttributes();
				/* Loop on attributes */
				while ( enumAttrs.hasMoreElements() ) {
					LDAPAttribute anAttr =
						(LDAPAttribute)enumAttrs.nextElement();
					String attrName = anAttr.getName();
					if (m_attrsonly) {
						printString(attrName+":");
						continue;
					}

					/* Loop on values for this attribute */
					Enumeration enumVals;
					enumVals = anAttr.getByteValues();

					if (enumVals != null) {
					  while ( enumVals.hasMoreElements() ) {
						if ( m_tempFiles ) {
						    try {
							    FileOutputStream f = getTempFile( attrName );
								f.write( (byte[])enumVals.nextElement() );
							} catch ( Exception e ) {
							    System.err.println( "Error writing values " +
													"of " + attrName + ", " +
													e.toString() );
							    System.exit(1);
							}
						} else {
							byte[] b = (byte[])enumVals.nextElement();
							String s = null;
							if (LDIF.isPrintable(b)) {
								s = new String(b, "UTF8");
						    	printString(attrName + m_sep + " " + s);
							} else {
								ByteBuf inBuf = new ByteBuf( b, 0, b.length );
								ByteBuf encodedBuf = new ByteBuf();
								// Translate to base 64 
								MimeBase64Encoder encoder = new MimeBase64Encoder();
								encoder.translate( inBuf, encodedBuf );
								int nBytes = encodedBuf.length();
								if ( nBytes > 0 ) {
									s = new String(encodedBuf.toBytes(), 0, nBytes);
									printString( attrName + ":: " + s );
								} else {
									m_pw.print( attrName + ": \n" );
								}
							}
						}
					  }
					}
					else
						System.err.println("Failed to do string conversion for "+attrName);
				}
				m_pw.println();
			}
		} catch (Exception e) {
			System.err.println( e.toString() );
			System.exit(0);
		}
	}


    private static void printString( String value ) {
	  if (m_foldLine)
          LDIF.breakString(m_pw, value, MAX_LINE);
	  else {
		m_pw.print(value);
        m_pw.print( '\n' );
	  }
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
					LDAPException.errorCodeToString(resultCode));
		}
    }

    private static FileOutputStream getTempFile( String name )
               throws IOException {
	    int num = 0;
		File f;
		String filename;
		do {
		    filename = name + '.' + num;
		    f = new File( filename );
			num++;
		} while ( f.exists() );
		printString(name + m_sep + " " + filename);
		return new FileOutputStream( f );
	}

	/**
	 * Internal variables
	 */
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
}
