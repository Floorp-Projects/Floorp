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
package netscape.ldap.util;

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
 * @see netscape.ldap.util.DN
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
        int index = rdn.indexOf( "=" );
        int next_plus;

        // if the rdn doesnt have = or = positions right at the beginning of the rdn
        if (index <= 0)
            return;

        Vector values = new Vector();
        Vector types = new Vector();

        types.addElement( rdn.substring( 0, index ).trim() );
        next_plus = findNextMVSeparator( rdn, index ); 
        while ( next_plus != -1 ) {
            m_ismultivalued = true;
            values.addElement( rdn.substring( index + 1, next_plus - 1 ).trim() );
            index = rdn.indexOf( "=", next_plus + 1 );
            if ( index == -1 ) {
                // malformed RDN?
                return;
            }
            types.addElement( rdn.substring( next_plus + 1, index ).trim() );
            next_plus = findNextMVSeparator( rdn, index );
        }    
        values.addElement( rdn.substring( index + 1 ).trim() );
        
        m_type = new String[types.size()];
        m_value = new String[values.size()];

        for( int i = 0; i < types.size(); i++ ) {
            m_type[i] = (String)types.elementAt( i );
            m_value[i] = (String)values.elementAt( i );
        }
    }
    
    /* find the next '+' that isn't escaped or in quotes */
    int findNextMVSeparator( String str, int offset ) {
        int next_plus = str.indexOf( '+', offset );
        int next_open_q = str.indexOf( '"', offset );
        int next_close_q = -1;

        if ( next_plus == offset ) {
            return offset;
        }
        
        if ( next_open_q > -1 ) {
            next_close_q = str.indexOf( '"', next_open_q + 1 );
        }

        while ( next_plus != -1 ) {
            if ( str.charAt( next_plus - 1 ) != '\\' &&
                 !( next_open_q < next_plus && next_plus < next_close_q ) ) {
                return next_plus;
            }

            next_plus = str.indexOf( '+', next_plus + 1 );
            if ( next_open_q > -1 && next_close_q > -1 ) {
                if ( next_close_q < next_plus ) {
                    next_open_q = str.indexOf( '"', next_close_q + 1 );
                }
            } else {
                next_open_q = -1;
            }

            next_close_q = next_open_q > -1 ? str.indexOf( '"', next_open_q + 1 )
                           : -1;
        }
        
        return -1;
                    
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
        return m_type[0];
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
        return m_value[0];
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
        StringBuffer buf = new StringBuffer( m_type[0] + "=" + m_value[0] );
        
        for ( int i = 1; i < m_type.length; i++ ) {
            buf.append( " + " + m_type[i] + "=" + m_value[i] );
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
     * @see netscape.ldap.util.RDN#registerCesAttribute
     * @see netscape.ldap.util.RDN#cesAttributeIsRegistered
     * @see netscape.ldap.util.RDN#getRegisteredCesAttributes
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
     * @see netscape.ldap.util.RDN#equals
     * @see netscape.ldap.util.RDN#unregisterAttributeSyntax
     * @see netscape.ldap.util.RDN#getAttributeSyntax
     * @see netscape.ldap.util.RDN#getAttributesForSyntax
     */
    public static void registerAttributeSyntax( String attr, String oid ) {
        m_attributehash.put( attr.toLowerCase(), oid );
    }
    
    /**
     * Removes the the given attribute from the attribute syntax table. 
     * @param attr the attribute to remove.
     * @see netscape.ldap.util.RDN#registerAttributeSyntax
     * @see netscape.ldap.util.RDN#getAttributeSyntax
     * @see netscape.ldap.util.RDN#getAttributesForSyntax
     */
    public static void unregisterAttributeSyntax( String attr ) {
        m_attributehash.remove( attr.toLowerCase() );
    }

    /**
     * Returns the syntax for the attribute if the given attribute is registered 
     * in the internal attribute table.
     * @param attr the attribute to lookup in the table.
     * @returns the syntax of the attribute if found, null otherwise.
     * @see netscape.ldap.util.RDN#unregisterAttributeSyntax
     * @see netscape.ldap.util.RDN#registerAttributeSyntax
     * @see netscape.ldap.util.RDN#getAttributesForSyntax
     */
    public static String getAttributeSyntax( String attr ) {
        return (String)m_attributehash.get( attr.toLowerCase() );
    }

    /**
     * Returns all attributes registered for the given syntax as a 
     * <code>String</code> Array.
     * @param oid the syntax to look up in the table.
     * @return all attributes for the given syntax as a <code>String[]</code>
     * @see netscape.ldap.util.RDN#unregisterAttributeSyntax
     * @see netscape.ldap.util.RDN#registerAttributeSyntax
     * @see netscape.ldap.util.RDN#getAttributeSyntax
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












