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

import java.io.Serializable;
import java.util.*;

import org.ietf.ldap.client.*;
import org.ietf.ldap.client.opers.*;

/**
 * Represents a set of attributes (for example, the set of attributes
 * in an entry).
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPAttribute
 */
public class LDAPAttributeSet implements Cloneable, Serializable, Set {
    static final long serialVersionUID = 5018474561697778100L;
    HashMap _attrHash = null;
    LDAPAttribute[] _attrs = new LDAPAttribute[0];
    /* If there are less attributes than this in the set, it's not worth
     creating a Hashtable - faster and cheaper most likely to do string
     comparisons. Most applications fetch attributes once only, anyway */
    static final int ATTR_COUNT_REQUIRES_HASH = 5;

    /**
     * Constructs a new set of attributes.  This set is initially empty.
     */
    public LDAPAttributeSet() {
        // For now, always create the hashtable
        prepareHashtable( true );
    }

    /**
     * Constructs an attribute set.
     * @param attrs the list of attributes
     */
    public LDAPAttributeSet( LDAPAttribute[] attrs ) {
        this();
        if ( attrs == null ) {
            attrs = new LDAPAttribute[0];
        }
        _attrs = attrs;
        if ( _attrs.length > 0 ) {
            for( int i = 0; i < _attrs.length; i++ ) {
                add( _attrs[i] );
            }
        }
    }

    /**
     * Removes all mappings from this attribute set
     */
    public void clear() {
        _attrHash = null;
        if ( _attrs.length > 0 ) {
            _attrs = new LDAPAttribute[0];
        }
    }

    /**
     * Adds the specified attribute to this attribute set, overriding
     * any previous definition with the same attribute name
     *
     * @param attr attribute to add to this set
     * @return true if this set changed as a result of the call
     */
    public synchronized boolean add( Object attr ) {
        if ( attr instanceof LDAPAttribute ) {
            if ( contains( attr ) ) {
                return false;
            }
            LDAPAttribute attrib = (LDAPAttribute)attr;
            LDAPAttribute[] vals = new LDAPAttribute[_attrs.length+1];
            for ( int i = 0; i < _attrs.length; i++ ) {
                vals[i] = _attrs[i];
            }
            vals[_attrs.length] = attrib;
            _attrs = vals;
            if ( _attrHash != null ) {
                _attrHash.put( attrib.getName().toLowerCase(), attr );
            }
            return true;
        } else {
            throw new ClassCastException( "Requires LDAPAttribute");
        }
    }

    /**
     * Adds the collection of attributes to this attribute set, overriding
     * any previous definition with the same attribute names
     *
     * @param attrs attributes to add to this set
     * @return true if any attribute was added
     */
    public synchronized boolean addAll( Collection attrs ) {
        if ( attrs == null ) {
            return false;
        }
        boolean present = true;
        Iterator it = attrs.iterator();
        while( it.hasNext() ) {
            Object attr = it.next();
            if ( !contains( attr ) ) {
                present = true;
                add( attr );
            }
        }
        return !present;
    }

