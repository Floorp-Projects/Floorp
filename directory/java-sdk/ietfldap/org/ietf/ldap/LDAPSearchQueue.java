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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.io.Serializable;
import java.util.*;

import org.ietf.ldap.client.*;

/**
 * Manages search results, references and responses returned on one or 
 * more search requests
 *
 */
public class LDAPSearchQueue extends LDAPMessageQueueImpl {

    static final long serialVersionUID = -7163312406176592277L;

    /**
     * Constructs a search message queue
     *
     * @param asynchOp a boolean flag indicating whether the object is used 
     * for asynchronous LDAP operations
     * @param cons LDAP search constraints
     * @see org.ietf.ldap.LDAPAsynchronousConnection
     */
    LDAPSearchQueue( boolean asynchOp,
                     LDAPSearchConstraints cons ) {
        super( asynchOp );
        _constraints = cons;
    }

    /**
     * Blocks until a search result, reference or response is available,
     * or until all operations associated with the object have completed
     * or been canceled.
     * Wakes up the LDAPConnThread if the backlog limit has been reached.
     *
     * @return a search result, search reference, search response message,
     * or null if there are no more outstanding requests. 
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     * @see LDAPResponse
     * @see LDAPSearchResult
     * @see LDAPSearchResultReference
     */
    public LDAPMessage getResponse() throws LDAPException{
        LDAPMessage result = super.getResponse();

        // Notify LDAPConnThread to wake up if backlog limit has been reached
        if ( result instanceof LDAPSearchResult ||
             result instanceof LDAPSearchResultReference ) {
            LDAPConnThread connThread =
                getConnThread( result.getMessageID() );
            if ( connThread != null ) {
                connThread.resultRetrieved();
            }
        }
        
        return result;
    }

    /**
     * Blocks until a response is available for a particular message ID, or 
     * until all operations associated with the message ID have completed or 
     * been canceled, and returns the response. If there is no outstanding 
     * operation for the message ID (or if it is zero or a negative number), 
     * IllegalArgumentException is thrown.
     *
     * @param msgid A particular message to query for responses available
     * @return a response for an LDAP operation or null if there are no
     * more outstanding requests.
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     */

    public  LDAPMessage getResponse( int msgid )
        throws LDAPException {
        LDAPMessage result = super.getResponse( msgid );

        // Notify LDAPConnThread to wake up if backlog limit has been reached
        if ( result instanceof LDAPSearchResult ||
             result instanceof LDAPSearchResultReference ) {
            LDAPConnThread connThread =
                getConnThread( result.getMessageID() );
            if ( connThread != null ) {
                connThread.resultRetrieved();
            }
        }
        
        return result;
    }

    /**
     * Reports true if all results for a particular message ID have been 
     * received by the API implementation. That is the case if a
     * searchResultDone response has been received by the SDK. There may
     * still be messages queued in the object for retrieval by the
     * application. If there is no outstanding operation for the message ID
     * (or if it is zero or a negative number), IllegalArgumentException is
     * thrown.
     *
     * @param msgid A particular message to query for completion
     * @return true if all results for a particular message ID have been 
     * received
     */
    public synchronized boolean isComplete( int msgid ) throws LDAPException {

        boolean OK = false;
        for ( int i = (_messageQueue.size()-1); i >= 0; i-- ) {
            LDAPMessage msg = (LDAPMessage)_messageQueue.get(i);
            if ( msg.getMessageID() == msgid ) {
                OK = true;
                break;
            }
        }
        if ( !OK ) {
            throw new IllegalArgumentException( "Invalid msg ID: " + msgid );
        }
        
        // Search an instance of LDAPResponse
        for ( int i = _messageQueue.size()-1; i >= 0; i-- ) {
            LDAPMessage msg = (LDAPMessage)_messageQueue.get(i);
            if ( msg instanceof LDAPResponse ) {
                return true;
            }
        }
        return false;
    }

    /**
     * Gets the key of the cache entry
     *
     * @return the key of the cache entry
     */
    Long getKey() {
        return _key;
    }

    /**
     * Returns the search constraints used to create this object.
     *
     * @return the search constraints used to create this object.
     */
    LDAPSearchConstraints getSearchConstraints() {
        return _constraints;
    }

    /**
     * Resets the state of this object so it can be recycled
     * Used by LDAPConnection synchronous operations.
     */
    void reset() {
        super.reset();
        _constraints = null;
    }
    
    /**
     * Sets the key of the cache entry. The queue needs to know this value
     * when the results get processed. After the results have been
     * saved in the vector, then the key and a vector of results are put in
     * the cache.
     *
     * @param key the key of the cache entry
     */
    void setKey( Long key ) {
        _key = key;
    }

    /**
     * Sets search constraints
     *
     * @param cons LDAP search constraints
     */
     void setSearchConstraints( LDAPSearchConstraints cons ) {
        _constraints = cons;
    }

    // this instance variable is only for cache purposes
    private Long _key = null;
    private LDAPSearchConstraints _constraints;
}

