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
import java.util.ArrayList;

/**
 * A queue of response messsages from the server. Multiple requests
 * can be multiplexed on the same queue. For synchronous LDAPConnection
 * requests, there will be only one request per queue. For asynchronous
 * LDAPConnection requests, the user can add multiple request to the
 * same queue.
 * 
 * Used as a delegate by LDAResponseListener and LDAPSearchListener
 *
 */
class LDAPMessageQueueImpl implements Serializable, LDAPMessageQueue {

    static final long serialVersionUID = -7163312406176592277L;

    /**
     * Request entry encapsulates request parameters
     */
    private static class RequestEntry {
        int id;
        LDAPConnection connection;
        LDAPConnThread connThread;
        long timeToComplete;
    
        RequestEntry( int id, LDAPConnection connection,
                      LDAPConnThread connThread, int timeLimit ) {
            this.id = id;
            this.connection = connection;
            this.connThread = connThread;
            this.timeToComplete = (timeLimit == 0) ?
                Long.MAX_VALUE : (System.currentTimeMillis() + timeLimit);
        }    
    }

    /**
     * Internal variables
     */
    protected /*LDAPMessage */ ArrayList _messageQueue = new ArrayList(1);
    private /*RequestEntry*/ ArrayList _requestList  = new ArrayList(1);
    private LDAPException _exception; /* For network errors */
    private boolean _asynchOp;
    
    // A flag to indicate if there are time constrained requests 
    private boolean _timeConstrained;

    /**
     * Constructor
     *
     * @param asynchOp <CODE>true</CODE> if the object is used 
     * for asynchronous LDAP operations
     * @see org.ietf.ldap.LDAPAsynchronousConnection
     */   
    LDAPMessageQueueImpl( boolean asynchOp ) {
        _asynchOp = asynchOp;
    }

    /**
     * Returns the count of queued messages
     *
     * @return message count
     */
    public int getMessageCount() {
        return _messageQueue.size();
    }

    /**
     * Returns a list of message IDs for all outstanding requests
     *
     * @return message ID array
     */
    synchronized public int[] getMessageIDs() {
        int[] ids = new int[_requestList.size()];
        for ( int i = 0; i < ids.length; i++ ) {
            RequestEntry entry = (RequestEntry)_requestList.get(i);
            ids[i] = entry.id;
        }
        return ids;
    }    

    /**
     * Blocks until a message is available or until all operations
     * associated with the object have completed or been canceled
     *
     * @return LDAP message or null if there are no more outstanding requests
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     */
    public synchronized LDAPMessage getResponse() throws LDAPException {

        while( true ) {
            if ( !waitForSomething() ) {
                return null; // No outstanding requests
            }

            // Dequeue the first entry
            LDAPMessage msg = (LDAPMessage)_messageQueue.get( 0 );
            _messageQueue.remove( 0 );
            
            // Has the operation completed?
            if ( msg instanceof LDAPResponse ) {
                removeRequest( msg.getMessageID() );
            }
            return msg;
        }
    }

    /**
     * Blocks until a message is available for a particular message ID, or 
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

    public synchronized LDAPMessage getResponse( int msgid )
        throws LDAPException {
            
        if ( !isValidMessageID( msgid ) ) {
            throw new IllegalArgumentException( "Invalid msg ID: " + msgid );
        }

        LDAPMessage msg = null;
        while( ( _requestList.size() != 0 ) &&
               ( _exception == null ) &&
               ( (msg = getMessageForID( msgid, true )) == null ) ) {
            waitForMessage();
        }
        
        // Network exception occurred?
        if ( _exception != null ) {
            LDAPException ex = _exception;
            _exception = null;
            throw ex;
        }   

        // Are there any outstanding requests left?
        if ( _requestList.size() == 0 ) {
            return null; // No outstanding requests
        }
            
        // Has the operation completed?
        if ( msg instanceof LDAPResponse ) {
            removeRequest( msg.getMessageID() );
        }
        
        return msg;
    }

    /**
     * Checks if a response message has been received
     *
     * @return true or false
     */
    public boolean isResponseReceived() {
        return ( _messageQueue.size() != 0 );
    }

