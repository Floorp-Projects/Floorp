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
package netscape.ldap;

import java.io.*;
import java.util.*;
import netscape.ldap.ber.stream.*;

/**
 * Represents the name and values of an attribute in an entry.
 *
 * @version 1.0
 * @see netscape.ldap.LDAPAttributeSet
 */
public class LDAPAttribute {

    private String name = null;
    private byte[] nameBuf = null;
    /**
     * Internally, it is a list of "byte[]"-based attribute values.
     */
    private Object values[] = new Object[0];

    /**
     * Constructs an attribute from another existing attribute.
     * Effectively, this makes a copy of the existing attribute.
     * @param attr The attribute to copy
     */
    public LDAPAttribute( LDAPAttribute attr ) {
        name = attr.name;
        nameBuf = attr.nameBuf;
        values = new Object[attr.values.length];
        for (int i = 0; i < attr.values.length; i++) {
            values[i] = new byte[((byte[])attr.values[i]).length];
            System.arraycopy((byte[])attr.values[i], 0, (byte[])values[i], 0,
                ((byte[])attr.values[i]).length);
        }
    }

    /**
     * Constructs an attribute with no values.
     * @param attrName Name of the attribute.
     */
    public LDAPAttribute( String attrName ) {
        name = attrName;
    }

    /**
     * Constructs an attribute with a byte-formatted value.
     * @param attrName Name of the attribute.
     * @param attrValue Value of the attribute in byte format.
     */
    public LDAPAttribute( String attrName, byte[] attrValue ) {
        name = attrName;
        addValue(attrValue);
    }

    /**
     * Constructs an attribute that has a single string value.
     * @param attrName Name of the attribute.
     * @param attrValue Value of the attribute in String format.
     */
    public LDAPAttribute( String attrName, String attrValue ) {
        name = attrName;
        addValue( attrValue );
    }

    /**
     * Constructs an attribute that has an array of string values.
     * @param attrName Name of the attribute.
     * @param attrValues The list of string values for this attribute.
     */
    public LDAPAttribute( String attrName, String[] attrValues ) {
        name = attrName;
        if (attrValues != null) {
            setValues( attrValues );
        }
    }

    /**
     * Constructs an attribute from a BER (Basic Encoding Rules) element.
     * (The protocol elements of LDAP are encoded for exchange using the
     * Basic Encoding Rules.)
     * @param element Element that you want translated into an attribute.
     * @exception IOException The attribute could not be created from
     * the specified element.
     */
    public LDAPAttribute(BERElement element) throws IOException {
        BERSequence seq = (BERSequence)element;
        BEROctetString type = (BEROctetString)seq.elementAt(0);
        nameBuf = type.getValue();
        BERSet set = (BERSet)seq.elementAt(1);
        if (set.size() > 0) {
            Object[] vals = new Object[set.size()];
            for (int i = 0; i < set.size(); i++) {
                vals[i] = ((BEROctetString)set.elementAt(i)).getValue();
            }
            setValues( vals );
        }
     }

    /**
     * Returns the number of values of the attribute.
     * @return Number of values for this attribute.
     */
    public int size() {
        return values.length;
    }

    /**
     * Returns an enumerator for the string values of an attribute.
     * @return Enumerator for the string values.
     */
    public Enumeration getStringValues() {
        Vector v = new Vector();
        synchronized(this) {
            try {
                for (int i=0; i<values.length; i++) {
                    if ( values[i] != null ) {
                        v.addElement(new String ((byte[])values[i], "UTF8"));
                    } else {
                        v.addElement( new String( "" ) );
                    }
                }
            } catch ( Exception e ) {
                return null;
            }
        }
        return v.elements();
    }
	
	/**
     * Returns a array for the values of the attribute as <CODE>String</CODE> objects.
     * @return Array of attribute values. Each element in the array
     * will be a <CODE>String</CODE> object.
     */
    public String[] getStringValueArray() {
    
    	String s[] = new String[values.length];
    	synchronized(this) {
    		try {
    			for (int i=0; i < values.length; i++) {
    				if ( values[i] !=null ) {
    					s[i] = new String((byte[])values[i], "UTF8");
    				} else {
    					s[i] = new String("");
    				}
    			}
    		} catch (Exception e) {
    			return null;
    		}
    	}
    	return s;
    }
    
    /**
     * Returns an enumerator for the values of the attribute in <CODE>byte[]</CODE>
     * format.
     * @return A set of attribute values. Each element in the enumeration
     * will be of type <CODE>byte[]</CODE>.
     */
    public Enumeration getByteValues() {
        Vector v = new Vector();
        synchronized(this) {
            for (int i=0; i<values.length; i++) {
                if ( values[i] != null ) {
                    v.addElement(values[i]);
                } else {
                    v.addElement( new byte[0] );
                }
            }
        }
        return v.elements();
    }

