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

/**
 * Represents sorting instructions for a particular attribute.
 *
 * @version 1.0
 */
public class LDAPSortKey implements java.io.Serializable {
    static final long serialVersionUID = -7044232342344864405L;
    public final static int REVERSE = 0x81;

    /**
     * Constructs a new <CODE>LDAPSortKey</CODE> object that will
     * sort based on the specified instructions.
     * @param keyDescription a single attribute specification by which to sort
     * If preceded by a hyphen ("-"), the attribute is sorted in reverse order.
     * You can also specify the object ID (OID) of a matching rule after
     * a colon (":"). For example:
     * <P>
     * <UL>
     * <LI><CODE>"cn"</CODE> (sort by the <CODE>cn</CODE> attribute) <P>
     * <LI><CODE>"-cn"</CODE> (sort by the <CODE>cn</CODE> attribute in
     * reverse order) <P>
     * <LI><CODE>"-cn:1.2.3.4"</CODE> (sort by the <CODE>cn</CODE>
     * attribute in reverse order and use the matching rule identified
     * by the OID 1.2.3.4) <P>
     *</UL>
     * @see org.ietf.ldap.controls.LDAPSortControl
     * @see org.ietf.ldap.controls.LDAPVirtualListControl
     */
    public LDAPSortKey( String keyDescription ) {
        if ( (keyDescription != null) && (keyDescription.length() > 0) ) {
            if ( keyDescription.charAt( 0 ) == '-' ) {
                m_reverse = true;
                m_key = keyDescription.substring( 1 );
            } else {
                m_reverse = false;
                m_key = keyDescription;
            }
            int colonIndex = m_key.indexOf( ':' );
            if ( colonIndex == 0 )
                m_key = null;
            else if ( colonIndex > 0 ) {
                m_matchRule = m_key.substring( colonIndex+1 );
                m_key = m_key.substring( 0, colonIndex );
            }
        }
    }

    /**
     * Constructs a new <CODE>LDAPSortKey</CODE> object that will
     * sort based on the specified attribute and sort order.
     * @param key a single attribute by which to sort.  For example:
     * <P>
     * <UL>
     * <LI><CODE>"cn"</CODE> (sort by the <CODE>cn</CODE> attribute)
     * <LI><CODE>"givenname"</CODE> (sort by the <CODE>givenname</CODE>
     * attribute)
     * </UL>
     * @param reverse if <CODE>true</CODE>, the sorting is done in
     * descending order
     * @see org.ietf.ldap.controls.LDAPSortControl
     * @see org.ietf.ldap.controls.LDAPVirtualListControl
     */
    public LDAPSortKey( String key,
                        boolean reverse) {
        m_key = key;
        m_reverse = reverse;
        m_matchRule = null;
    }

    /**
     * Constructs a new <CODE>LDAPSortKey</CODE> object that will
     * sort based on the specified attribute, sort order, and matching
     * rule.
     * @param key a single attribute by which to sort. For example:
     * <P>
     * <UL>
     * <LI><CODE>"cn"</CODE> (sort by the <CODE>cn</CODE> attribute)
     * <LI><CODE>"givenname"</CODE> (sort by the <CODE>givenname</CODE>
     * attribute)
     * </UL>
     * @param reverse if <CODE>true</CODE>, the sorting is done in
     * descending order
     * @param matchRule object ID (OID) of the matching rule for
     * the attribute (for example, <CODE>1.2.3.4</CODE>)
     * @see org.ietf.ldap.controls.LDAPSortControl
     * @see org.ietf.ldap.controls.LDAPVirtualListControl
     */
    public LDAPSortKey( String key,
                        boolean reverse,
                        String matchRule) {
        m_key = key;
        m_reverse = reverse;
        m_matchRule = matchRule;
    }

    /**
     * Returns the attribute by which to sort.
     * @return a single attribute by which to sort.
     */
    public String getKey() {
        return m_key;
    }

    /**
     * Returns <CODE>true</CODE> if sorting is to be done in descending order.
     * @return <CODE>true</CODE> if sorting is to be done in descending order.
     */
    public boolean getReverse() {
        return m_reverse;
    }

    /**
     * Returns the object ID (OID) of the matching rule used for sorting.
     * If no matching rule is specified, <CODE>null</CODE> is returned.
     * @return the object ID (OID) of the matching rule, or <CODE>null</CODE>
     * if the sorting instructions specify no matching rule.
     */
    public String getMatchRule() {
        return m_matchRule;
    }

    public String toString() {
        
        StringBuffer sb = new StringBuffer("{SortKey:");
        
        sb.append(" key=");
        sb.append(m_key);
        
        sb.append(" reverse=");
        sb.append(m_reverse);

        if (m_matchRule != null) {
            sb.append(" matchRule=");
            sb.append(m_matchRule);
        }
        
        sb.append("}");

        return sb.toString();
    }

    private String m_key;
    private boolean m_reverse;
    private String m_matchRule;
}

