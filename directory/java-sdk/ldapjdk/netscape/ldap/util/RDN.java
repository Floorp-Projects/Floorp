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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap.util;

import java.util.StringTokenizer;

/**
 * Objects of this class represent the components of a distinguished
 * name (DN).  (In some situations, these components are referred to
 * as relative distinguished names, or RDNs.)  For example, the
 * DN "uid=bjensen, ou=People, o=Airius.com" has three components:
 * "uid=bjensen", "ou=People", and "o=Airius.com".
 * <P>
 *
 * Each DN component consists of an attribute type and a value.
 * For example, in "o=Airius.com", the attribute type is "o"
 * and the value is "Airius.com".
 * <P>
 *
 * You can use objects of this class to add components to an
 * existing <CODE>DN</CODE> object.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.util.DN
 */
public final class RDN implements java.io.Serializable {

    static final long serialVersionUID = 7895454691174650321L;

    /**
     * List of RDNs. DN consists of one or more RDNs.
     */
    private String m_type = null;
    private String m_value = null;

    /**
     * Constructs a new <CODE>RDN</CODE> object from the specified
     * DN component.
     * @param rdn DN component.
     */
    public RDN(String rdn) {
        int index = rdn.indexOf("=");

        // if the rdn doesnt have = or = positions right at the beginning of the rdn
        if (index <= 0)
            return;

        m_type = rdn.substring(0, index).trim();
        m_value = rdn.substring(index+1).trim();
    }

    /**
     * Returns the DN component as the first element in an
     * array of strings.
     * @param noType Specify true to ignore the attribute type and
     * equals sign (for example, "cn=") and return only the value.
     * @return An array of strings representing the DN component.
     */
    public String[] explodeRDN(boolean noType) {
        if (m_type == null)
            return null;
        String str[] = new String[1];
        if (noType) {
            str[0] = getValue();
        } else {
            str[0] = toString();
        }
        return str;
    }

    /**
     * Returns the attribute type of the DN component.
     * @return rdn The attribute type of the DN component.
     */
    public String getType() {
        return m_type;
    }

    /**
     * Returns the value of the DN component.
     * @return rdn The value of the DN component.
     */
    public String getValue() {
        return m_value;
    }

    /**
     * Returns the string representation of the DN component.
     * @return The string representation of the DN component.
     */
    public String toString() {
        return m_type + "=" + m_value;
    }

    /**
     * Determines if the specified string is an distinguished name component.
     * @param dn The string that you want to check.
     * @return true if the string is a distinguished name component.
     */
    public static boolean isRDN(String rdn) {
        RDN newrdn = new RDN(rdn);
        return ((newrdn.getType() != null) && (newrdn.getValue() != null));
    }

    /**
     * Determines if the current DN component is equal to the specified
     * DN component.
     * @param rdn The DN component that you want compared against the
     * current DN component.
     * @return true if the two DN components are equal.
     */
    public boolean equals(RDN rdn) {
        return(toString().toUpperCase().equals(rdn.toString().toUpperCase()));
    }
}
