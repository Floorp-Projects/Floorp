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
package netscape.ldap;

import java.util.Vector;

/**
 * A queue of response messsages from the server. Multiple requests
 * can be multiplexed on the same queue. For synchronous LDAPConnection
 * requests, there will be only one request per queue. For asynchronous
 * LDAPConnection requests, the user can add multiple request to the
 * same queue.
 * 
 * Superclass for LDAResponseListener and LDAPSearchListener
 *
 */
class LDAPMessageQueue implements java.io.Serializable {

    static final long serialVersionUID = -7163312406176592278L;

    /**
     * Request entry encapsulates request parameters
     */
    private static class RequestEntry {
        int id;
        LDAPConnection connection;
        LDAPConnThread connThread;
        long timeToComplete;
        
    
        RequestEntry(int id, LDAPConnection connection,
                     LDAPConnThread connThread, int timeLimit) {
            this.id= id;
            this.connection = connection;
            this.connThread = connThread;
            this.timeToComplete = (timeLimit == 0) ?
                 Long.MAX_VALUE : (System.currentTimeMillis() + timeLimit);
        }    
    }

    /**
     * Internal variables
     */
    private /*LDAPMessage */ Vector m_messageQueue = new Vector(1);
    private /*RequestEntry*/ Vector m_requestList  = new Vector(1);
    private LDAPException m_exception; /* For network errors */
    private boolean m_asynchOp;
    
    // A flag whether there are time constrained requests 
    private boolean m_timeConstrained;

    /**
     * Constructor
     * @param asynchOp a boolean flag  that is true if the object is used 
     * for asynchronous LDAP operations
     * @see netscape.ldap.LDAPAsynchronousConnection
     */   
    LDAPMessageQueue (boolean asynchOp) {
        m_asynchOp = asynchOp;
    }

    /**
     * Returns a flag whether the listener is used for asynchronous LDAP
     * operations
     * @return asynchronous operation flag.
     * @see netscape.ldap.LDAPAsynchronousConnection
     */
    boolean isAsynchOp() {
        return m_asynchOp;
    }

    /**
     * Blocks until a response is available.
     * Used by LDAPConnection.sendRequest (synch ops) to test if the server
     * is really available after a request had been sent.
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     */
    synchronized void waitFirstMessage () throws LDAPException {
        
        while(m_requestList.size() != 0 && m_exception == null && m_messageQueue.size() == 0) {
            waitForMessage();
        }
        
        // Network exception occurred ?
        if (m_exception != null) {
            LDAPException ex = m_exception;
            m_exception = null;
            throw ex;
        }        
    }
    
    /**
     * Blocks until a response is available or until all operations
     * associated with the object have completed or been canceled.
     * @return LDAP message or null if there are no more outstanding requests.
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     */
    synchronized LDAPMessage nextMessage () throws LDAPException {
        
        while(m_requestList.size() != 0 && m_exception == null && m_messageQueue.size() == 0) {
            waitForMessage();
        }
        
        // Network exception occurred ?
        if (m_exception != null) {
            LDAPException ex = m_exception;
            m_exception = null;
            throw ex;
        }   

        // Are there any outstanding requests left
        if (m_requestList.size() == 0) {
            return null; // No outstanding requests
        }
            
        // Dequeue the first entry
        LDAPMessage msg = (LDAPMessage) m_messageQueue.elementAt(0);
        m_messageQueue.removeElementAt(0);
        
        // Is the ldap operation completed?
        if (msg instanceof LDAPResponse) {
            removeRequest(msg.getMessageID());
        }
        
        return msg;
    }

    /**
     * Wait for request to complete. This method blocks until a message of
     * type LDAPResponse has been received. Used by synchronous search 
     * with batch size of zero (block until all results are received)
     * @return LDAPResponse message or null if there are no more outstanding requests.
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     */
    synchronized LDAPResponse completeRequest () throws LDAPException {

        while (true) {
            while(m_requestList.size() != 0 && m_exception == null && m_messageQueue.size() == 0) {            
                waitForMessage();
            }

            // Network exception occurred ?
            if (m_exception != null) {
                LDAPException ex = m_exception;
                m_exception = null;
                throw ex;
            }   

            // Are there any outstanding requests left?
            if (m_requestList.size() == 0) {
                return null; // No outstanding requests
            }
            
            // Search an instance of LDAPResponse
            for (int i= m_messageQueue.size()-1; i >=0; i--) {
                LDAPMessage msg = (LDAPMessage) m_messageQueue.elementAt(i);
                if (msg instanceof LDAPResponse) {
                
                    // Dequeue the entry and return
                    m_messageQueue.removeElementAt(i);
                    return (LDAPResponse)msg;
                }
            }

            // Not found, wait for the next message
            waitForMessage();
        }            
    }

