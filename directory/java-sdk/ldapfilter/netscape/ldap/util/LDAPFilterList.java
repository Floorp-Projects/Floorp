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
 * The list of LDAPFilter objects returned from a LDAPFilterDescriptor
 * Object.  Note that this is an enumeration, so if multiple iterations
 * are needed, save the results.
 *
 * @see LDAPFilterDescriptor
 * @see LDAPFilter
 * @version 1.0
 */

public class LDAPFilterList implements Enumeration {
    private Vector m_vFilterList;
    private static int DEFAULT_LIST_SIZE = 2;

    /**
     * Constructs an LDAPFilterList object.  This methos shouldn't need to
     * be called by the developer directly.  Construction of the
     * LDAPFilterList object should take place when the Prepare function
     * of LDAPFilterDescriptor is called.
     *
     */
    public LDAPFilterList () {
        m_vFilterList = new Vector ( DEFAULT_LIST_SIZE );
    }


    /**
     *  Add an LDAPFilter to the private vector.  Since the filter we're
     *  being passed has already been cloned from the master
     *  LDAPFilterDescriptor set, all we have to do is add it to the list.
     */
    void add ( LDAPFilter filter ) {
        m_vFilterList.addElement ( filter );
    }

    /**
     * Returns true if there are any LDAPFilter objects to returned.
     */
    public boolean hasMoreElements() {
        return ( ! m_vFilterList.isEmpty() );
    }

    /**
     * Returns the next LDAPFilter as an Object.  Note: the preferred way
     * to return the next LDAPFilter is to call next()
     *
     * @see LDAPFilterList#next()
     * @return The next LDAPFilter object (as an instance of Object)
     */
    public Object nextElement() {
        Object o = m_vFilterList.firstElement();
        m_vFilterList.removeElementAt ( 0 );
        return o;
    }

    /**
     * Returns the next LDAPFilter
     *
     * @return The next LDAPFilter
     */
    public LDAPFilter next() {
        Object o = m_vFilterList.firstElement();
        m_vFilterList.removeElementAt ( 0 );
        return (LDAPFilter)o;
    }

    /**
     * Return the number of filters in the filter list.
     * Note that this number decreases every time next() or nextElement() is
     * called because the elements are removed as they're returned.
     */
    public int numFilters (){
        return m_vFilterList.size();
    }
}

