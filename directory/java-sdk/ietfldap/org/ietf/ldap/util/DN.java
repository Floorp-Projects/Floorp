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
import java.io.*;
import java.util.StringTokenizer;

/**
 * Objects of this class represent distinguished names (DN). A
 * distinguished name is used to identify an entry in a directory.
 * <P>
 *
 * The <CODE>org.ietf.ldap.LDAPDN</CODE> class uses this class
 * internally.  In most cases, when working with DNs in the
 * LDAP Java classes, you should use the
 * <CODE>org.ietf.ldap.LDAPDN</CODE> class.
 * <P>
 *
 * The following DNs are examples of the different formats
 * for DNs that may appear:
 * <UL>
 * <LI>uid=bjensen, ou=People, o=Airius.com
 * (<A HREF="http://ds.internic.net/rfc/rfc1485.txt"
 * TARGET="_blank">RFC 1485</A> format)
 * <LI>o=Airius.com/ou=People/uid=bjensen (OSF format)
 * </UL>
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPDN
 */
public final class DN implements Serializable {

    /**
     * List of RDNs. DN consists of one or more RDNs.
     * RDNs follow RFC1485 order.
     */
    private Vector m_rdns = new Vector();

    /**
     * Type specifying a DN in the RFC format.
     * <P>
     *
     * @see org.ietf.ldap.util.DN#getDNType
     * @see org.ietf.ldap.util.DN#setDNType
     */
    public static int RFC = 0;

    /**
     * Type specifying a DN in the OSF format.
     * <P>
     *
     * @see org.ietf.ldap.util.DN#getDNType
     * @see org.ietf.ldap.util.DN#setDNType
     */
    public static int OSF = 1;

    private int m_dnType = RFC;
    static final long serialVersionUID = -8867457218975952548L;

    /**
     * Constructs an empty <CODE>DN</CODE> object.
     */
    public DN() {
    }

    /**
     * Constructs a <CODE>DN</CODE> object from the specified
     * distinguished name.  The string representation of the DN
     * can be in RFC 1485 or OSF format.
     * <P>
     *
     * @param dn string representation of the distinguished name
     */
    public DN(String dn) {

        String neutralDN = neutralizeEscapes(dn);
        if (neutralDN == null) {
            return; // malformed
        }
        
        // RFC1485
        if (neutralDN.indexOf(',') != -1 || neutralDN.indexOf(';') != -1) {
            parseRDNs(neutralDN, dn, ",;");
        }
        else if (dn.indexOf('/') != -1) { /* OSF */
            m_dnType = OSF;
            StringTokenizer st = new StringTokenizer(dn, "/");
            Vector rdns = new Vector();
            while (st.hasMoreTokens()) {
                String rdn = st.nextToken();
                if (RDN.isRDN(rdn))
                    rdns.addElement(new RDN(rdn));
                else
                    return;
            }
            /* reverse the RDNs order */
            for (int i = rdns.size() - 1; i >= 0; i--) {
                m_rdns.addElement(rdns.elementAt(i));
            }        
        }        
        else if (RDN.isRDN(dn)) {
            m_rdns.addElement(new RDN(dn));
        }
    }

    /**
     * Neutralize backslash escapes and quoted sequences for easy parsing.
     * @return dn string with disabled escapes or null if malformed dn
     */    
    private String neutralizeEscapes(String dn) {
        String neutralDN = RDN.neutralizeEscapes(dn);
        if (neutralDN == null) {
            return null; // malformed
        }
        
        String dn2 = neutralDN.trim();
        if (dn2.length() == 0) {
            return neutralDN;
        }
        if (dn2.charAt(0) == ';' || dn2.charAt(0)  == ',') {
            return null; // malformed
        }
        int lastIdx = dn2.length() -1;
        if (dn2.charAt(lastIdx) == ';' || dn2.charAt(lastIdx) == ',') {
            return null; // malformed
        }
        return neutralDN;
    }
    
    /**
     * Parse RDNs in the DN
     */
    private void parseRDNs(String neutralDN, String dn, String sep) {
        int startIdx=0, endIdx=0;
        RDN rdn = null;
        StringTokenizer tok = new StringTokenizer(neutralDN, sep);
        while (tok.hasMoreElements()) {
            String neutralRDN = tok.nextToken();
            endIdx = startIdx + neutralRDN.length();
            rdn = new RDN (dn.substring(startIdx, endIdx));
            if (rdn.getTypes() != null) {
                m_rdns.addElement(rdn);
            }
            else { // malformed rdn
                m_rdns.removeAllElements();
                return;
            }
            startIdx = endIdx + 1;
        }        
    }

