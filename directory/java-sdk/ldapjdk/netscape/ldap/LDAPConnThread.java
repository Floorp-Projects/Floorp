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
import netscape.ldap.client.opers.*;
import netscape.ldap.ber.stream.*;
import netscape.ldap.util.*;
import java.io.*;
import java.net.*;

/**
 * Multiple LDAPConnection clones can share a single physical connection,
 * which is maintained by a thread.
 *
 *   +----------------+
 *   | LDAPConnection | --------+
 *   +----------------+         |
 *                              |
 *   +----------------+         |        +----------------+
 *   | LDAPConnection | --------+------- | LDAPConnThread |
 *   +----------------+         |        +----------------+
 *                              |
 *   +----------------+         |
 *   | LDAPConnection | --------+
 *   +----------------+
 *
 * All LDAPConnections send requests and get responses from
 * LDAPConnThread (a thread).
 */
class LDAPConnThread extends Thread {

    /**
     * Constants
     */
    private final static int MAXMSGID = Integer.MAX_VALUE;

    /**
     * Internal variables
     */
    transient private static int m_highMsgId;
    transient private InputStream m_serverInput;
    transient private OutputStream m_serverOutput;
    transient private Hashtable m_requests;
    transient private Hashtable m_messages = null;
    transient private Vector m_registered;
    transient private boolean m_disconnected = false;
    transient private LDAPCache m_cache = null;
    transient private boolean m_doRun = true;
    private Socket m_socket = null;
    transient private Thread m_thread = null;
    transient Object m_sendRequestLock = new Object();
    transient LDAPConnSetupMgr m_connMgr = null;
    transient PrintWriter m_traceOutput = null;

    /**
     * Constructs a connection thread that maintains connection to the
     * LDAP server.
     * @param host LDAP host name
     * @param port LDAP port number
     * @param factory LDAP socket factory
     */
    public LDAPConnThread(LDAPConnSetupMgr connMgr, LDAPCache cache, OutputStream traceOutput)
        throws LDAPException {
        super("LDAPConnThread " + connMgr.getHost() +":"+ connMgr.getPort());
        m_requests = new Hashtable ();
        m_registered = new Vector ();
        m_connMgr = connMgr;
        m_socket = connMgr.getSocket();
        setCache( cache );
        setTraceOutputStream(traceOutput);
        
        setDaemon(true);
        
        try {

            m_serverInput = new BufferedInputStream (m_socket.getInputStream());
            m_serverOutput = new BufferedOutputStream (m_socket.getOutputStream());

        } catch (IOException e) {

            // a kludge to make the thread go away. Since the thread has already
            // been created, the only way to clean up the thread is to call the
            // start() method. Otherwise, the exit method will be never called
            // because the start() was never called. In the run method, the stop
            // method calls right away if the m_doRun is set to false.
            m_doRun = false;
            start();
            throw new LDAPException ( "failed to connect to server " +
                  m_connMgr.getHost(), LDAPException.CONNECT_ERROR );
        }
        start(); /* start the thread */
    }

    InputStream getInputStream() {
        return m_serverInput;
    }

    void setInputStream( InputStream is ) {
        m_serverInput = is;
    }

    OutputStream getOutputStream() {
        return m_serverOutput;
    }

    void setOutputStream( OutputStream os ) {
        m_serverOutput = os;
    }

    void setTraceOutputStream(OutputStream os) {
        synchronized (m_sendRequestLock) {
            m_traceOutput = (os == null) ? null : new PrintWriter(os);
        }            
    }
    
        
    /**
     * Set the cache to use for searches.
     * @param cache The cache to use for searches; <CODE>null</CODE> for no cache
     */
    synchronized void setCache( LDAPCache cache ) {
        m_cache = cache;
        m_messages = (m_cache != null) ? new Hashtable() : null;
    }

    /**
     * Allocates a new LDAP message ID.  These are arbitrary numbers used to
     * correlate client requests with server responses.
     * @return new unique msgId
     */
    private int allocateId () {
        synchronized (m_sendRequestLock) {
            m_highMsgId = (m_highMsgId + 1) % MAXMSGID;
            return m_highMsgId;
        }            
    }