    /**
     * Wait for a response message. Process interrupts and honor
     * time limit if set for any request
     */
    synchronized private void waitForMessage () throws LDAPException{
        
        if (!m_timeConstrained) {
            try {
                wait ();
                return;
            } catch (InterruptedException e) {
                throw new LDAPInterruptedException("Interrupted LDAP operation");
            }
        }

        /**
         * Perform time constrained wait
         */
        long minTimeToComplete = Long.MAX_VALUE;
        long now = System.currentTimeMillis();
        for (int i=0; i < m_requestList.size(); i++) {
            RequestEntry entry = (RequestEntry)m_requestList.elementAt(i);
            
            // time limit exceeded ?
            if (entry.timeToComplete <= now) {
                entry.connection.abandon(entry.id);
                throw new LDAPException("Time to complete operation exceeded",
                                         LDAPException.LDAP_TIMEOUT);
            }                
                
            if (entry.timeToComplete < minTimeToComplete) {
                minTimeToComplete = entry.timeToComplete;
            }            
        }
        
        long timeLimit = (minTimeToComplete == Long.MAX_VALUE)? 
                         0 :(minTimeToComplete - now);
        
        try {
            m_timeConstrained = (timeLimit != 0);
            wait (timeLimit);
        } catch (InterruptedException e) {
            throw new LDAPInterruptedException("Interrupted LDAP operation");
        }
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
    void merge(LDAPMessageQueue mq2) {
        
        // Yield just in case the LDAPConnThread is in the process of
        // dispatching a message
        Thread.yield();
        
        synchronized(this) {
            
            synchronized (mq2) {
                for (int i=0; i < mq2.m_messageQueue.size(); i++) {
                    m_messageQueue.addElement(mq2.m_messageQueue.elementAt(i));
                }
                if (mq2.m_exception != null) {
                    m_exception = mq2.m_exception;
                }
                for (int i=0; i < mq2.m_requestList.size(); i++) {
                    RequestEntry entry = (RequestEntry)mq2.m_requestList.elementAt(i);
                    m_requestList.addElement(entry);
                    // Notify LDAPConnThread to redirect mq2 designated responses to this mq
                    entry.connThread.changeListener(entry.id, this);
                }
                    
                mq2.reset();
                notifyAll(); // notify for mq2
            }

            notifyAll();  // notify this mq
        }
    }


    /**
     * Retrieves all messages currently in the queue without blocking
     * @return vector of messages.
     */
    synchronized Vector getAllMessages() {
        Vector result = m_messageQueue;
        m_messageQueue = new Vector(1);
        return result;
    }
    
    /**
     * Queues the LDAP server's response.  This causes anyone waiting
     * in nextMessage() to unblock.
     * @param msg response message
     */
    synchronized void addMessage (LDAPMessage msg) {
        m_messageQueue.addElement(msg);
        
        // Mark conn as bound for asych bind operations
        if (isAsynchOp() && msg.getType() == msg.BIND_RESPONSE) {
            if (((LDAPResponse) msg).getResultCode() == 0) {
                getConnection(msg.getMessageID()).setBound(true);
            }                
        }
                
        notifyAll ();
    }

    /**
     * Signals that a network exception occured while servicing the
     * request.  This exception will be throw to any thread waiting
     * in nextMessage()
     * @param connThread LDAPConnThread on which the exception occurred
     * @param e exception
     */
    synchronized void setException (LDAPConnThread connThread, LDAPException e) {        
        m_exception = e;
        removeAllRequests(connThread);
        notifyAll ();
    }

    /**
     * Checks if response message is received.
     * @return true or false.
     */
    boolean isMessageReceived() {
        return m_messageQueue.size() != 0;
    }

    /**
     * Returns the count of queued messages
     * @return message count.
     */
    public int getMessageCount () {
        return m_messageQueue.size();
    }

    /**
     * Remove all queued messages associated with the request ID
     * Called when a LDAP operation is abandoned
     * 
     * Not synchronized as its private and can be called only by
     * abandon() and removeAllRequests()
     * 
     * @return count of removed messages.
     */
    private int removeAllMessages(int id) {
        int removeCount=0;
        for (int i=(m_messageQueue.size()-1); i>=0; i--) {
            LDAPMessage msg = (LDAPMessage)m_messageQueue.elementAt(i);
            if (msg.getMessageID() == id) {
                m_messageQueue.removeElementAt(i);
                removeCount++;
            }
        }
        return removeCount;
    }   

    /**
     * Resets the state of this object, so it can be recycled.
     * Used by LDAPConnection synchronous operations.
     * @see netscape.ldap.LDAPConnection#getResponseListener
     * @see netscape.ldap.LDAPConnection#getSearchListener
     */
    void reset () {
        m_exception = null;
        m_messageQueue.removeAllElements();
        m_requestList.removeAllElements();
        m_timeConstrained = false;
    }
    
    /**
     * Returns the connection associated with the specified request id
     * @param id request id
     * @return connection.
     */
    synchronized LDAPConnection getConnection(int id) {
        for (int i=0; i < m_requestList.size(); i++) {
            RequestEntry entry = (RequestEntry)m_requestList.elementAt(i);
            if (id == entry.id) {                
                return entry.connection;
            }
        }
        return null;

    }

    /**
     * Returns the connection thread associated with the specified request id
     * @param id request id.
     * @return connection thread.
     */
    synchronized LDAPConnThread getConnThread(int id) {
        for (int i=0; i < m_requestList.size(); i++) {
            RequestEntry entry = (RequestEntry)m_requestList.elementAt(i);
            if (id == entry.id) {                
                return entry.connThread;
            }
        }
        return null;

    }

    /**
     * Returns message ID of the last request
     * @return message ID.
     */
    synchronized int getMessageID() {
        int reqCnt = m_requestList.size();
        if ( reqCnt == 0) {
            return -1;
        }
        else {
            RequestEntry entry = (RequestEntry)m_requestList.elementAt(reqCnt-1);
            return entry.id;
        }
    }
    
    /**
     * Returns a list of message IDs for all outstanding requests
     * @return message ID array.
     */
    synchronized int[] getMessageIDs() {
        int[] ids = new int[m_requestList.size()];
        for (int i=0; i < ids.length; i++) {
            RequestEntry entry = (RequestEntry)m_requestList.elementAt(i);
            ids[i] = entry.id;
        }
        return ids;
    }    

    /**
     * Registers a LDAP request
     * @param id LDAP request message ID
     * @param connection LDAP Connection for the message ID
     * @param connThread a physical connection to the server
     * @param timeLimit the maximum number of milliseconds to wait for
     * the request to complete 
    */
    synchronized void addRequest(int id, LDAPConnection connection,
                                 LDAPConnThread connThread, int timeLimit) {
        
        m_requestList.addElement(new RequestEntry(id, connection,
                                                  connThread, timeLimit));
        if (timeLimit != 0) {
            m_timeConstrained = true;
        }
        notifyAll();
    }

    /**
     * Returns the number of outstanding requests.
     * @return outstanding request count.
     */    
    public int getRequestCount() {
        return m_requestList.size();
    }
    
    /**
     * Remove request with the specified ID
     * Called when a LDAP operation is abandoned (called from
     * LDAPConnThread), or terminated (called by nextMessage() when
     * LDAPResponse message is received) 
     * @return flag indicating whether the request was removed.
     */
    synchronized boolean removeRequest(int id) {
        for (int i=0; i < m_requestList.size(); i++) {
            RequestEntry entry = (RequestEntry)m_requestList.elementAt(i);
            if (id == entry.id) {
                m_requestList.removeElementAt(i);
                removeAllMessages(id);
                notifyAll();
                return true;
            }
        }
        return false;
    }            

    /**
     * Remove all requests associated with the specified connThread
     * Called when a connThread has a network error
     * @return number of removed requests.
     */
    synchronized int removeAllRequests(LDAPConnThread connThread) {
        int removeCount=0;
        for (int i=(m_requestList.size()-1); i>=0; i--) {
            RequestEntry entry = (RequestEntry)m_requestList.elementAt(i);
            if (connThread == entry.connThread) {
                m_requestList.removeElementAt(i);
                removeCount++;
                
                // remove all queued messages as well
                removeAllMessages(entry.id);
            }
        }
        notifyAll();
        return removeCount;
    }   

    /**
     * String representation of the object
     */
    public String toString() {
        StringBuffer sb = new StringBuffer("LDAPMessageQueue:");
        sb.append(" requestIDs={");
        for (int i=0; i < m_requestList.size(); i++) {
            if (i>0) {
                sb.append(",");
            }
            sb.append(((RequestEntry)m_requestList.elementAt(i)).id);
        }
        sb.append("} messageCount="+m_messageQueue.size());
        
        return sb.toString();
    }
}
