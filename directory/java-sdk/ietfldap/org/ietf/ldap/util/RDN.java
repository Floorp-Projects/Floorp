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
 * @see org.ietf.ldap.util.DN
 */
public final class RDN implements java.io.Serializable {

    static final long serialVersionUID = 7895454691174650321L;

    /**
     * List of RDNs. DN consists of one or more RDNs.
     */
    private String[] m_type = null;
    private String[] m_value = null;
    private boolean m_ismultivalued = false;

    /**
     * Hash table of case sensitive attributes
     */
    private static Hashtable m_attributehash = new Hashtable();    
        
    /**
     * Constructs a new <CODE>RDN</CODE> object from the specified
     * DN component.
     * @param rdn DN component
     */
    public RDN( String rdn ) {
        String neutralRDN = neutralizeEscapes(rdn);
        if (neutralRDN == null) {
            return; // malformed RDN
        }
        int index = neutralRDN.indexOf( "=" );
        int next_plus;

        // if the rdn doesnt have = or = positions right at the beginning of the rdn
        if (index <= 0)
            return;

        Vector values = new Vector();
        Vector types = new Vector();

        types.addElement( rdn.substring( 0, index ).trim() );
        next_plus = neutralRDN.indexOf( '+', index ); 
        while ( next_plus != -1 ) {
            m_ismultivalued = true;
            values.addElement( rdn.substring( index + 1, next_plus).trim() );
            index = neutralRDN.indexOf( "=", next_plus + 1 );
            if ( index == -1 ) {
                // malformed RDN?
                return;
            }
            types.addElement( rdn.substring( next_plus + 1, index ).trim() );
            next_plus = neutralRDN.indexOf('+', index );
        }    
        values.addElement( rdn.substring( index + 1 ).trim() );
        
        m_type = new String[types.size()];
        m_value = new String[values.size()];

        for( int i = 0; i < types.size(); i++ ) {
            m_type[i] = (String)types.elementAt( i );
            if (!isValidType(m_type[i])) {
                m_type = m_value = null;
                return; // malformed
            }
            m_value[i] = (String)values.elementAt( i );
            if (!isValidValue(m_value[i])) {
                m_type = m_value = null;
                return; // malformed
            }
        }
    }
    
    /**
     * Neutralize backslash escapes and quoted sequences for easy parsing.
     * @return rdn string with disabled escapes or null if malformed rdn
     */
     static String neutralizeEscapes(String rdn) {
        if (rdn == null) {
            return null;
        }        
        StringBuffer sb = new StringBuffer(rdn);
        boolean quoteOn = false;
        // first pass, disable backslash escapes
        for (int i=0; i < sb.length(); i++) {
            if (sb.charAt(i) =='\\') {
                sb.setCharAt(i, 'x');
                if (i < sb.length()-1) {
                    sb.setCharAt(i+1, 'x');
                }
                else {
                    return null;
                }
            }
        }
        // second pass, disable quoted sequences
        for (int i=0; i < sb.length(); i++) {
            if (sb.charAt(i) == '"') {
                quoteOn = !quoteOn;
                continue;
            }
            if (quoteOn) {
                sb.setCharAt(i, 'x');
            }
        }        
        return quoteOn ? null : sb.toString();
    }

    /**
     * Type names can not contain any DN special characters
     */
    private boolean isValidType(String type) {
        if (type == null || type.length() < 1) {
            return false;
        }
        for (int i=0; i< type.length(); i++) {
            for (int j=0; j < DN.ESCAPED_CHAR.length; j++) {
                if (type.charAt(i) == DN.ESCAPED_CHAR[j]) {
                    return false;
                }
            }
        }
        return true;
    }
    
    /**
     * Values can contain only single quote sequence, where quotes are
     * at the beginning and the end of the sequence
     */
    private boolean isValidValue(String val) {
        if (val == null || val.length() < 1) {
            return false;
        }
        // count unescaped '"'
        int cnt=0, i=0;
        while (i >=0 && i < val.length()) {
            i = val.indexOf('"', i);
            if (i >= 0) {
                if (i==0 || (val.charAt(i-1) != '\\')) {
                    cnt++;
                }
                i++;
            }
        }
        if (cnt == 0) {
            return true;
        }
        else if (cnt != 2) { // can have only two of them surrounding the value
            return false;
        }
        else if (val.charAt(0) != '"' || val.charAt(val.length()-1) != '"') {
            return false;
        }
    
        return true;
    }
    