    /**
     * Sends LDAP request via this connection thread.
     * @param request request to send
     * @param toNotify response listener to invoke when the response
     *          is ready
     */
    void sendRequest (LDAPConnection conn, JDAPProtocolOp request,
        LDAPMessageQueue toNotify, LDAPConstraints cons)
         throws LDAPException {
        if (m_serverOutput == null)
            throw new LDAPException ( "not connected to a server",
                                  LDAPException.SERVER_DOWN );

        LDAPMessage msg = 
            new LDAPMessage(allocateId(), request, cons.getServerControls());

        if ( toNotify != null ) {
            if (!(request instanceof JDAPAbandonRequest ||
                  request instanceof JDAPUnbindRequest)) {
                /* Only worry about toNotify if we expect a response... */
                this.m_requests.put (new Integer (msg.getID()), toNotify);
                /* Notify the backlog checker that there may be another outstanding
                   request */
                resultRetrieved(); 
            }
            toNotify.addRequest(msg.getID(), conn, this, cons.getTimeLimit());
        }

        synchronized( m_sendRequestLock ) {
            try {
                if (m_traceOutput != null) {
                    m_traceOutput.println(msg.toTraceString());
                    m_traceOutput.flush();
                }                    
                msg.write (m_serverOutput);
                m_serverOutput.flush ();
            } catch (IOException e) {
                networkError(e);
            }
        }
    }

    /**
     * Register with this connection thread.
     * @param conn LDAP connection
     */
    public synchronized void register(LDAPConnection conn) {
        if (!m_registered.contains(conn))
            m_registered.addElement(conn);
    }

    synchronized int getClientCount() {
        return m_registered.size();
    }

    /**
     * De-Register with this connection thread. If all the connection
     * is deregistered. Then, this thread should be killed.
     * @param conn LDAP connection
     */
    public synchronized void deregister(LDAPConnection conn) {
        m_registered.removeElement(conn);
        if (m_registered.size() == 0) {
            try {
                
                m_doRun =false;
                
                if (!m_disconnected) {
                    LDAPSearchConstraints cons = conn.getSearchConstraints();
                    sendRequest (null, new JDAPUnbindRequest (), null, cons);
                }                    
                
                if ( m_thread != null && m_thread != Thread.currentThread()) {
                    
                    m_thread.interrupt();
                    
                    // Wait up to 1 sec for thread to accept disconnect
                    // notification. When the interrupt is accepted,
                    // m_thread is set to null. See run() method.
                    try {
                        wait(1000);
                    }
                    catch (InterruptedException e) {
                    }                        
                }
                
              	cleanUp();

            } catch (Exception e) {
                LDAPConnection.printDebug(e.toString());
            }
        }
    }

    /**
     * Clean ups before shutdown the thread.
     */
    private void cleanUp() {
        if (!m_disconnected) {
            try {
                m_serverOutput.close ();
            } catch (Exception e) {
            } finally {
                m_serverOutput = null;
            }

            try {
                m_serverInput.close ();
            } catch (Exception e) {
            } finally {
                m_serverInput = null;
            }

            try {
                m_socket.close ();
            } catch (Exception e) {
            } finally {
                m_socket = null;
            }
        
            m_disconnected = true;

            /**
             * Notify the Connection Setup Manager that the connection was
             * terminated by the user
             */
            m_connMgr.disconnect();
        
            /**
             * Cancel all outstanding requests
             */
            if (m_requests != null) {
                Enumeration requests = m_requests.elements();
                while (requests.hasMoreElements()) {
                    LDAPMessageQueue listener =
                        (LDAPMessageQueue)requests.nextElement();
                    listener.removeAllRequests(this);
                }
            }                

            /**
             * Notify all the registered about this bad moment.
             * IMPORTANT: This needs to be done at last. Otherwise, the socket
             * input stream and output stream might never get closed and the whole
             * task will get stuck in the stop method when we tried to stop the
             * LDAPConnThread.
             */

            if (m_registered != null) {
                Vector registerCopy = (Vector)m_registered.clone();

                Enumeration cancelled = registerCopy.elements();

                while (cancelled.hasMoreElements ()) {
                    LDAPConnection c = (LDAPConnection)cancelled.nextElement();
                    c.deregisterConnection();
                }
            }
            m_registered = null;
            m_messages = null;
            m_requests.clear();
        }
    }

