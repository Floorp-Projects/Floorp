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

import java.util.*;
import netscape.ldap.client.*;

/**
 * Manages search results, references and responses returned on one or 
 * more search requests
 *
 */
public class LDAPSearchListener extends LDAPMessageQueue {

    // this instance variable is only for cache purpose
    private Long m_key = null;
    private LDAPSearchConstraints m_constraints;

    /**
     * Constructs a LDAP search listener.
     * @param asynchOp a boolean flag indicating whether the object is used 
     * for asynchronous LDAP operations
     * @param cons LDAP search constraints
     * @see netscape.ldap.LDAPAsynchronousConnection
     */
    LDAPSearchListener ( boolean asynchOp,
                         LDAPSearchConstraints cons ) {
        super ( asynchOp );
        m_constraints = cons;
    }

    /**
     * Block until all results are in. Used for synchronous search with 
     * batch size of zero.
     * @return search response message.
     * @exception Network exception error
     */
    LDAPResponse completeSearchOperation () throws LDAPException{
        return completeRequest();
    }


    /**
     * Blocks until a search result, reference or response is available,     * or until all operations associated with the object have completed     * or been canceled.
     *
     * @return a search result, search reference, search response message,
     * or null if there are no more outstanding requests. 
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     * @see LDAPResponse
     * @see LDAPSearchResult
     * @see LDAPSearchResultReference
     */
    public LDAPMessage getResponse () throws LDAPException{
        return nextMessage();
    }


    /**
     * Override nextMessage to wake up the LDAPConnThread if 
     *  backlog limit has been reached 
     */
    LDAPMessage nextMessage()  throws LDAPException{
        LDAPMessage result = super.nextMessage();

        // Notify LDAPConnThread to wake up if backlog limit has been reached
        if (result instanceof LDAPSearchResult || result instanceof LDAPSearchResultReference) {
            LDAPConnection ld = getConnection(result.getID());
            if (ld != null) {
                ld.resultRetrieved();
            }                
        }            
        
        return result;
    }

    /**
     * Merge two search listeners
     * Move/append the content from another search listener to this one.
     *   
     * To be used for synchronization of asynchronous LDAP operations where
     * requests are sent by one thread but processed by another one
     * 
     * A client may be implemented in such a way that one thread makes LDAP
     * requests and calls l.getIDs(), while another thread is responsible for
     * processing of responses (call l.getResponse()). Both threads are using
     * the same listener objects. In such a case, a race
     * condition may occur, where a LDAP response message is retrieved and
     * the request terminated (request ID removed) before the first thread
     * has a chance to execute l.getIDs().
     * The proper way to handle this scenario is to create a separate listener
     * for each new request, and after l.getIDs() has been invoked, merge the
     * new request with the existing one.
     * @param listener2 the listener with which to merge
     */
    public void merge(LDAPSearchListener listener2) {
        super.merge(listener2);
    }
    
    /**
     * Reports true if a response has been received from the server.
     *
     * @return a flag indicating whether the response message queue is empty.
     */
    public boolean isResponseReceived() {
        return super.isMessageReceived();
    }

    /**
     * Returns message IDs for all outstanding requests
     * @return message ID array.
     */
    public int[] getIDs() {
        return super.getIDs();
    }
    
    /**
     * Return the search constraints used to create this object.
     * @return the search constraints used to create this object.
     */
    LDAPSearchConstraints getSearchConstraints() {
        return m_constraints;
    }

    /**
     * Set the key of the cache entry. The listener needs to know this value
     * when the results get processed in the queue. After the results have been
     * saved in the vector, then the key and a vector of results are put in
     * the cache.
     * @param key the key of the cache entry
     */
    void setKey(Long key) {
        m_key = key;
    }

    /**
     * Get the key of the cache entry.
     * @return the key of the cache entry.
     */
    Long getKey() {
        return m_key;
    }
}
