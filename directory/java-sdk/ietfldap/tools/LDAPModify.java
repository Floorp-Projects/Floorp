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
/*
 * @(#) LDAPModify.java
 *
 */

import java.io.*;
import java.net.*;
import java.util.*;
import org.ietf.ldap.*;
import org.ietf.ldap.util.*;

/**
 * Executes modify, delete, add, and modRDN.
 * This class is implemented based on the java LDAP classes.
 *
 * <pre>
 * usage       : java LDAPModify [options]
 * example     : java LDAPModify -D "uid=johnj,ou=People,o=Airius.com"
 *                 -w "password" -h ldap.netscape.com -p 389
 *                 -f modify.cfg
 *
 * options: {np = no parameters, p = requires parameters}
 *  'D' bind DN --------------------------------------------- p
 *  'w' bind password --------------------------------------- p
 *  'f' input file ------------------------------------------ p
 *  'h' LDAP host ------------------------------------------- p
 *  'p' LDAP port ------------------------------------------- p
 *  'e' record rejected records in a text file -------------- p
 *  'c' continuous, do not stop on error ------------------- np
 *  'a' add, if no operation is specified ------------------ np
 *  'r' replace, if no operation is specified -------------- np
 *  'b' binary, read values starting with / from a file ---- np
 *  'd' debugging level ------------------------------------- p
 *  'V' version, specify LDAP protocol version (2 or 3) ----- p
 *  'R' do not follow referrals ---------------------------- np
 *  'O' hop limit ------------------------------------------- p
 *  'H' help, display usage--------------------------------- np
 *  'M' manage referrals, do not follow them --------------- np
 *  'n' show what would be done but do not do it ----------- np
 *  'v' verbose mode --------------------------------------- np
 *  'e' reject file, where to list rejected records --------- p
 *  'y' proxy DN, DN to use for access control -------------- p
 *
 * note: '-' or '/' is used to mark an option field.
 *       e.g. -a -b /c /d parameter -e parameter
 *
 * </pre>
 *
 * @version 1.0
 */

public class LDAPModify extends LDAPTool { /* LDAPModify */

    public static void main(String args[]) { /* main */

        /* extract parameters from the arguments list */
        extractParameters(args);

        /* perform an LDAP client connection operation */
        try {
            if (!m_justShow) {
                m_client = new LDAPConnection();
                m_client.connect( m_ldaphost, m_ldapport );
            }
        } catch(Exception e) {
            System.err.println("Error: client connection failed!");
            System.exit(0);
        }

        /* perform an LDAP bind operation */
        if (!m_justShow) {
			if ( (m_binddn != null) && (m_passwd != null) ) {
			    try {
                    m_client.bind( m_version, m_binddn, m_passwd.getBytes() );
			    } catch (Exception e) {
                    System.err.println( e.toString() );
                    System.exit(0);
			    }
			}
        }

        try {
            if ( m_file != null )
                m_ldif = new LDIF(m_file);
            else {
                m_ldif = new LDIF();
}
        } catch (Exception e) {
            if ( m_file == null )
                m_file = "stdin";
            System.err.println("Failed to read LDIF file " + m_file +
                               ", " + e.toString());
            System.exit(0);
        }

        /* performs a JDAP Modify operation */
        try {
            doModify();
        } catch (Exception e) {
            System.err.println( e.toString() );
        }

        /* disconnect */
        try {
            if (!m_justShow)
                m_client.disconnect();
        } catch (Exception e) {
            System.err.println( e.toString() );
        }
        System.exit(0);
    } /* main */

    /**
     * Prints usage.
     */
    private static void doUsage() {
        System.err.println( "usage: LDAPModify [options]" );
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
        System.err.println("  -R            do not automatically follow " +
                           "referrals");
        System.err.println("  -O hop limit  maximum number of referral " +
                           "hops to traverse");
        System.err.println("  -H            display usage information");
        System.err.println("  -c            continuous mode (do not " +
                           "stop on errors)");
        System.err.println("  -M            manage references (treat them " +
                           "as regular entries)");
        System.err.println("  -f file       read modifications from " +
                           "file instead of standard input");
        System.err.println("  -a            add entries");
        System.err.println("  -b            read values that start with " +
                           "/ from files (for bin attrs)");
        System.err.println("  -n            show what would be done but " +
                           "don\'t actually do it");
        System.err.println("  -v            run in verbose mode");
        System.err.println("  -r            replace existing values by " +
                           "default");
        System.err.println("  -e rejectfile save rejected entries in " +
                           "\'rejfile\'");
        System.err.println("  -y proxy-DN   DN to use for access control");
    }

