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

import java.io.*;
import java.util.*;
import org.ietf.ldap.ber.stream.*;

/**
 * Represents the name and values of an attribute in an entry.
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPAttributeSet
 */
public class LDAPAttribute implements Cloneable, Serializable {

    static final long serialVersionUID = -4594745735452202600L; 
    private String name = null;
    private byte[] nameBuf = null;
    /**
     * Internally, this is a list of "byte[]"-based attribute values.
     */
    private Object values[] = new Object[0];

    /**
     * Constructs an attribute from another existing attribute.
     * Effectively, this makes a copy of the existing attribute.
     * @param attr the attribute to copy
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
     * @param attrName name of the attribute
     */
    public LDAPAttribute( String attrName ) {
        name = attrName;
    }

    /**
     * Constructs an attribute with a byte-formatted value.
     * @param attrName name of the attribute
     * @param attrValue value of the attribute in byte format
     */
    public LDAPAttribute( String attrName, byte[] attrValue ) {
        name = attrName;
        addValue(attrValue);
    }

    /**
     * Constructs an attribute that has a single string value.
     * @param attrName name of the attribute
     * @param attrValue value of the attribute in String format
     */
    public LDAPAttribute( String attrName, String attrValue ) {
        name = attrName;
        addValue( attrValue );
    }

    /**
     * Constructs an attribute that has an array of string values.
     * @param attrName name of the attribute
     * @param attrValues the list of string values for this attribute
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
     * @param element element that you want translated into an attribute
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
                if (vals[i] == null) {
                    vals[i] = new byte[0];
                }
            }
            setValues( vals );
        }
     }

    /**
     * Returns the number of values of the attribute.
     * @return number of values for this attribute.
     */
    public int size() {
        return values.length;
    }

    /**
     * Returns an enumerator for the string values of an attribute.
     * @return enumerator for the string values.
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
     * Returns the values of the attribute as an array of <CODE>String</CODE> 
     * objects.
     * @return array of attribute values. Each element in the array
     * is a <CODE>String</CODE> object.
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
     * @return a set of attribute values. Each element in the enumeration
     * is of type <CODE>byte[]</CODE>.
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
     * Returns the values of the attribute in an array of <CODE>byte[]</CODE>
     * format.
     * @return array of attribute values. Each element in the array
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
     * @return name of the attribute.
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
     * @param attrName name of the attribute from which to extract the subtypes
     * @return array of subtypes, or null (if the name has no subtypes).
     * @see org.ietf.ldap.LDAPAttribute#getBaseName
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
     * @return array of subtypes, or null (if the name has no subtypes).
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
     * @return the language subtype, or null (if the name has no
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
     * @param attrName name of the attribute from which to extract the base name
     * @return base name (the attribute name without any subtypes).
     * @see org.ietf.ldap.LDAPAttribute#getSubtypes
     */
    public static String getBaseName( String attrName ) {
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
     * @return base name (the attribute name without any subtypes).
     * @see org.ietf.ldap.LDAPAttribute#getSubtypes
     */
    public String getBaseName() {
        return getBaseName(getName());
    }

    /**
     * Reports whether the attribute name contains the specified subtype.
     * For example, if you check for the subtype <CODE>lang-en</CODE>
     * and the attribute name is <CODE>cn;lang-en</CODE>, this method
     * returns <CODE>true</CODE>.
     * <P>
     *
     * @param subtype the single subtype for which you want to check
     * @return true if the attribute name contains the specified subtype.
     * @see org.ietf.ldap.LDAPAttribute#getSubtypes
     */
    public boolean hasSubtype( String subtype ) {
        String[] mytypes = getSubtypes();
        for(int i = 0; i < mytypes.length; i++) {
            if( subtype.equalsIgnoreCase( mytypes[i] ) )
                return true;
        }
        return false;
    }

    /**
     * Reports if the attribute name contains all specified subtypes
     * For example, if you check for the subtypes <CODE>lang-en</CODE>
     * and <CODE>phonetic</CODE> and the attribute name is
     * <CODE>cn;lang-en;phonetic</CODE>, this method returns <CODE>true</CODE>.
     * If the attribute name is <CODE>cn;phonetic</CODE> or
     * <CODE>cn;lang-en</CODE>, this method returns <CODE>false</CODE>.
     * <P>
     * @param subtypes an array of subtypes to check
     * @return true if the attribute name contains all subtypes
     * @see org.ietf.ldap.LDAPAttribute#getSubtypes
     */
    public boolean hasSubtypes( String[] subtypes ) {
        for(int i = 0; i < subtypes.length; i++) {
            if( !hasSubtype(subtypes[i]) )
                return false;
        }
        return true;
    }

    /**
     * Adds a string value to the attribute.
     * @param attrValue the string value to add to the attribute
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
     * @param attrValues the string values to use in the attribute
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
     * @param attrValue the <CODE>byte[]</CODE>-formatted value to
     * add to the attribute
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
     * @param attrValues the values to use in the attribute
     */
    protected synchronized void setValues( Object[] attrValues ) {
        values = attrValues;
    }

    /**
     * Removes a string value from the attribute.
     * @param attrValue the string value to remove
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
     * @param attrValue <CODE>byte[]</CODE>-formatted value to remove
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
     * Retrieves the BER (Basic Encoding Rules) representation of an attribute.
     * (The protocol elements of LDAP are encoded for exchange using the
     * Basic Encoding Rules.)
     * @return the BER representation of the attribute.
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
     * @return string representation parameters.
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
                    String sval = new String(val, "UTF8");
                    if (sval.length() == 0 && val.length > 0) {
                        sb.append("<binary value, length:");
                        sb.append(val.length);
                        sb.append(">");
                    }
                    else {
                        sb.append(sval);
                    }
                     
                } catch (Exception e) {
                    if (val != null) {
                        sb.append("<binary value, length:");
                        sb.append(val.length);
                        sb.append(">");
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
     * Creates and returns a new <CODE>LDAPAttribute</CODE> object that
     * contains the same information as this one. The cloned object has
     * a deep copy of the contents of the original.
     *
     * @return A deep copy of this object
     */
    public synchronized Object clone() {
        return new LDAPAttribute( this );
    }

    /**
     * Retrieves the string representation of an attribute
     * in an LDAP entry. For example:
     *
     * <PRE>LDAPAttribute {type='cn', values='Barbara Jensen,Babs Jensen'}</PRE>
     *
     * @return string representation of the attribute.
     */
    public String toString() {
        return "LDAPAttribute " + getParamString();
    }
}