	/**
     * Returns an array for the values of the attribute in <CODE>byte[]</CODE>
     * format.
     * @return Array of attribute values. Each element in the array
     * will be of type <CODE>byte[]</CODE>.
     */
     public byte[][] getByteValueArray() {
    	byte b[][] = new byte[values.length][];
    	synchronized(this) {
    		try {
    			for (int i=0; i < values.length; i++) {
    				b[i] = new byte[((byte[])(values[i])).length];
	    			System.arraycopy((byte[])values[i], 0, (byte[])b[i], 0, 
    					((byte[])(values[i])).length);
    			}
    		} catch (Exception e) {
    			return null;
    		}
    	}
    	return b;
     
     }
     
    /**
     * Returns the name of the attribute.
     * @return Name of the attribute.
     */
    public String getName() {
        if ((name == null) && (nameBuf != null)) {
            try{
                name = new String(nameBuf, "UTF8");
            } catch(Throwable x) {}
        }
        return name;
    }

    /**
     * Extracts the subtypes from the specified attribute name.
     * For example, if the attribute name is <CODE>cn;lang-ja;phonetic</CODE>,
     * this method returns an array containing <CODE>lang-ja</CODE>
     * and <CODE>phonetic</CODE>.
     * <P>
     *
     * @param attrName Name of the attribute to extract the subtypes from
     * @return Array of subtypes, or null (if the name has no subtypes)
     * @see netscape.ldap.LDAPAttribute#getBaseName
     */
    public static String[] getSubtypes(String attrName) {
        StringTokenizer st = new StringTokenizer(attrName, ";");
        if( st.hasMoreElements() ) {
            // First element is base name
            st.nextElement();
            String[] subtypes = new String[st.countTokens()];
            int i = 0;
            // Extract the types
            while( st.hasMoreElements() )
                subtypes[i++] = (String)st.nextElement();
            return subtypes;
        }
        return null;
    }

    /**
     * Extracts the subtypes from the attribute name of the current
     * <CODE>LDAPAttribute</CODE> object.  For example, if the attribute
     * name is <CODE>cn;lang-ja;phonetic</CODE>, this method returns an array
     * containing <CODE>lang-ja</CODE> and <CODE>phonetic</CODE>.
     *<P>
     *
     * @return Array of subtypes, or null (if the name has no subtypes)
     */
    public String[] getSubtypes() {
        return getSubtypes(getName());
    }

    /**
     * Extracts the language subtype from the attribute name of the
     * <CODE>LDAPAttribute</CODE> object, if any.  For example, if the
     * attribute name is <CODE>cn;lang-ja;phonetic</CODE>, this method
     * returns the String <CODE>lang-ja</CODE>.
     *<P>
     *
     * @return The language subtype, or null (if the name has no
     * language subtype).
     */
    public String getLangSubtype() {
        String[] subTypes = getSubtypes();
        if ( subTypes != null ) {
            for( int i = 0; i < subTypes.length; i++ ) {
                if ((subTypes[i].length() >= 5) &&
                    (subTypes[i].substring(0, 5).equalsIgnoreCase("lang-")))
                    return subTypes[i];
            }
        }
        return null;
    }

    /**
     * Extracts the base name from the specified attribute name.
     * For example, if the attribute name is <CODE>cn;lang-ja;phonetic</CODE>,
     * this method returns <CODE>cn</CODE>.
     * <P>
     *
     * @param attrName Name of the attribute from which to extract the base name
     * @return Base name (the attribute name without any subtypes)
     * @see netscape.ldap.LDAPAttribute#getSubtypes
     */
    public static String getBaseName(String attrName) {
        String basename = attrName;
        StringTokenizer st = new StringTokenizer(attrName, ";");
        if( st.hasMoreElements() )
            // First element is base name
            basename = (String)st.nextElement();
        return basename;
    }

    /**
     * Extracts the base name from the attribute name of the current
     * <CODE>LDAPAttribute</CODE> object. For example, if the attribute
     * name is <CODE>cn;lang-ja;phonetic</CODE>, this method returns
     * <CODE>cn</CODE>.
     * <P>
     *
     * @return Base name (the attribute name without any subtypes)
     * @see netscape.ldap.LDAPAttribute#getSubtypes
     */
    public String getBaseName() {
        return getBaseName(getName());
    }

    /**
     * Report if the attribute name contains the specified subtype.
     * For example, if you check for the subtype <CODE>lang-en</CODE>
     * and the attribute name is <CODE>cn;lang-en</CODE>, this method
     * returns <CODE>true</CODE>.
     * <P>
     *
     * @param subtype The single subtype that you want to check for
     * @return true if the attribute name contains the specified subtype
     * @see netscape.ldap.LDAPAttribute#getSubtypes
     */
    public boolean hasSubtype(String subtype) {
        String[] mytypes = getSubtypes();
        for(int i = 0; i < mytypes.length; i++) {
            if( subtype.equalsIgnoreCase( mytypes[i] ) )
                return true;
        }
        return false;
    }