    /**
     * Reports true if a response has been received from the server for a 
     * particular message ID. If there is no outstanding operation for the 
     * message ID (or if it is zero or a negative number), 
     * IllegalArgumentException is thrown.
     *
     * @param msgid A particular message to query for responses available
     * @return a flag indicating whether the response message queue is empty
     */
    public boolean isResponseReceived( int msgid ) {
        if ( !isValidMessageID( msgid ) ) {
            throw new IllegalArgumentException( "Invalid msg ID: " + msgid );
        }

        return ( getMessageForID( msgid, false ) != null );
    }

    /**
     * Merge two message queues.
     * Move/append the content from another message queue to this one.
     * 
     * To be used for synchronization of asynchronous LDAP operations where
     * requests are sent by one thread but processed by another one
     * 
     * A client may be implemented in such a way that one thread makes LDAP
     * requests and calls l.getMessageIDs(), while another thread is
     * responsible for
     * processing of responses (call l.getResponse()). Both threads are using
     * the same listener objects. In such a case, a race
     * condition may occur, where a LDAP response message is retrieved and
     * the request terminated (request ID removed) before the first thread
     * has a chance to execute l.getMessageIDs().
     * The proper way to handle this scenario is to create a separate listener
     * for each new request, and after l.getMessageIDs() has been invoked,
     * merge the
     * new request with the existing one.
     * @param mq2 message queue to merge with this one
     */
    public void merge( LDAPMessageQueue mq2 ) {
        
        // Yield just in case the LDAPConnThread is in the process of
        // dispatching a message
        Thread.yield();
        
        synchronized( this ) {
            LDAPMessageQueueImpl mq = (LDAPMessageQueueImpl)mq2;
            synchronized( mq ) {
				ArrayList queue2 = mq.getAllMessages();
                for( int i = 0; i < queue2.size(); i++ ) {
                    _messageQueue.add( queue2.get( i ) );
                }
                if ( mq.getException() != null ) {
                    _exception = mq.getException();
                }
				ArrayList list2 = mq.getAllRequests();
                for( int i = 0; i < list2.size(); i++ ) {
                    RequestEntry entry = (RequestEntry)list2.get( i );
                    _requestList.add( entry );
                    // Notify LDAPConnThread to redirect mq2 designated
					// responses to this mq
                    entry.connThread.changeQueue( entry.id, this );
                }

                mq.reset();
                notifyAll(); // notify for mq2
            }

            notifyAll();  // notify this mq
        }
    }

    /**
     * Gets String representation of the object
     *
     * @return String representation of the object
     */
    synchronized public String toString() {
        StringBuffer sb = new StringBuffer( "LDAPMessageQueueImpl:" );
        sb.append(" requestIDs={");
        for ( int i = 0; i < _requestList.size(); i++ ) {
            if ( i > 0 ) {
                sb.append( "," );
            }
            sb.append( ((RequestEntry)_requestList.get(i)).id );
        }
        sb.append( "} messageCount=" + _messageQueue.size() );
        
        return sb.toString();
    }

    /**
     * Retrieves the next response for a particular message ID, or null
     * if there is none
     *
     * @param msgid A particular message to query for responses available
     * @param remove <code>true</code> if the retrieved message is to be
     * removed from the queue
     * @return a flag indicating whether the response message queue is empty
     */
    synchronized LDAPMessage getMessageForID( int msgid, boolean remove ) {
        LDAPMessage msg = null;
        for ( int i = 0; i < _messageQueue.size(); i++ ) {
            msg = (LDAPMessage)_messageQueue.get(i);
            if ( msg.getMessageID() == msgid ) {
                if ( remove ) {
                    _messageQueue.remove( i );
                }
                break;
            }
        }
        return msg;
    }

    /**
     * Reports if the listener is used for asynchronous LDAP
     * operations
	 *
     * @return asynchronous operation flag.
     * @see org.ietf.ldap.LDAPAsynchronousConnection
     */
    boolean isAsynchOp() {
        return _asynchOp;
    }

    /**
     * Waits for request to complete. This method blocks until a message of
     * type LDAPResponse has been received. Used by synchronous search 
     * with batch size of zero (block until all results are received).
     *
     * @return LDAPResponse message or null if there are no more outstanding
     * requests
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     */
    synchronized LDAPResponse completeRequest() throws LDAPException {
        while ( true ) {
            if ( !waitForSomething() ) {
                return null; // No outstanding requests
            }
            
            // Search for an instance of LDAPResponse
            for ( int i = _messageQueue.size()-1; i >= 0; i-- ) {
                Object msg = _messageQueue.get(i);
                if ( msg instanceof LDAPResponse ) {
                
                    // Dequeue the entry and return
                    _messageQueue.remove( i );
                    return (LDAPResponse)msg;
                }
            }
            // Not found, wait for the next message
        }            
    }