    /**
     * Returns a deep copy of this attribute set
     *
     * @return a deep copy of this attribute set
     */
    public synchronized Object clone() {
        try {
            LDAPAttributeSet attributeSet = new LDAPAttributeSet();
            attributeSet._attrs = new LDAPAttribute[_attrs.length];
            for (int i = 0; i < _attrs.length; i++) {
                attributeSet._attrs[i] = new LDAPAttribute(_attrs[i]);
            }
             return attributeSet;
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Returns true if this attribute set contains the specified attribute
     *
     * @param attr attribute whose presence in this set is to be tested
     * @return true if the attribute set contains the specified attribute
     */
    public boolean contains( Object attr ) {
        if ( !(attr instanceof LDAPAttribute) ) {
            return false;
        } else {
            if ( _attrHash != null ) {
                return _attrHash.containsValue( attr );
            } else {
                for ( int i = 0; i < _attrs.length; i++ ) {
                    if ( attr.equals(_attrs[i]) ) {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    /**
     * Returns true if this attribute set contains all the specified attributes
     *
     * @param attrs attributes whose presence in this set is to be tested
     * @return true if the attribute set contains the specified attributes
     */
    public boolean containsAll( Collection attrs ) {
        if ( _attrHash != null ) {
            Iterator it = attrs.iterator();
            while( it.hasNext() ) {
                if ( !_attrHash.containsValue( it.next() ) ) {
                    return false;
                }
            }
            return true;
        } else {
            return false;
        }
    }

    /**
     * Returns true if this attribute set contains the specified attribute name
     *
     * @param attrName attribute name whose presence in this set is to be tested
     * @return true if the attribute set contains the specified attribute
     */
    public boolean containsKey( Object attrName ) {
        if ( !(attrName instanceof String) ) {
            return false;
        } else {
            return ( getAttribute( (String)attrName ) != null );
        }
    }

    /**
     * Returns true if this attribute set equals a specified set
     *
     * @param attrSet attribute set to compare to
     * @return true if this attribute set equals a specified set
     */
    public boolean equals( Object attrSet ) {
        if ( !(attrSet instanceof LDAPAttributeSet) ) {
            return false;
        }
        return ((LDAPAttributeSet)attrSet)._attrHash.equals( _attrHash );
    }

    /**
     * Returns the hash code for this attribute set
     *
     * @return the hash code for this attribute set
     */
    public int hashCode() {
        return _attrHash.hashCode();
    }

    /**
     * Returns true if there are no attributes in this attribute set
     *
     * @return true if there are no attributes in this attribute set
     */
    public boolean isEmpty() {
        return ( _attrs.length < 1 );
    }

    /**
     * Returns an iterator over the attributes in this attribute set
     *
     * @return an iterator over the attributes in this attribute set
     */
    public Iterator iterator() {
        return _attrHash.values().iterator();
    }

    /**
     * Removes the specified attribute
     *
     * @param attr the attribute to remove
     * @return true if the attribute was removed
     */
    public boolean remove( Object attr ) {
        if ( !(attr instanceof LDAPAttribute) ) {
            return false;
        }
        boolean present = contains( attr );
        if ( present ) {
            synchronized(this) {
                LDAPAttribute[] vals = new LDAPAttribute[_attrs.length-1];
                int j = 0;
                for (int i = 0; i < _attrs.length; i++) {
                    if ( !attr.equals(_attrs[i] ) ) {
                        vals[j++] = _attrs[i];
                    }
                }
                if (_attrHash != null) {
                    _attrHash.remove(
                        ((LDAPAttribute)attr).getName().toLowerCase() );
                }
                _attrs = vals;
            }
        }
        return present;
    }

    /**
     * Removes the specified attributes
     *
     * @param attrs the attributes to remove
     * @return true if any attribute was removed
     */
    public boolean removeAll( Collection attrs ) {
        if ( attrs == null ) {
            return false;
        }
        boolean present = true;
        Iterator it = attrs.iterator();
        while( it.hasNext() ) {
            Object attr = it.next();
            if ( !contains( attr ) ) {
                present = true;
                remove( attr );
            }
        }
        return !present;
    }

    /**
     * Retains only the attributes in this set that are contained in the
     * specified collection
     *
     * @param attrs attributes to retain
     * @return true if the attribute set was changed as a result of the
     * operation
     */
    public boolean retainAll( Collection attrs ) {
        HashMap newmap = new HashMap();
        Iterator it = attrs.iterator();
        while( it.hasNext() ) {
            Object attr = it.next();
            if ( attr instanceof LDAPAttribute ) {
                newmap.put( ((LDAPAttribute)attr).getName().toLowerCase(),
                            attr );
            }
        }
        if ( newmap.equals( _attrHash ) ) {
            return false;
        } else {
            _attrHash = newmap;
            _attrs = (LDAPAttribute[])_attrHash.values().toArray(
                new LDAPAttribute[0] );
            return true;
        }
    }

    /**
     * Returns the number of attributes in this set.
     * @return number of attributes in this attribute set.
     */
    public int size() {
        return _attrs.length;
    }

    /**
     * Returns the attributes of the set as an array
     *
     * @return the attributes of the set as an array
     */
    public Object[] toArray() {
        return _attrs;
    }

    /**
     * Returns the attributes of the set as an array
     *
     * @param attrs an attribute array to fill with the attributes of this
     * set. If the array is not large enough, a new array is allocated.
     * @return the attributes of the set as an array
     */
    public Object[] toArray( Object[] attrs ) {
        if ( !(attrs instanceof LDAPAttribute[]) ) {
            throw new ArrayStoreException(
                "Must provide an LDAPAttribute array" );
        } else if ( attrs.length >= _attrs.length ) {
            for( int i = 0; i < _attrs.length; i++ ) {
                attrs[i] = _attrs[i];
            }
            return attrs;
        } else {
            return _attrs;
        }
    }

    /**
     * Creates a new attribute set containing only the attributes
     * that have the specified subtypes.
     * <P>
     *
     * For example, suppose an attribute set contains the following attributes:
     * <P>
     *
     * <PRE>
     * cn
     * cn;lang-ja
     * sn;phonetic;lang-ja
     * sn;lang-us
     * </PRE>
     *
     * If you call the <CODE>getSubset</CODE> method and pass
     * <CODE>lang-ja</CODE> as the argument, the method returns
     * an attribute set containing the following attributes:
     * <P>
     *
     * <PRE>
     * cn;lang-ja
     * sn;phonetic;lang-ja
     * </PRE>
     *
     * @param subtype semi-colon delimited list of subtypes
     * to find within attribute names. 
     * For example:
     * <PRE>
     *     "lang-ja"        // Only Japanese language subtypes
     *     "binary"         // Only binary subtypes
     *     "binary;lang-ja" // Only Japanese language subtypes
     *                         which also are binary
     * </PRE>
     * @return attribute set containing the attributes that have
     * the specified subtypes.
     * @see org.ietf.ldap.LDAPAttribute
     * @see org.ietf.ldap.LDAPAttributeSet#getAttribute
     * @see org.ietf.ldap.LDAPEntry#getAttributeSet
     */
    public LDAPAttributeSet getSubset( String subtype ) {
        LDAPAttributeSet attrs = new LDAPAttributeSet();
        if ( subtype == null )
            return attrs;
        StringTokenizer st = new StringTokenizer(subtype, ";");
        if( st.countTokens() < 1 )
            return attrs;
        String[] searchTypes = new String[st.countTokens()];
        int i = 0;
        while( st.hasMoreTokens() ) {
            searchTypes[i] = (String)st.nextToken();
            i++;
        }
        Iterator it = _attrHash.values().iterator();
        while( it.hasNext() ) {
            LDAPAttribute attr = (LDAPAttribute)it.next();
            if( attr.hasSubtypes( searchTypes ) )
                attrs.add( new LDAPAttribute( attr ) );
        }
        return attrs;
    }

    /**
     * Returns a single attribute that exactly matches the specified attribute
     * name.
     * @param attrName name of attribute to return
     * For example:
     *<PRE>
     *     "cn"            // Only a non-subtyped version of cn
     *     "cn;lang-ja"    // Only a Japanese version of cn
     *</PRE>
     * @return attribute that has exactly the same name, or null
     * (if no attribute in the set matches the specified name).
     * @see org.ietf.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute( String attrName ) {
        if ( attrName == null ) {
            return null;
        } else if ( _attrHash != null ) {
            return (LDAPAttribute)_attrHash.get( attrName.toLowerCase() );
        } else {
            for ( int i = 0; i < _attrs.length; i++ ) {
                if ( attrName.equalsIgnoreCase(_attrs[i].getName()) ) {
                    return _attrs[i];
                }
            }
            return null;
        }
    }

    /**
     * Prepares hashtable for fast attribute lookups.
     */
    private void prepareHashtable( boolean force ) {
        if ( (_attrHash == null) &&
             (force || (_attrs.length >= ATTR_COUNT_REQUIRES_HASH)) ) {
            if ( _attrHash == null ) {
                _attrHash = new HashMap();
            } else {
                _attrHash.clear();
            }
            for ( int j = 0; j < _attrs.length; j++ ) {
                _attrHash.put( _attrs[j].getName().toLowerCase(), _attrs[j] );
            }
        }
    }

    /**
     * Returns the subtype that matches the attribute name specified
     * by <CODE>attrName</CODE> and the language specificaton identified
     * by <CODE>lang</CODE>.
     * <P>
     *
     * If no attribute in the set has the specified name and subtype,
     * the method returns <CODE>null</CODE>.
     *
     * Attributes containing subtypes other than <CODE>lang</CODE>
     * (for example, <CODE>cn;binary</CODE>) are returned only if
     * they contain the specified <CODE>lang</CODE> subtype and if
     * the set contains no attribute having only the <CODE>lang</CODE>
     * subtype.  (For example, <CODE>getAttribute( "cn", "lang-ja" )</CODE>
     * returns <CODE>cn;lang-ja;phonetic</CODE> only if the 
     * <CODE>cn;lang-ja</CODE> attribute does not exist.)
     * <P>
     *
     * If null is specified for the <CODE>lang</CODE> argument,
     * calling this method is the same as calling the
     * <CODE>getAttribute(attrName)</CODE> method.
     * <P>
     *
     * For example, suppose an entry contains only the following attributes:
     * <P>
     * <UL>
     * <LI><CODE>cn;lang-en</CODE>
     * <LI><CODE>cn;lang-ja-JP-kanji</CODE>
     * <LI><CODE>sn</CODE>
     * </UL>
     * <P>
     *
     * Calling the following methods will return the following values:
     * <P>
     * <UL>
     * <LI><CODE>getAttribute( "cn" )</CODE> returns <CODE>null</CODE>.
     * <LI><CODE>getAttribute( "sn" )</CODE> returns the "<CODE>sn</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-en-us" )</CODE> returns the "<CODE>cn;lang-en</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-en" )</CODE> returns the "<CODE>cn;lang-en</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-ja" )</CODE> returns <CODE>null</CODE>.
     * <LI><CODE>getAttribute( "sn", "lang-en" )</CODE> returns the "<CODE>sn</CODE>" attribute.
     *</UL>
     * <P>
     * @param attrName name of attribute to find in the entry
     * @param lang a language specification
     * @return the attribute that matches the base name and that best
     * matches any specified language subtype.
     * @see org.ietf.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute( String attrName, String lang ) {
        if ( (lang == null) || (lang.length() < 1) )
            return getAttribute( attrName );

        String langLower = lang.toLowerCase();
        if ((langLower.length() < 5) ||
            ( !langLower.substring( 0, 5 ).equals( "lang-" ) )) {
            return null;
        }
        StringTokenizer st = new StringTokenizer( langLower, "-" );
        // Skip first token, which is "lang-"
        st.nextToken();
        String[] langComponents = new String[st.countTokens()];
        int i = 0;
        while ( st.hasMoreTokens() ) {
            langComponents[i] = st.nextToken();
            i++;
        }

        String searchBasename = LDAPAttribute.getBaseName(attrName);
        String[] searchTypes = LDAPAttribute.getSubtypes(attrName);
        LDAPAttribute found = null;
        int matchCount = 0;
        for( i = 0; i < _attrs.length; i++ ) {
            boolean isCandidate = false;
            LDAPAttribute attr = _attrs[i];
            // Same base name?
            if ( attr.getBaseName().equalsIgnoreCase(searchBasename) ) {
                // Accept any subtypes?
                if( (searchTypes == null) || (searchTypes.length < 1) ) {
                    isCandidate = true;
                } else {
                    // No, have to check each subtype for inclusion
                    if( attr.hasSubtypes( searchTypes ) )
                    isCandidate = true;
                }
            }
            String attrLang = null;
            if ( isCandidate ) {
                attrLang = attr.getLangSubtype();
                // At this point, the base name and subtypes are okay
                if ( attrLang == null ) {
                    // If there are no language attributes, this one is okay
                    found = attr;
                } else {
                    // We just have to check for language match
                    st = new StringTokenizer( attrLang.toLowerCase(), "-" );
                    // Skip first token, which is "lang-"
                    st.nextToken();
                    // No match if the attribute's language spec is longer
                    // than the target one
                    if ( st.countTokens() > langComponents.length )
                        continue;

                    // How many subcomponents of the language match?
                    int j = 0;
                    while ( st.hasMoreTokens() ) {
                        if ( !langComponents[j].equals( st.nextToken() ) ) {
                            j = 0;
                            break;
                        }
                        j++;
                    }
                    if ( j > matchCount ) {
                        found = attr;
                        matchCount = j;
                    }
                }
            }
        }
        return found;
    }

    /**
     * Retrieves the string representation of all attributes
     * in the attribute set.  For example:
     *
     * <PRE>
     * LDAPAttributeSet: LDAPAttribute {type='cn', values='Barbara Jensen,Babs
     * Jensen'}LDAPAttribute {type='sn', values='Jensen'}LDAPAttribute {type='
     * givenname', values='Barbara'}LDAPAttribute {type='objectclass', values=
     * 'top,person,organizationalPerson,inetOrgPerson'}LDAPAttribute {type='ou',
     * values='Product Development,People'}
     * </PRE>
     *
     * @return string representation of all attributes in the set.
     */
    public String toString() {
        StringBuffer sb = new StringBuffer("LDAPAttributeSet: ");
        for( int i = 0; i < _attrs.length; i++ ) {
            if (i != 0) {
                sb.append(" ");
            }            
            sb.append(_attrs[i].toString());
        }
        return sb.toString();
    }
}