    /**
     * Adds the specified relative distinguished name (RDN) to the
     * beginning of the current DN.
     * <P>
     *
     * @param rdn the relative distinguished name to add to the 
     * beginning of the current DN
     * @see org.ietf.ldap.util.RDN
     */
    public void addRDNToFront(RDN rdn) {
        m_rdns.insertElementAt(rdn, 0);
    }

    /**
     * Adds the specified relative distinguished name (RDN) to the
     * end of the current DN.
     * <P>
     *
     * @param rdn the relative distinguished name to append to the current DN
     * @see org.ietf.ldap.util.RDN
     */
    public void addRDNToBack(RDN rdn) {
        m_rdns.addElement(rdn);
    }

    /**
     * Adds the specified relative distinguished name (RDN) to the current DN.
     * If the DN is in RFC 1485 format, the RDN is added to the beginning
     * of the DN.  If the DN is in OSF format, the RDN is appended to the
     * end of the DN.
     * <P>
     *
     * @param rdn the relative distinguished name to add to the current DN
     * @see org.ietf.ldap.util.RDN
     */
    public void addRDN(RDN rdn) {
        if (m_dnType == RFC) {
            addRDNToFront(rdn);
        } else {
            addRDNToBack(rdn);
        }
    }

    /**
     * Sets the type of format used for the DN (RFC format or OSF format).
     * <P>
     *
     * @param type one of the following constants: <CODE>DN.RFC</CODE>
     * (to use the RFC format) or <CODE>DN.OSF</CODE> (to use the OSF format)
     * @see org.ietf.ldap.util.DN#getDNType
     * @see org.ietf.ldap.util.DN#RFC
     * @see org.ietf.ldap.util.DN#OSF
     */
    public void setDNType(int type) {
        m_dnType = type;
    }

    /**
     * Gets the type of format used for the DN (RFC format or OSF format).
     * <P>
     *
     * @return one of the following constants: <CODE>DN.RFC</CODE>
     * (if the DN is in RFC format) or <CODE>DN.OSF</CODE>
     * (if the DN is in OSF format).
     * @see org.ietf.ldap.util.DN#setDNType
     * @see org.ietf.ldap.util.DN#RFC
     * @see org.ietf.ldap.util.DN#OSF
     */
    public int getDNType() {
        return m_dnType;
    }

    /**
     * Returns the number of components that make up the current DN.
     * @return the number of components in this DN.
     */
    public int countRDNs() {
        return m_rdns.size();
    }

    /**
     * Returns a list of the components (<CODE>RDN</CODE> objects)
     * that make up the current DN.
     * @return a list of the components of this DN.
     * @see org.ietf.ldap.util.RDN
     */
    public Vector getRDNs() {
        return m_rdns;
    }

    /**
     * Returns an array of the individual components that make up
     * the current distinguished name.
     * @param noTypes specify <code>true</code> to remove the attribute type
     * and equals sign (for example, "cn=") from each component
     */
    public String[] explodeDN(boolean noTypes) {
        if (m_rdns.size() == 0)
            return null;
        String str[] = new String[m_rdns.size()];
        for (int i = 0; i < m_rdns.size(); i++) {
            if (noTypes)
                str[i] = ((RDN)m_rdns.elementAt(i)).getValue();
            else
                str[i] = ((RDN)m_rdns.elementAt(i)).toString();
        }
        return str;
    }

    /**
     * Determines if the DN is in RFC 1485 format.
     * @return <code>true</code> if the DN is in RFC 1485 format.
     */
    public boolean isRFC() {
        return (m_dnType == RFC);
    }

    /**
     * Returns the DN in RFC 1485 format.
     * @return the DN in RFC 1485 format.
     */
    public String toRFCString() {
        String dn = "";
        for (int i = 0; i < m_rdns.size(); i++) {
            if (i != 0)
                dn += ",";
            dn = dn + ((RDN)m_rdns.elementAt(i)).toString();
        }
        return dn;
    }

    /**
     * Returns the DN in OSF format.
     * @return the DN in OSF format.
     */
    public String toOSFString() {
        String dn = "";
        for (int i = 0; i < m_rdns.size(); i++) {
            if (i != 0) {
                dn = "/" + dn;
            }
            RDN rdn = (RDN)m_rdns.elementAt(i);
            dn = rdn.toString() + dn;
        }
        return dn;
    }