    /**
     * Report if the attribute name contains all specified subtypes
     * For example, if you check for the subtypes <CODE>lang-en</CODE>
     * and <CODE>phonetic</CODE> and if the attribute name is
     * <CODE>cn;lang-en;phonetic</CODE>, this method returns <CODE>true</CODE>.
     * If the attribute name is <CODE>cn;phonetic</CODE> or
     * <CODE>cn;lang-en</CODE>, this method returns <CODE>false</CODE>.
     * <P>
     * @param subtypes An array of subtypes to check
     * @return true if the attribute name contains all subtypes
     * @see netscape.ldap.LDAPAttribute#getSubtypes
     */
    public boolean hasSubtypes(String[] subtypes) {
        for(int i = 0; i < subtypes.length; i++) {
            if( !hasSubtype(subtypes[i]) )
                return false;
        }
        return true;
    }

    /**
     * Adds a string value to the attribute.
     * @param attrValue The string value that you want to add to the attribute.
     */
    public synchronized void addValue( String attrValue ) {
        if (attrValue != null) {
            try {
                byte[] b = attrValue.getBytes("UTF8");
                addValue( b );
            } catch(Throwable x)
            {}
        }
    }

    /**
     * Sets the string values as the attribute's values.
     * @param attrValues The string values that you want to use in the attribute.
     */
    protected void setValues( String[] attrValues ) {
        Object[] vals;
        if (attrValues != null) {
            vals = new Object[attrValues.length];
            for (int i = 0; i < vals.length; i++) {
                try {
                    vals[i] = attrValues[i].getBytes("UTF8");
                } catch(Throwable x)
                    { vals[i] = new byte[0]; }
            }
        } else {
            vals = new Object[0];
        }
        setValues(vals);
    }

    /**
     * Adds a <CODE>byte[]</CODE>-formatted value to the attribute.
     * @param attrValue The <CODE>byte[]</CODE>-formatted value that you
     * want to add to the attribute.
     */
    public synchronized void addValue( byte[] attrValue ) {
        if (attrValue != null) {
            Object[] vals = new Object[values.length+1];
            for (int i = 0; i < values.length; i++)
                vals[i] = values[i];
            vals[values.length] = attrValue;
            values = vals;
        }
    }

    /**
     * Sets the byte[] values as the attribute's values.
     * @param attrValues The values that you want to use in the attribute.
     */
    protected synchronized void setValues( Object[] attrValues ) {
        values = attrValues;
    }

    /**
     * Removes a string value from the attribute.
     * @param attrValue The string value that you want removed.
     */
    public synchronized void removeValue( String attrValue) {
        if (attrValue != null) {
            try{
                byte b[] = attrValue.getBytes("UTF8");
                removeValue ( b );
            } catch(Throwable x)
            {}
        }
    }

    /**
     * Removes a <CODE>byte[]</CODE>-formatted value from the attribute.
     * @param attrValue <CODE>byte[]</CODE>-formatted value that you want removed.
     */
    public synchronized void removeValue( byte[] attrValue) {
        if ((attrValue == null) || (values == null)|| (values.length < 1))
            return;
        int ind = -1;
        for (int i=0; i<values.length; i++) {
            if (equalValue(attrValue, (byte[])values[i])) {
                ind = i;
                 break;
            }
        }
        if (ind >= 0) {
            Object[] vals = new Object[values.length-1];
            int j = 0;
            for (int i = 0; i < values.length; i++) {
                if (i != ind) {
                    vals[j++] = values[i];
                }
            }
            values = vals;
        }
    }

    private static boolean equalValue(byte[] a, byte[] b) {
        if (a.length != b.length)
            return false;

        for (int i=0; i<a.length; i++) {
            if (a[i] != b[i])
                return false;
        }
        return true;
    }

    /**
     * Retrieves the BER (Basic Encoding Rules) representation of attribute.
     * (The protocol elements of LDAP are encoded for exchange using the
     * Basic Encoding Rules.)
     * @return The BER representation of the attribute.
     */
    public BERElement getBERElement() {
        try {
            BERSequence seq = new BERSequence();
            seq.addElement(new BEROctetString(getName()));
            BERSet set = new BERSet();
            for (int i = 0; i < values.length; i++) {
                set.addElement(new BEROctetString((byte[])values[i]));
            }
            seq.addElement(set);
            return seq;
        } catch (IOException e) {
            return null;
        }
    }

    /**
     * Retrieves the string representation of attribute parameters.
     * @return string representation parameters
     */
    private String getParamString() {
        StringBuffer sb = new StringBuffer();
        
        if ( values.length > 0 ) {
            for (int i = 0; i < values.length; i++) {
                if (i != 0) {
                    sb.append(",");
                }
                byte[] val = (byte[])values[i];
                try {
                    sb.append(new String(val, "UTF8"));
                } catch (Exception e) {
                    if (val != null) {
                        sb.append(val.length);
                        sb.append(" bytes");
                    }
                    else {
                        sb.append("null value");
                    }
                }
            }
        }
        return "{type='" + getName() + "', values='" + sb.toString() + "'}";
    }

    /**
     * Retrieves the string representation of attribute
     * in an LDAP entry. For example:
     *
     * <PRE>LDAPAttribute {type='cn', values='Barbara Jensen,Babs Jensen'}</PRE>
     *
     * @return String representation of the attribute.
     */
    public String toString() {
        return "LDAPAttribute " + getParamString();
    }
}