    /**
     * Returns the DN component as the first element in an
     * array of strings.
     * @param noType specify <code>true</code> to ignore the attribute type and
     * equals sign (for example, "cn=") and return only the value
     * @return an array of strings representing the DN component.
     * @deprecated use <code>toString</code> or <code>getValues</code> instead.
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
     * @return rdn the attribute type of the DN component.
     * @deprecated use <code>getTypes()</code> instead.
     */
    public String getType() {
        if (m_type != null && m_type.length > 0) {
            return m_type[0];
        }
        return null;
    }

    /**
     * Returns the attribute types of the DN component.
     * @return rdn the attribute types of the DN component.
     */
    public String[] getTypes() {
        return m_type;
    }

    /**
     * Returns the value of the DN component.
     * @return rdn the value of the DN component.
     * @deprecated use <code>getValues()</code> instead.
     */
    public String getValue() {
        if (m_value != null && m_value.length > 0) {
            return m_value[0];
        }
        return null;
    }

    /**
     * Returns the values of the DN component.
     * @return rdn the values of the DN component.
     */
    public String[] getValues() {
        return m_value;
    }

    /** 
     * Returns <code>true</code> if the RDN is multi-valued.
     * @return <code>true</code> if the RDN is multi-valued.
     */
    public boolean isMultivalued() {
        return m_ismultivalued;
    }
    
    /**
     * Returns the string representation of the DN component.
     * @return the string representation of the DN component.
     */
    public String toString() {
        StringBuffer buf = new StringBuffer();
                
        for ( int i = 0; m_type != null  && i < m_type.length; i++ ) {
            if ( i != 0) {
                buf.append(" + ");
            }
            buf.append( m_type[i] + "=" + m_value[i]);
        }

        return buf.toString();
    }

    /**
     * Determines if the specified string is a distinguished name component.
     * @param dn the string to check
     * @return <code>true</code> if the string is a distinguished name component.
     */
    public static boolean isRDN(String rdn) {
        RDN newrdn = new RDN(rdn);
        return ((newrdn.getTypes() != null) && (newrdn.getValues() != null));
    }

    /**
     * Determines if the current DN component is equal to the specified
     * DN component. Uses an internal table of ces (case exact string) 
     * attributes to determine how the attributes should be compared.
     * @param rdn the DN component to compare against the
     * current DN component.
     * @return <code>true</code> if the two DN components are equal.
     * @see org.ietf.ldap.util.RDN#registerAttributeSyntax
     * @see org.ietf.ldap.util.RDN#getAttributeSyntax
     */
    public boolean equals(RDN rdn) {
        String[] this_types = (String[])getTypes().clone(); 
        String[] this_values = (String[])getValues().clone();
        String[] rdn_types = (String[])rdn.getTypes().clone();
        String[] rdn_values = (String[])rdn.getValues().clone();

        if ( this_types.length != rdn_types.length ) {
            return false;
        }

        sortTypesAndValues( this_types, this_values );
        sortTypesAndValues( rdn_types, rdn_values );
        
        for (int i = 0; i < this_types.length; i++ ) { 
            
            if ( !this_types[i].equalsIgnoreCase( rdn_types[i] ) ) {
                return false;
            }

            if ( CES_SYNTAX.equals( getAttributeSyntax( this_types[i] ) ) ) {
                if ( !this_values[i].equals( rdn_values[i] ) ) {
                    return false;
                }
            } else { 
                if ( !this_values[i].equalsIgnoreCase( rdn_values[i] ) ) {
                    return false;
                }
            }
        }

        return true;
    }


    /* sorts the rdn components by attribute to make comparison easier */
    void sortTypesAndValues( String[] types, String[] values ) {
        boolean no_changes;
        do {
            no_changes = true;
            for ( int i = 0; i < types.length - 1; i++ ) {
                if ( types[i].toLowerCase().compareTo( types[i + 1].toLowerCase() ) > 0 ) {
                    String tmp_type = types[i];
                    String tmp_value = values[i];
                    types[i] = types[i + 1];
                    values[i] = values[i + 1];
                    types[i + 1] = tmp_type;
                    values[i + 1] = tmp_value;
                    no_changes = false;
                }
            }
        } while ( no_changes = false );   
    }

