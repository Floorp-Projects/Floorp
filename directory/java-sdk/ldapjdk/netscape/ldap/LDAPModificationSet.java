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

/**
 * Represents a set of modifications to be made to attributes in an entry.
 * A set of modifications is made up of <CODE>LDAPModification</CODE> objects.
 * <P>
 *
 * After you specify a change to an attribute, you can execute the change
 * by calling the <CODE>LDAPConnection.modify</CODE> method and specifying
 * the DN of the entry that you want to modify.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPModification
 * @see netscape.ldap.LDAPConnection#modify(java.lang.String, netscape.ldap.LDAPModificationSet)
 */
public class LDAPModificationSet {

    private int current = 0;
    private Vector modifications;

    /**
     * Constructs a new, empty set of modifications.
     * You can add modifications to this set by calling the
     * <CODE>LDAPModificationsSet.add</CODE> method.
     */
    public LDAPModificationSet() {
        modifications = new Vector();
        current = 0;
    }

    /**
     * Retrieves the number of <CODE>LDAPModification</CODE>
     * objects in this set.
     * @return the number of <CODE>LDAPModification</CODE>
     * objects in this set.
     */
    public int size () {
        return modifications.size();
    }

    /**
     * Retrieves a particular <CODE>LDAPModification</CODE> object at
     * the position specified by the index.
     * @param index position of the <CODE>LDAPModification</CODE>
     * object that you want to retrieve.
     * @return <CODE>LDAPModification</CODE> object representing
     * a change to make to an attribute.
     */
    public LDAPModification elementAt (int index) {
        return (LDAPModification)modifications.elementAt(index);
    }

    /**
     * Removes a particular <CODE>LDAPModification</CODE> object at
     * the position specified by the index.
     * @param index position of the <CODE>LDAPModification</CODE>
     * object that you want to remove
     */
    public void removeElementAt( int index ) {
        modifications.removeElementAt(index);
    }

    /**
     * Specifies another modification to be added to the set of modifications.
     * @param op the type of modification to make. This can be one of the following:
     *   <P>
     *   <UL>
     *   <LI><CODE>LDAPModification.ADD</CODE> (the value should be added to the attribute)
     *   <LI><CODE>LDAPModification.DELETE</CODE> (the value should be removed from the attribute)
     *   <LI><CODE>LDAPModification.REPLACE</CODE> (the value should replace the existing value of the attribute)
     *   </UL><P>
     * If you are working with a binary value (not a string value), you need to bitwise OR (|) the
     * modification type with <CODE>LDAPModification.BVALUES</CODE>.
     * <P>
     *
     * @param attr the attribute (possibly with values) to modify
     */
    public synchronized void add( int op, LDAPAttribute attr ) {
        LDAPModification mod = new LDAPModification( op, attr );
        modifications.addElement( mod );
    }

    /**
     * Removes the first attribute with the specified name in the set of modifications.
     * @param name name of the attribute to remove
     */
    public synchronized void remove( String name ) {
        for( int i = 0; i < modifications.size(); i++ ) {
            LDAPModification mod = (LDAPModification)modifications.elementAt( i );
            LDAPAttribute attr = mod.getAttribute();
            if ( name.equalsIgnoreCase( attr.getName() ) ) {
                modifications.removeElementAt( i );
                break;
            }
        }
    }

    /**
     * Retrieves the string representation of the
     * modification set.
     *
     * @return string representation of the modification set.
     */
    public String toString() {
        String s = "LDAPModificationSet: {";
        for( int i = 0; i < modifications.size(); i++ ) {
            s += (LDAPModification)modifications.elementAt(i);
            if ( i < modifications.size()-1 ) {
                s += ", ";
            }
        }
        s += "}";
        return s;
    }
}
