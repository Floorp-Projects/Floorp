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
import netscape.ldap.client.*;

/**
 * This implements an object for taking the asynchronous LDAP responses
 * and making them appear somewhat synchronized.  Uses built-in Java
 * monitors to block until a response is received.  This is also used
 * to communicate exceptions across threads.  This is used in particular
 * for LDAP search operations; all other operations use the generic
 * LDAPResponseListener object.
 *
 * This class is part of the implementation; it is of no interest to
 * developers.
 *
 * @version 1.0
 */
class LDAPSearchListener extends LDAPResponseListener {
    private Vector searchResults;
    private int lastRetrieved = -1;

    // this instance variable is only for cache purpose
    private Long m_key = null;

    /**
     * Constructs a LDAP search listener.
     */
    LDAPSearchListener ( LDAPConnection conn ) {
        super ( conn );
        searchResults = new Vector ();
    }

    /**
     * Informs the object of new search items (but not a response)
     * @param newResult new search result message
     */
    synchronized void addSearchResult (JDAPMessage newResult) {
        searchResults.addElement (newResult);
        notifyAll ();
    }

    /**
     * Returns a collection of the currently-known search results.
     * @return search result in enumeration
     */
    Enumeration getSearchResults () {
        return searchResults.elements ();
    }

    /**
     * Returns a count of the currently-known search results.
     * @return search result count
     */
    int getCount () {
        return searchResults.size ();
    }

    /**
     * Resets the state of this object, so it can be recycled.
     */
    void reset () {
        super.reset();
        lastRetrieved = -1;
        if ( searchResults != null )
            searchResults.removeAllElements();
    }

    /**
     * Returns the next server search result.  This method only makes
     * sense in asynchronous mode.  It will block if no new messages have
     * been received since the last call to nextResult.  If a response
     * is received indicating that there are no more matches, this
     * method returns null.
     * @return jdap message
     */
    synchronized JDAPMessage nextResult () {
        while (lastRetrieved >= (searchResults.size()-1)) {
            if (isResponseReceived()) {
                searchResults.removeAllElements();
                return null;
            }

            try {
                wait();
            } catch (InterruptedException e ) {
            }
        }

        lastRetrieved++;
        JDAPMessage result = (JDAPMessage)searchResults.elementAt (lastRetrieved);
        /* Allow garbage collection to free this result */
        searchResults.setElementAt (null, lastRetrieved);
        return result;
    }

    /**
     * Set the key of the cache entry. The listener needs to know this value
     * when the results get processed in the queue. After the results have been
     * saved in the vector, then the key and a vector of results are put in
     * the cache.
     * @param key The key of the cache entry
     */
    void setKey(Long key) {
        m_key = key;
    }

    /**
     * Get the key of the cache entry.
     * @return The key of the cache entry
     */
    Long getKey() {
        return m_key;
    }
}