    /**
     * Registers the the given attribute for the given syntax in an
     * internal table. This table is used for attribute comparison in the 
     * <code>equals()</code> method.
     * @param attr the attribute to register.
     * @param oid the syntax to register with the attribute.
     * @see org.ietf.ldap.util.RDN#equals
     * @see org.ietf.ldap.util.RDN#unregisterAttributeSyntax
     * @see org.ietf.ldap.util.RDN#getAttributeSyntax
     * @see org.ietf.ldap.util.RDN#getAttributesForSyntax
     */
    public static void registerAttributeSyntax( String attr, String oid ) {
        m_attributehash.put( attr.toLowerCase(), oid );
    }
    
    /**
     * Removes the the given attribute from the attribute syntax table. 
     * @param attr the attribute to remove.
     * @see org.ietf.ldap.util.RDN#registerAttributeSyntax
     * @see org.ietf.ldap.util.RDN#getAttributeSyntax
     * @see org.ietf.ldap.util.RDN#getAttributesForSyntax
     */
    public static void unregisterAttributeSyntax( String attr ) {
        m_attributehash.remove( attr.toLowerCase() );
    }

    /**
     * Returns the syntax for the attribute if the given attribute is registered 
     * in the internal attribute table.
     * @param attr the attribute to lookup in the table.
     * @return the syntax of the attribute if found, null otherwise.
     * @see org.ietf.ldap.util.RDN#unregisterAttributeSyntax
     * @see org.ietf.ldap.util.RDN#registerAttributeSyntax
     * @see org.ietf.ldap.util.RDN#getAttributesForSyntax
     */
    public static String getAttributeSyntax( String attr ) {
        return (String)m_attributehash.get( attr.toLowerCase() );
    }

    /**
     * Returns all attributes registered for the given syntax as a 
     * <code>String</code> Array.
     * @param oid the syntax to look up in the table.
     * @return all attributes for the given syntax as a <code>String[]</code>
     * @see org.ietf.ldap.util.RDN#unregisterAttributeSyntax
     * @see org.ietf.ldap.util.RDN#registerAttributeSyntax
     * @see org.ietf.ldap.util.RDN#getAttributeSyntax
     */
    public static String[] getAttributesForSyntax( String oid ) {
        Enumeration enum = m_attributehash.keys();
        Vector key_v = new Vector();
        String tmp_str = null;

        while ( enum.hasMoreElements() ) {
            tmp_str = (String)enum.nextElement();
            if ( oid.equals( (String)m_attributehash.get( tmp_str ) ) ) {
                key_v.addElement( tmp_str );
            }
        }

        String[]  str = new String[key_v.size()];
        for ( int i = 0; i < str.length; i++ ) {
            str[i] = new String( (String)key_v.elementAt( i ) );
        }
        
        return str;
    }

    public static final String[] _cesAttributes = {
        "adminurl",
        "altserver",
        "automountinformation",
        "bootfile",
        "bootparameter",
        "cirbindcredentials",
        "generation",
        "homedirectory",
        "internationalisdnnumber",
        "labeleduri",
        "membercertificatedescription",
        "membernisnetgroup",
        "memberuid",
        "memberurl",
        "nismapentry",
        "nisnetgrouptriple",
        "nsaddressbooksyncurl",
        "presentationaddress",
        "ref",
        "replicaentryfilter",
        "searchguide",
        "subtreeaci",
        "vlvfilter",
        "vlvname",
        "x121address"
    };

    public static final String CES_SYNTAX = "1.3.6.1.4.1.1466.115.121.1.26";

    static {
        /* static initializer to fill the ces attribute hash 
         * this list was generated from the slapd.at.conf that
         * ships with Netscape Directory Server 4.1 
         */
        for ( int i = 0; i < _cesAttributes.length; i++ ) {
            registerAttributeSyntax( _cesAttributes[i], CES_SYNTAX );
        }
    }
}