    /**
     * This class-method is used to extract specified parameters from the
     * arguments list.
     */
    /* extract parameters */
    protected static void extractParameters(String args[]) {

        String privateOpts = "abcHFre:f:";

        GetOpt options = LDAPTool.extractParameters( privateOpts, args );

        /* -H Help */
        if (options.hasOption('H')) {
            doUsage();
            System.exit(0);
        } /* Help */

        if (options.hasOption('F'))
            m_force = true;

        if (options.hasOption('a'))
            m_add = true;

        if (options.hasOption('c'))
            m_continuous = true;

        if (options.hasOption('r'))
            m_add = false;

        if (options.hasOption('b'))
            m_binaryFiles = true;

        /* -f input file */
        if(options.hasOption('f')) { /* Is input file */
            m_file = (String)options.getOptionParam('f');
        } /* End Is input file */

        /* -e rejects file */
        if(options.hasOption('e')) { /* rejects file */
            m_rejectsFile = (String)options.getOptionParam('e');
        } /* End rejects file */

    } /* extract parameters */

    /**
     * Call the LDAPConnection modify operation with the
     * specified options, and/or parameters.
     */
    private static void doModify() throws IOException { /* doModify */
        PrintWriter reject = null;
        LDAPConstraints baseCons = null;
        if (!m_justShow) {
            baseCons = m_client.getConstraints();
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
                baseCons.setControls(controls);
            }
            baseCons.setReferralFollowing( m_referrals );
            if ( m_referrals ) {
                setDefaultReferralCredentials( baseCons );
            }
            baseCons.setHopLimit( m_hopLimit );
        }

        LDIFRecord rec = m_ldif.nextRecord();