    /**
     * Sleep if there is a backlog of search results
     */
    private void checkBacklog() throws InterruptedException{
        boolean doWait;
        do {
            doWait = false;
            Enumeration listeners = m_requests.elements();
            while( listeners.hasMoreElements() ) {
                LDAPMessageQueue l =
                    (LDAPMessageQueue)listeners.nextElement();
                // If there are any threads waiting for a regular response
                // message, we have to go read the next incoming message
                if ( !(l instanceof LDAPSearchListener) ) {
                    doWait = false;
                    break;
                }
                // If the backlog of any search thread is too great,
                // wait for it to diminish, but if this is a synchronous
                // search, then just keep reading
                LDAPSearchListener sl = (LDAPSearchListener)l;
                
                // Asynch operation ?
                if (sl.isAsynchOp()) {
                    if (sl.getMessageCount() >= sl.getSearchConstraints().getMaxBacklog()) {
                        doWait = true;
                    }
                }
                // synch op with non-zero batch size ?
                else if (sl.getSearchConstraints().getBatchSize() != 0) {
                    if (sl.getMessageCount() >= sl.getSearchConstraints().getMaxBacklog()) {
                        doWait = true;
                    }
                }                    
            }
            if ( doWait ) {
                synchronized(this) {
                    wait();
                }
            }
        } while ( doWait );
    }

    /**
     * This is called when a search result has been retrieved from the incoming
     * queue. We use the notification to unblock the listener thread, if it
     * is waiting for the backlog to lighten.
     */
    synchronized void resultRetrieved() {
        notifyAll();
    }

    /**
     * Reads from the LDAP server input stream for incoming LDAP messages.
     */
    public void run() {
        
        // if there is a problem of establishing connection to the server,
        // stop the thread right away...
        if (!m_doRun) {
            return;
        }

        m_thread = Thread.currentThread();
        LDAPMessage msg = null;
        JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();

        while (m_doRun) {
            yield();
            int[] nread = new int[1];
            nread[0] = 0;
            try  {

                // Avoid too great a backlog of results
                checkBacklog();

                BERElement element = BERElement.getElement(decoder,
                                                           m_serverInput,
                                                           nread);
                msg = LDAPMessage.parseMessage(element);

                if (m_traceOutput != null) {
                    synchronized( m_sendRequestLock ) {
                        m_traceOutput.println(msg.toTraceString());
                        m_traceOutput.flush();
                    }
                }                    

                // passed in the ber element size to approximate the size of the cache
                // entry, thereby avoiding serialization of the entry stored in the
                // cache
                processResponse (msg, nread[0]);

            } catch (Exception e)  {
                if (m_doRun) {
                    networkError(e);
                    m_doRun =false;
                }
                else {
                    synchronized (this) {
                        m_thread = null;
                        notifyAll();
                    }
                }
            }
        }
    }

