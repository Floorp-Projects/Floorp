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
import netscape.ldap.util.*;
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
     * Returns the individual components of a distinguished name (DN).
     * @param dn distinguished name of which you want to get the components.
     * @param noTypes if <CODE>true</CODE>, returns only the values of the
     * components and not the names (such as 'cn=')
     * @return an array of strings representing the components of the DN.
     * @see netscape.ldap.LDAPDN#explodeRDN(java.lang.String, boolean)
     */
    public static String[] explodeDN (String dn, boolean noTypes) {
        DN name = new DN(dn);
        return name.explodeDN(noTypes);
    }

    /**
     * Returns the individual components of a relative distinguished name (RDN).
     * @param rdn relative distinguished name of which you want to get the components.
     * @param noTypes if <CODE>true</CODE>, returns only the values of the
     * components and not the names (such as 'cn=')
     * @return an array of strings representing the components of the RDN.
     * @see netscape.ldap.LDAPDN#explodeDN(java.lang.String, boolean)
     */
    public static String[] explodeRDN (String rdn, boolean noTypes) {
        RDN name = new RDN(rdn);
        return name.explodeRDN(noTypes);
    }

    /**
     * Returns the RDN after escaping the characters specified
     * by <CODE>netscape.ldap.util.DN.ESCAPED_CHAR</CODE>.
     * <P>
     *
     * @param rdn the RDN to escape
     * @return the RDN with the characters escaped.
     * @see netscape.ldap.util.DN#ESCAPED_CHAR
     * @see netscape.ldap.LDAPDN#unEscapeRDN(java.lang.String)
     */
    public static String escapeRDN(String rdn) {

        RDN name = new RDN(rdn);
        String val = name.getValue();
        if (val == null)
            return rdn;

        StringBuffer buffer = new StringBuffer(val);

        int i=0;
        while (i<buffer.length()) {
            if (isEscape(buffer.charAt(i))) {
                buffer.insert(i, '\\');
                i++;
            }

            i++;
        }

        return name.getType()+"="+(new String(buffer));
    }

    /**
     * Returns the RDN after unescaping any escaped characters.
     * For a list of characters that are typically escaped in a
     * DN, see <CODE>netscape.ldap.LDAPDN.ESCAPED_CHAR</CODE>.
     * <P>
     *
     * @param rdn the RDN to unescape
     * @return the unescaped RDN.
     * @see netscape.ldap.util.DN#ESCAPED_CHAR
     * @see netscape.ldap.LDAPDN#escapeRDN(java.lang.String)
     */
    public static String unEscapeRDN(String rdn) {
        RDN name = new RDN(rdn);
        String val = name.getValue();
        if (val == null)
            return rdn;

        StringBuffer buffer = new StringBuffer(val);
        StringBuffer copy = new StringBuffer();
        int i=0;
        while (i<buffer.length()) {
            char c = buffer.charAt(i);
            if (c != '\\')
                copy.append(c);

            i++;
        }

        return name.getType()+"="+(new String(copy));
    }

    private static boolean isEscape(char c) {
        for (int i=0; i<DN.ESCAPED_CHAR.length; i++)
            if (c == DN.ESCAPED_CHAR[i])
                return true;
        return false;
    }
}