        for (; rec != null; rec = m_ldif.nextRecord() ) {
            LDAPConstraints cons = baseCons;
            if (!m_justShow) {
                // Were there any controls specified in the LDIF
                // record?
                LDAPControl[] newControls = rec.getControls();
                if ( newControls != null ) {
                    LDAPControl[] controls = newControls;
                    LDAPControl[] oldControls = baseCons.getControls();
                    if ( oldControls != null ) {
                        // If there were already controls defined, merge in
                        // the new ones
                        controls =
                            new LDAPControl[oldControls.length +
                                           newControls.length];
                        for( int i = 0; i < oldControls.length; i++ ) {
                            controls[i] = oldControls[i];
                        }
                        for( int i = 0; i < newControls.length; i++ ) {
                            controls[i+oldControls.length] = newControls[i];
                        }
                    }
                    // Assign the merged controls to a copy of the constraints
                    cons = (LDAPSearchConstraints)baseCons.clone();
                    cons.setControls( controls );
                }
            }

            LDIFContent content = rec.getContent();
            LDAPModification mods[] = null;
            LDAPAttribute addAttrs[] = null;
            boolean doDelete = false;
            boolean doModDN = false;
            LDAPEntry newEntry = null;

            /* What type of record is this? */
            if ( content instanceof LDIFModifyContent ) {
                mods = ((LDIFModifyContent)content).getModifications();
            } else if ( content instanceof LDIFAddContent ) {
                addAttrs = ((LDIFAddContent)content).getAttributes();
            } else if ( content instanceof LDIFAttributeContent ) {
                /* No change type; decide what to do based on options */
                if ( m_add ) {
                    addAttrs =
                        ((LDIFAttributeContent)content).getAttributes();
                } else {
                    LDAPAttribute[] tmpAttrs =
                        ((LDIFAttributeContent)content).getAttributes();
                    mods = new LDAPModification[tmpAttrs.length];
                    for( int ta = 0; ta < tmpAttrs.length; ta++ ) {
                        mods[ta] = new LDAPModification(
                            LDAPModification.REPLACE, tmpAttrs[ta] );
                    }
                }
            } else if ( content instanceof LDIFDeleteContent ) {
                doDelete = true;
            } else if (content instanceof LDIFModDNContent ) {
                doModDN = true;
            } else {
            }

            /* Prepare for adding */
            if ( addAttrs != null ) {
                LDAPAttributeSet newAttrSet = new LDAPAttributeSet();
                for( int a = 0; a < addAttrs.length; a++ )
                    newAttrSet.add( addAttrs[a] );
                newEntry = new LDAPEntry( rec.getDN(), newAttrSet );
            }

            /* Get values from files? */
            boolean skip = false;
            if ( m_binaryFiles ) {
                /* Check each value of each attribute, to see if it
                   needs replacing with the contents of a file */
                if ( mods != null ) {
                    for( int m = 0; m < mods.length; m++ ) {
                        LDAPModification mod = mods[m];
                        LDAPAttribute attr = mods[m].getAttribute();

                        LDAPAttribute newAttr = checkFiles( attr );
                        if ( newAttr == null )
                            skip = true;
                        else
                            mods[m] = new LDAPModification(
                                mod.getOp(), newAttr );
                    }
                } else if ( addAttrs != null ) {
                    LDAPAttributeSet newAttrSet = new LDAPAttributeSet();
                    for( int a = 0; a < addAttrs.length; a++ ) {
                        LDAPAttribute attr = addAttrs[a];

                        LDAPAttribute newAttr = checkFiles( attr );
                        if ( newAttr == null ) {
                            skip = true;
                            break;
                        } else {
                            newAttrSet.add( newAttr );
                        }
                    }
                    if ( !skip ) {
                        newEntry = new LDAPEntry( rec.getDN(), newAttrSet );
                    }
                }
            }

            /* Do the directory operation */
            int errCode = 0;
            if ( !skip ) {
                try {
                    if ( mods != null ) {
                        System.out.println("\nmodifying entry "+rec.getDN() );
                        for( int m = 0; m < mods.length; m++ ) {
                            if (m_verbose) {
                                System.out.println("\t"+mods[m] );
			    }
                        }

                        if (!m_justShow) {
                            m_client.modify( rec.getDN(), mods, cons );
                        }
                    } else if ( newEntry != null ) {
                        System.out.println( "\nadding new entry " + newEntry.getDN() );
                        if ( m_verbose ) {
                            LDAPAttributeSet set = newEntry.getAttributeSet();
			    Iterator it = set.iterator();
			    while( it.hasNext() ) {
                                System.out.println("\t"+it.next() );
                            }
                        } 
                        if (!m_justShow) {
                            m_client.add( newEntry, cons );
                        }
                    } else if ( doDelete ) {
                        System.out.println( "\ndeleting entry " + rec.getDN() );
                        if (!m_justShow)
                            m_client.delete( rec.getDN(), cons );
                    } else if ( doModDN) {
                        System.out.println( "\nmodifying RDN of entry " + 
                            rec.getDN()+" and/or moving it beneath a new parent");
                        if ( m_verbose ) {
                            System.out.println( "\t"+content.toString());
                        }
                        if (!m_justShow) {
                            LDIFModDNContent moddnContent = (LDIFModDNContent)content;
                            m_client.rename( rec.getDN(), moddnContent.getRDN(),
                                moddnContent.getNewParent(), 
                                moddnContent.getDeleteOldRDN(), cons );
                            System.out.println( "rename completed");
                        }
                    }
                } catch (LDAPException e) {
                    System.err.println( rec.getDN() + ": " +
                                        e.resultCodeToString() );
                    if (e.getLDAPErrorMessage() != null)
                        System.err.println( "additional info: " +
                                        e.getLDAPErrorMessage() );
                    if ( !m_continuous ) {
                        System.exit(1);
                    }
                    skip = true;
                    errCode = e.getResultCode();
                }
            }

            /* Write to rejects file? */
            if ( skip && (m_rejectsFile != null) ) {
                try {
                    if ( reject == null ) {
                        reject = new PrintWriter(
                            new FileOutputStream( m_rejectsFile ) );
                    }
                } catch ( Exception e ) {
                }
                if ( reject != null ) {
                    reject.println( "dn: "+rec.getDN()+ " # Error: " +
                                    errCode );
                    if ( mods != null ) {
                        for( int m = 0; m < mods.length; m++ ) {
                            reject.println( mods[m].toString() );
                        }
                    } else if ( newEntry != null ) {
                        reject.println( "Add " + newEntry.toString() );
                    } else if ( doDelete ) {
                        reject.println( "Delete " + rec.getDN() );
                    } else if (doModDN) {
                        reject.println( "ModDN "+ 
                                    ((LDIFModDNContent)content).toString() );
                    }
                    reject.flush();
                }
            }
        }
        System.exit(0);
    } /* doModify */


    /**
     * Read in binary data for values specified with a leading /
     * @param attr Source attribute.
     * @return Updated attribute.
     **/
    private static LDAPAttribute checkFiles ( LDAPAttribute attr ) {
        LDAPAttribute newAttr =
            new LDAPAttribute( attr.getName() );

        /* Check each value and see if it is a file name */
        Enumeration e = attr.getStringValues();
        if (e != null) {
          while ( e.hasMoreElements() ) {
            String val = (String)e.nextElement();
            if ( (val != null) && (val.length() > 1)) {
                try {
                    File file = new File( val );
                    FileInputStream fi =
                        new FileInputStream( file );
                    byte[] bval = new byte[(int)file.length()];
                    fi.read( bval, 0, (int)file.length() );
                    newAttr.addValue( bval );
                } catch (FileNotFoundException ex) {
                    newAttr.addValue(val) ;
                } catch ( IOException ex ) {
                    System.err.println( "Unable to read value " +
                        "from file " + val );
                    if ( !m_continuous )
                        System.exit(1);
                    newAttr = null;
                }
            } else {
                newAttr.addValue( val );
            }
          }
        }
        else
          System.err.println("Failed to do string conversion for "+attr.getName());
        return newAttr;
    }

  private static boolean m_continuous = false;
  private static boolean m_force = false;
  private static boolean m_add = false;
  private static boolean m_binaryFiles = false;
  private static String m_rejectsFile = null;
  private static LDIF m_ldif = null;
  private static String m_file = null;
} /* LDAPModify */