    /**
     * When a response arrives from the LDAP server, it is processed by
     * this routine.  It will pass the message on to the listening object
     * associated with the LDAP msgId.
     * @param incoming New message from LDAP server
     */
    private void processResponse (LDAPMessage incoming, int size) {
        Integer messageID = new Integer (incoming.getID());
        LDAPMessageQueue l = (LDAPMessageQueue)m_requests.get (messageID);

        if (l == null) {
            return; /* nobody is waiting for this response (!) */
        }

        // For asynch operations controls are to be read from the LDAPMessage
        // For synch operations controls are copied into the LDAPConnection
        // For synch search operations, controls are also copied into
        // LDAPSearchResults (see LDAPConnection.checkSearchMsg())
        if ( ! l.isAsynchOp()) {
            
            /* Were there any controls for this client? */
            LDAPControl[] con = incoming.getControls();
            if (con != null) {
                int msgid = incoming.getID();
                LDAPConnection ldc = l.getConnection(msgid);
                if (ldc != null) {
                    ldc.setResponseControls( this,
                        new LDAPResponseControl(ldc, msgid, con));
                }                    
            }
        }

        if ((l instanceof LDAPSearchListener) && m_cache != null) {
            cacheSearchResult((LDAPSearchListener)l, incoming, size);
        }            
        
        l.addMessage (incoming);

        if (incoming instanceof LDAPResponse) { 
            m_requests.remove (messageID);
        }        
    }

    private synchronized void cacheSearchResult (LDAPSearchListener l, LDAPMessage incoming, int size) {
        Integer messageID = new Integer (incoming.getID());
        Long key = l.getKey();
        Vector v = null;

        if ((m_cache == null) || (key == null)) {
            return;
        }
        
        if ((incoming instanceof LDAPSearchResult)/* ||
            (incoming instanceof LDAPSearchResultReference)*/) {

            // get the vector containing the LDAPMessages for the specified messageID
            v = (Vector)m_messages.get(messageID);

            if (v == null) {
                v = new Vector();
                // keeps track of the total size of all LDAPMessages belonging to the
                // same messageID, now the size is 0
                v.addElement(new Long(0));
            }

            // add the size of the current LDAPMessage to the lump sum
            // assume the size of LDAPMessage is more or less the same as the size
            // of LDAPEntry. Eventually LDAPEntry object gets stored in the cache
            // instead of LDAPMessage object.
            long entrySize = ((Long)v.firstElement()).longValue() + size;

            // update the lump sum located in the first element of the vector
            v.setElementAt(new Long(entrySize), 0);

            // convert LDAPMessage object into LDAPEntry which is stored to the
            // end of the Vector
            v.addElement(((LDAPSearchResult)incoming).getEntry());

            // replace the entry
            m_messages.put(messageID, v);

        } else if (incoming instanceof LDAPResponse) {

            boolean fail = ((LDAPResponse)incoming).getResultCode() > 0;

            if (!fail)  {
                // Collect all the LDAPMessages for the specified messageID
                // no need to keep track of this entry. Remove it.
                v = (Vector)m_messages.remove(messageID);

                // If v is null, meaning there are no search results from the
                // server
                if (v == null) {
                    v = new Vector();

                    // set the entry size to be 0
                    v.addElement(new Long(0));
                }

                try {
                    // add the new entry with key and value (a vector of
                    // LDAPEntry objects)
                    m_cache.addEntry(key, v);
                } catch (LDAPException e) {
                    System.out.println("Exception: "+e.toString());
                }
            }
        }
    }

    /**
     * Stop dispatching responses for a particular message ID.
     * @param id Message ID for which to discard responses.
     */
    void abandon (int id ) {
        LDAPMessageQueue l = (LDAPMessageQueue)m_requests.remove(new Integer(id));
        if (l != null) {
            l.removeRequest(id);
        }
        resultRetrieved(); // If LDAPConnThread is blocked in checkBacklog()
    }

    /**
     * Handles network errors.  Basically shuts down the whole connection.
     * @param e The exception which was caught while trying to read from
     * input stream.
     */
    private synchronized void networkError (Exception e) {

        // notify the Connection Setup Manager that the connection is lost
        m_connMgr.invalidateConnection();

        try {
            
            // notify each listener that the server is down.
            Enumeration requests = m_requests.elements();
            while (requests.hasMoreElements()) {
                LDAPMessageQueue listener =
                    (LDAPMessageQueue)requests.nextElement();
                listener.setException(this, new LDAPException("Server down",
                                                        LDAPException.OTHER));
            }

        } catch (NullPointerException ee) {
          System.err.println("Exception: "+ee.toString());
        }

        cleanUp();
    }
}
