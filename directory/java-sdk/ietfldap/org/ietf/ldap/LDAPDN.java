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
package org.ietf.ldap;

import java.util.*;
import org.ietf.ldap.util.*;
import java.io.*;

/**
 * Represents a distinguished name in LDAP.
 * <P>
 *
 * You can use objects of this class to split a distinguished name
 * (DN) into its individual components.  You can also escape the
 * characters in a DN.
 * <P>
 *
 * @version 1.0
 */
public class LDAPDN {

    /**
     * Returns the individual components of a relative distinguished name (RDN).
     * @param rdn relative distinguished name of which you want to get the components.
     * @param noTypes if <CODE>true</CODE>, returns only the values of the
     * components and not the names (such as 'cn=')
     * @return an array of strings representing the components of the RDN.
     * @see org.ietf.ldap.LDAPDN#explodeDN(java.lang.String, boolean)
     */
    public static String[] explodeRDN (String rdn, boolean noTypes) {
        RDN name = new RDN(rdn);
        if ( noTypes ) {
            return name.getValues();
        } else {
            String[] str = new String[1];
            str[0] = name.toString();
            return str;
        }
    }

    /** 
     * Compares two dn's for equality.
     * @param dn1 the first dn to compare
     * @param dn2 the second dn to compare
     * @return true if the two dn's are equal
     */
    public static boolean equals(String dn1, String dn2) {
        return normalize(dn1).equals(normalize(dn2));
    }

    /**
     * Returns the RDN after escaping the characters specified
     * by <CODE>org.ietf.ldap.util.DN.ESCAPED_CHAR</CODE>.
     * <P>
     *
     * @param rdn the RDN to escape
     * @return the RDN with the characters escaped.
     * @see org.ietf.ldap.util.DN#ESCAPED_CHAR
     * @see org.ietf.ldap.LDAPDN#unEscapeRDN(java.lang.String)
     */
    public static String escapeRDN(String rdn) {

        RDN name = new RDN(rdn);
        String[] val = name.getValues();
        if (val == null)
            return rdn;

        StringBuffer[] buffer = new StringBuffer[val.length];
        StringBuffer retbuf = new StringBuffer();
        String[] types = name.getTypes();

        for (int j = 0; j < val.length; j++ ) {
            buffer[j] = new StringBuffer(val[j]);

            int i=0;
            while (i<buffer[j].length()) {
                if (isEscape(buffer[j].charAt(i))) {
                    buffer[j].insert(i, '\\');
                    i++;
                }
                
                i++;
            }

            retbuf.append( ((retbuf.length() > 0) ? " + " : "") + types[j] + "=" +
                           ( new String( buffer[j] ) ) );
        }

        return new String( retbuf );
    }

    /**
     * Returns the individual components of a distinguished name (DN).
     * @param dn distinguished name of which you want to get the components.
     * @param noTypes if <CODE>true</CODE>, returns only the values of the
     * components and not the names (such as 'cn=')
     * @return an array of strings representing the components of the DN.
     * @see org.ietf.ldap.LDAPDN#explodeRDN(java.lang.String, boolean)
     */
    public static String[] explodeDN (String dn, boolean noTypes) {
        DN name = new DN(dn);
        return name.explodeDN(noTypes);
    }

    /**
     * Returns true if the string conforms to distinguished name syntax;
     * NOT IMPLEMENTED YET
     *
     * @param dn string to evaluate for distinguished name syntax
     * @return true if the string conforms to distinguished name syntax
     */
    public boolean isValid( String dn ) {
        return true;
    }

    /** 
     * Normalizes the dn.
     * @param dn the DN to normalize
     * @return the normalized DN
     */
    public static String normalize(String dn) {
        return (new DN(dn)).toString();
    }
    
    /**
     * Returns the RDN after unescaping any escaped characters.
     * For a list of characters that are typically escaped in a
     * DN, see <CODE>org.ietf.ldap.LDAPDN.ESCAPED_CHAR</CODE>.
     * <P>
     *
     * @param rdn the RDN to unescape
     * @return the unescaped RDN.
     * @see org.ietf.ldap.util.DN#ESCAPED_CHAR
     * @see org.ietf.ldap.LDAPDN#escapeRDN(java.lang.String)
     */
    public static String unEscapeRDN(String rdn) {
        RDN name = new RDN(rdn);
        String[] vals = name.getValues();
        if ( (vals == null) || (vals.length < 1) )
            return rdn;

        StringBuffer buffer = new StringBuffer(vals[0]);
        StringBuffer copy = new StringBuffer();
        int i=0;
        while (i<buffer.length()) {
            char c = buffer.charAt(i++);
            if (c != '\\') {
                copy.append(c);
            }
            else { // copy the escaped char following the back slash
                if (i<buffer.length()) {
                    copy.append(buffer.charAt(i++));
                }
            }
        }

        return name.getTypes()[0]+"="+(new String(copy));
    }

    private static boolean isEscape(char c) {
        for (int i=0; i<DN.ESCAPED_CHAR.length; i++)
            if (c == DN.ESCAPED_CHAR[i])
                return true;
        return false;
    }
}
