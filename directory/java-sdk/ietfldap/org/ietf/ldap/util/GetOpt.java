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
package org.ietf.ldap.util;
import java.util.*;

/**
 * This class is similar to the <CODE>getopt()</CODE> function in
 * UNIX System V. You can use this class to parse command-line
 * arguments.
 * <P>
 *
 * When you create an object of this class, you specify a string
 * containing the command-line options that you want to check for.
 * The string should contain the letters of these options. If an
 * option requires an argument (for example, "-h <hostname>"),
 * you should add a colon after the letter in this string.
 * <P>
 *
 * For example, in the following string, the <CODE>-h</CODE>,
 * <CODE>-p</CODE>, <CODE>-D,</CODE>, and <CODE>-w</CODE> options
 * all require arguments.  The <CODE>-H</CODE> option does not
 * require any arguments.
 * <PRE>
 * "h:p:D:w:H"
 * </PRE>
 *
 * You can use the <CODE>hasOption</CODE> method to determine if
 * an option has been specified and the <CODE>getOptionParam</CODE>
 * method to get the argument specified after a particular option.
 * <P>
 *
 * If an option not specified in the string is passed in as an
 * argument, the <CODE>GetOpt</CODE> object prints out an error
 * message.  Note that the object does not throw an exception or
 * exit the application if an invalid option is specified.
 * <P>
 *
 * Note that you are still responsible for verifying that any
 * required arguments have been specified.
 * <P>
 *
 * The following example parses the command-line arguments for
 * the hostname, port number, DN, and password to use when
 * connecting and authenticating to an LDAP server.
 * <PRE>
 * import org.ietf.ldap.*;
 * import org.ietf.ldap.controls.*;
 * import org.ietf.ldap.util.*;
 * import java.util.*;
 *
 * public class SearchDirectory {
 *
 *     public static void main( String[] args )
 *     {
 *
 *         String usage = "Usage: java SearchDirectory -h <host> -p <port> "
 *                      + "[-D <bind dn>] [-w <password>]"
 *
 *         int portnumber = LDAPConnection.DEFAULT_PORT;
 *
 *         // Check for these options. -H means to print out a usage message.
 *         GetOpt options = new GetOpt( "h:p:D:w:H", args );
 *
 *         // Get the arguments specified for each option.
 *         String hostname = options.getOptionParam( 'h' );
 *         String port = options.getOptionParam( 'p' );
 *         String bindDN = options.getOptionParam( 'D' );
 *         String bindPW = options.getOptionParam( 'w' );
 *
 *         // Check to see if the hostname (which is mandatory)
 *         // is not specified or if the user simply wants to
 *         // see the usage message (-H).
 *         if ( hostname == null || options.hasOption( 'H' ) ) {
 *             System.out.println( usage );
 *             System.exit( 1 );
 *         }
 *
 *         // If a port number was specified, convert the port value
 *         //  to an integer.
 *         if ( port != null ) {
 *             try {
 *                 portnumber = java.lang.Integer.parseInt( port );
 *             } catch ( java.lang.Exception e ) {
 *                 System.out.println( "Invalid port number: " + port );
 *                 System.out.println( usage );
 *                 System.exit( 1 );
 *             }
 *         }
 *
 *         // Create a new connection.
 *         LDAPConnection ld = new LDAPConnection();
 *
 *         try {
 *             // Connect and authenticate to server.
 *             ld.connect( 3, hostname, portnumber, bindDN, bindPW );
 *             ...
 *         } catch ( LDAPException e ) {
 *             System.out.println( "Error: " + e.toString() );
 *         }
 *         ...
 *     }
 * }
 * </PRE>
 *
 * @version 1.0
 */
public class GetOpt implements java.io.Serializable {
    /**
     * Internal variables
     */
    private int m_pos;
    private String optarg;
    private String m_control;
    private Vector m_option;
    private Vector m_ParameterList;
    private Hashtable m_optionHashTable;
    private Hashtable m_optionParamHashTable;
    static final long serialVersionUID = -2570196909939660248L;

    /**
     * Constructs a <CODE>GetOpt</CODE> object.
     * @param strControl a string specifying the letters of
     * all available options. If an option requires an argument
     * (for example, "-h <hostname>"), use a colon after the
     * letter for that option (for example, "h:p:D:w:H").
     * @param args an array of strings representing the list
     * of arguments to parse (for example, the
     * array passed into Main).
     */
    public GetOpt(String strControl, String args[]) {
        m_option = new Vector();
        m_control = strControl;
        m_optionHashTable = new Hashtable();
        m_optionParamHashTable = new Hashtable();
        m_ParameterList = new Vector();

        for (int i=0;i<args.length ;i++ ) {
            String sOpt = args[i];
                if (sOpt.length()>0) {
                    if ((sOpt.charAt(0)=='-') ||
                        (sOpt.charAt(0)=='/')) {
                        if (sOpt.length()>1) {
                            int nIndex = m_control.indexOf(sOpt.charAt(1));
                            if (nIndex == (-1)) {
                                System.err.println("Invalid usage. No option -" +
                                    sOpt.charAt(1));
                            } else {
                                char cOpt[]= new char[1];
                                cOpt[0]= sOpt.charAt(1);
                                String sName = new String(cOpt);
                                m_optionHashTable.put(sName,"1");
                                if ((m_control != null) && (m_control.length() > (nIndex+1))) {
                                    if (m_control.charAt(nIndex+1)==':') {
                                        i++;
                                        if (i < args.length)
                                            m_optionParamHashTable.put(sName,args[i]);
                                        else
                                            System.err.println("Missing argument for option "+
                                                sOpt);
                                    }
                                }
                            }
                        } else {
                        System.err.println("Invalid usage.");
                    }
                } else {
                    // probably parameters
                    m_ParameterList.addElement(args[i]);
                }
            }
        }
    }

    /**
     * Determines if an option was specified. For example,
     * <CODE>hasOption( 'H' )</CODE> checks if the -H option
     * was specified.
     * <P>
     *
     * @param c letter of the option to check
     * @return <code>true</code> if the option was specified.
     */
    public boolean hasOption(char c) {
        boolean fReturn = false;
        char cOption[]=new char[1];
        cOption[0]=c;
        String s = new String(cOption);
        if (m_optionHashTable.get(s)=="1") {
            fReturn = true;
        }
        return(fReturn);
    }

    /**
     * Gets the argument specified with an option.
     * For example, <CODE>getOptionParameter( 'h' )</CODE>
     * gets the value of the argument specified with
     * the -h option (such as "localhost" in "-h localhost").
     * <P>
     *
     * @param c the letter of the option to check
     * @return the argument specified for this option.
     */
    public String getOptionParam(char c) {
        char cOption[] = new char[1];
        cOption[0]=c;
        String s = new String(cOption);
        String sReturn=(String)m_optionParamHashTable.get(s);
        return(sReturn);
    }

    /**
     * Gets a list of any additional parameters specified
     * (not including the arguments for any options).
     * @return a list of the additional parameters.
     */
    public Vector getParameters() {
        return(m_ParameterList);
    }
}