    /**
     * Returns the string representation of the DN
     * in its original format. (For example, if the
     * <CODE>DN</CODE> object was constructed from a DN
     * in RFC 1485 format, this method returns the DN
     * in RFC 1485 format.
     * @return the string representation of the DN.
     */
    public String toString() {
        if (m_dnType == RFC)
            return toRFCString();
        else
            return toOSFString();
    }

    /**
     * Determines if the given string is an distinguished name or
     * not.
     * @param dn distinguished name
     * @return <code>true</code> or <code>false</code>.
     */
    public static boolean isDN(String dn) {
        if ( dn.equals( "" ) ) {
            return true;
        }
        DN newdn = new DN(dn);
        return (newdn.countRDNs() > 0);
    }

    /**
     * Determines if the current DN is equal to the specified DN.
     * @param dn DN to compare against the current DN
     * @return <code>true</code> if the two DNs are the same.
     */
    public boolean equals(DN dn) {
        return (dn.toRFCString().toUpperCase().equals(toRFCString().toUpperCase()));
    }

    /**
     * Gets the parent DN for this DN.
     * <P>
     *
     * For example, the following section of code gets the
     * parent DN of "uid=bjensen, ou=People, o=Airius.com."
     * <PRE>
     *    DN dn = new DN("uid=bjensen, ou=People, o=Airius.com");
     *    DN parent = dn.getParent();
     * </PRE>
     * The parent DN in this example is "ou=People, o=Airius.com".
     * <P>
     *
     * @return DN of the parent of this DN.
     */
    public DN getParent() {
        DN newdn = new DN();
        for (int i = m_rdns.size() - 1; i > 0; i--) {
            newdn.addRDN((RDN)m_rdns.elementAt(i));
        }
        return newdn;
    }

    /**
     * Determines if the given DN is under the subtree defined by this DN.
     * <P>
     *
     * For example, the following section of code determines if the
     * DN specified by <CODE>dn1</CODE> is under the subtree specified
     * by <CODE>dn2</CODE>.
     * <PRE>
     *    DN dn1 = new DN("uid=bjensen, ou=People, o=Airius.com");
     *    DN dn2 = new DN("ou=People, o=Airius.com");
     *
     *    boolean isContain = dn1.contains(dn2)
     * </PRE>
     * In this case, since "uid=bjensen, ou=People, o=Airius.com"
     * is an entry under the subtree "ou=People, o=Airius.com",
     * the value of <CODE>isContain</CODE> is true.
     * <P>
     *
     * @param dn the DN of a subtree to check
     * @return <code>true</code> if the current DN belongs to the subtree
     * specified by <CODE>dn</CODE>.
     * @deprecated Please use isDescendantOf() instead.
     */

    public boolean contains(DN dn) {
        
        return isDescendantOf(dn);
    }
    
    /**
     * Determines if this DN is a descendant of the given DN.
     * <P>
     *
     * For example, the following section of code determines if the
     * DN specified by <CODE>dn1</CODE> is a descendant of the DN specified
     * by <CODE>dn2</CODE>.
     * <PRE>
     *    DN dn1 = new DN("uid=bjensen, ou=People, o=Airius.com");
     *    DN dn2 = new DN("ou=People, o=Airius.com");
     *
     *    boolean isDescendant = dn1.isDescendantOf(dn2)
     * </PRE>
     * In this case, since "uid=bjensen, ou=People, o=Airius.com"
     * is an entry under the subtree "ou=People, o=Airius.com",
     * the value of <CODE>isDescendant</CODE> is true.
     * <P>
     *
     * In the case where the given DN is equal to this DN
     * it returns false.
     *
     * @param dn the DN of a subtree to check
     * @return <code>true</code> if the current DN is a descendant of the DN
     * specified by <CODE>dn</CODE>.
     */

    public boolean isDescendantOf(DN dn) {

        Vector rdns1 = dn.m_rdns;

        Vector rdns2 = this.m_rdns;

        int i = rdns1.size() - 1;
        int j = rdns2.size() - 1;
        
        if ((j < i) || (equals(dn) == true)) 
          return false;

        for (; i>=0 && j>=0; i--, j--) {
            RDN rdn1 = (RDN)rdns1.elementAt(i);
            RDN rdn2 = (RDN)rdns2.elementAt(j);
            if (!rdn2.equals(rdn1)) {
                return false;
            }
        }

        return true;
    }

    /**
     * Array of the characters that may be escaped in a DN.
     */
    public static final char[] ESCAPED_CHAR = {',', '+', '"', '\\', '<', '>', ';'};
}