    /**
     * Waits for any message. Processes interrupts and honors
     * time limit if set for any request.
     */
    synchronized private void waitForMessage() throws LDAPException {
        if ( !_timeConstrained ) {
            try {
                wait();
                return;
            } catch (InterruptedException e) {
                throw new LDAPInterruptedException(
                    "Interrupted LDAP operation" );
            }
        }

        /**
         * Perform time constrained wait
         */
        long minTimeToComplete = Long.MAX_VALUE;
        long now = System.currentTimeMillis();
        for ( int i = 0; i < _requestList.size(); i++ ) {
            RequestEntry entry = (RequestEntry)_requestList.get( i );
            
            // time limit exceeded ?
            if ( entry.timeToComplete <= now ) {
				entry.connection.abandon( entry.id );
                throw new LDAPException( "Time to complete operation exceeded",
                                         LDAPException.LDAP_TIMEOUT );
            }                
                
            if ( entry.timeToComplete < minTimeToComplete ) {
                minTimeToComplete = entry.timeToComplete;
            }            
        }
        
        long timeLimit = ( minTimeToComplete == Long.MAX_VALUE ) ?
            0 : ( minTimeToComplete - now );
        
        try {
            _timeConstrained = ( timeLimit != 0 );
            wait( timeLimit );
        } catch ( InterruptedException e ) {
            throw new LDAPInterruptedException( "Interrupted LDAP operation" );
        }
    }

    /**
     * Waits for a message, for the request list to be empty, or for
     * an exception
     *
     * @return <CODE>true</CODE> if the request list is not empty
     * @exception LDAPException Network error
     */
    boolean waitForSomething() throws LDAPException {
        while( (_requestList.size() != 0) &&
               (_exception == null) &&
               (_messageQueue.size() == 0) ) {
            waitForMessage();
        }
        
        // Network exception occurred ?
        if ( _exception != null ) {
            LDAPException ex = _exception;
            _exception = null;
            throw ex;
        }   

        // Are there any outstanding requests left
        if ( _requestList.size() == 0 ) {
            return false; // No outstanding requests
        }

        return true;
    }
            

    /**
     * Retrieves all messages currently in the queue without blocking.
	 * The messages are removed from the queue.
	 *
     * @return vector of messages
     */
    synchronized ArrayList getAllMessages() {
        ArrayList result = _messageQueue;
        _messageQueue = new ArrayList(1);
        return result;
    }
    
    /**
     * Retrieves all requests currently in the queue.
	 * The requests are removed from the queue.
	 *
     * @return vector of requests
     */
    synchronized ArrayList getAllRequests() {
        ArrayList result = _requestList;
        _requestList = new ArrayList(1);
        return result;
    }
    
    /**
     * Queues the LDAP server's response. This causes anyone waiting
     * in getResponse() to unblock.
	 *
     * @param msg response message
     */
    synchronized void addMessage( LDAPMessage msg ) {
        _messageQueue.add(msg);
        
        // Mark conn as bound for asych bind operations
        if ( isAsynchOp() &&
             (msg.getType() == msg.BIND_RESPONSE) ) {
            if ( ((LDAPResponse)msg).getResultCode() == 0 ) {
                getConnection( msg.getMessageID() ).markConnAsBound();
            }
        }
                
        notifyAll();
    }

	/**
	 * Gets a possible exception from the queue
	 *
	 * @return a possibly null exception
	 */
	LDAPException getException() {
		return _exception;
	}

    /**
     * Reports if the message ID is in the request list
     *
     * @param msgid The message ID to validate
     * @return <CODE>true</CODE> if the message ID is in the request list
     */
    synchronized boolean isValidMessageID( int msgid ) {
        return ( getRequestEntry( msgid ) != null );
    }

    /**
     * Signals that a network exception occured while servicing the
     * request.  This exception will be throwm to any thread waiting
     * in getResponse().
	 *
     * @param connThread LDAPConnThread on which the exception occurred
     * @param e exception
     */
    synchronized void setException( LDAPConnThread connThread,
									LDAPException e ) {
        _exception = e;
        removeAllRequests( connThread );
        notifyAll();
    }

    /**
     * Remove all queued messages associated with the request ID.
     * Called when an LDAP operation is abandoned.
     * 
     * Not synchronized as it is private and can be called only by
     * abandon() and removeAllRequests().
     * 
     * @return count of removed messages
     */
    private int removeAllMessages( int id ) {
        int removeCount = 0;
        for ( int i = (_messageQueue.size()-1); i >= 0; i-- ) {
            LDAPMessage msg = (LDAPMessage)_messageQueue.get( i );
            if ( msg.getMessageID() == id ) {
                _messageQueue.remove( i );
                removeCount++;
            }
        }
        return removeCount;
    }   

    /**
     * Resets the state of this object so it can be recycled.
     * Used by LDAPConnection synchronous operations.
	 *
     * @see org.ietf.ldap.LDAPConnection#getResponseListener
     * @see org.ietf.ldap.LDAPConnection#getSearchListener
     */
    void reset() {
        _exception = null;
        _messageQueue.clear();
        _requestList.clear();
        _timeConstrained = false;
    }
    
    /**
     * Returns the connection associated with the specified request id
	 *
     * @param id request id
     * @return connection
     */
    synchronized LDAPConnection getConnection( int id ) {
        RequestEntry entry = getRequestEntry( id );
        if ( entry != null ) {
            return entry.connection;
        }
        return null;
    }

    /**
     * Returns the connection thread associated with the specified request id
     *
     * @param id request id
     * @return connection thread
     */
    synchronized LDAPConnThread getConnThread( int id ) {
        RequestEntry entry = getRequestEntry( id );
        if ( entry != null ) {
            return entry.connThread;
        }
        return null;
    }

    /**
     * Returns the request entry associated with the specified request id
     *
     * @param id request id
     * @return request entry or null
     */
    synchronized RequestEntry getRequestEntry( int id ) {
        for ( int i = 0; i < _requestList.size(); i++ ) {
            RequestEntry entry = (RequestEntry)_requestList.get( i );
            if ( id == entry.id ) {
                return entry;
            }
        }
        return null;

    }

    /**
     * Returns message ID of the last request
     *
     * @return message ID
     */
    synchronized int getMessageID() {
        int reqCnt = _requestList.size();
        if ( reqCnt == 0 ) {
            return -1;
        } else {
            RequestEntry entry =
				(RequestEntry)_requestList.get( reqCnt-1 );
            return entry.id;
        }
    }
    
    /**
     * Registers an LDAP request
     *
     * @param id LDAP request message ID
     * @param connection LDAP Connection for the message ID
     * @param connThread a physical connection to the server
     * @param timeLimit the maximum number of milliseconds to wait for
     * the request to complete 
    */
    synchronized void addRequest( int id, LDAPConnection connection,
								  LDAPConnThread connThread, int timeLimit ) {

        _requestList.add( new RequestEntry( id, connection,
                                            connThread, timeLimit ) );
        if ( timeLimit != 0 ) {
            _timeConstrained = true;
        }
        notifyAll();
    }

    /**
     * Returns the number of outstanding requests.
     * @return outstanding request count.
     */    
    public int getRequestCount() {
        return _requestList.size();
    }
    
    /**
     * Removes the request with the specified ID.
     * Called when a LDAP operation is abandoned (called from
     * LDAPConnThread), or terminated (called by getResponse() when
     * LDAPResponse message is received).
	 *
     * @return flag indicating if the request was removed
     */
    synchronized boolean removeRequest( int id ) {
        for ( int i = 0; i < _requestList.size(); i++ ) {
            RequestEntry entry = (RequestEntry)_requestList.get( i );
            if ( id == entry.id ) {
                _requestList.remove( i );
                removeAllMessages( id );
                notifyAll();
                return true;
            }
        }
        return false;
    }            

    /**
     * Removes all requests associated with the specified connThread.
     * Called when a connThread has a network error.
	 *
     * @return number of removed requests
     */
    synchronized int removeAllRequests( LDAPConnThread connThread ) {
        int removeCount = 0;
        for ( int i = (_requestList.size()-1); i >= 0; i-- ) {
            RequestEntry entry = (RequestEntry)_requestList.get( i );
            if ( connThread == entry.connThread ) {
                _requestList.remove( i );
                removeCount++;
                
                // remove all queued messages as well
                removeAllMessages( entry.id );
            }
        }
        notifyAll();
        return removeCount;
    }   
}
