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
    transient private boolean m_failed = false;
    private Socket m_socket = null;
    transient private Thread m_thread = null;
    transient Object m_sendRequestLock = new Object();
    transient LDAPConnSetupMgr m_connMgr = null;

    /**
     * Constructs a connection thread that maintains connection to the
     * LDAP server.
     * @param host LDAP host name
     * @param port LDAP port number
     * @param factory LDAP socket factory
     */
    public LDAPConnThread(LDAPConnSetupMgr connMgr, LDAPCache cache)
        throws LDAPException {
        super("LDAPConnection " + connMgr.getHost() +":"+ connMgr.getPort());
        m_requests = new Hashtable ();
        m_registered = new Vector ();
        m_connMgr = connMgr;
        m_socket = connMgr.getSocket();
        setCache( cache );
        
        setDaemon(true);
        
        try {

            m_serverInput = new BufferedInputStream (m_socket.getInputStream());
            m_serverOutput = new BufferedOutputStream (m_socket.getOutputStream());

        } catch (IOException e) {

            // a kludge to make the thread go away. Since the thread has already
            // been created, the only way to clean up the thread is to call the
            // start() method. Otherwise, the exit method will be never called
            // because the start() was never called. In the run method, the stop
            // method calls right away if the m_failed is set to true.
            m_failed = true;
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

    /**
     * Set the cache to use for searches.
     * @param cache The cache to use for searches; <CODE>null</CODE> for no cache
     */
    synchronized void setCache( LDAPCache cache ) {
        m_cache = cache;
        m_messages = (m_cache != null) ? new Hashtable() : null;
    }
    
    /**
     * Sends LDAP request via this connection thread.
     * @param request request to send
     * @param toNotify response listener to invoke when the response
     *          is ready
     */
    synchronized void sendRequest (LDAPConnection conn, JDAPProtocolOp request,
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
                this.m_requests.put (new Integer (msg.getId()), toNotify);
                /* Notify the backlog checker that there may be another outstanding
                   request */
                resultRetrieved(); 
            }
            toNotify.addRequest(msg.getId(), conn, this);
        }

        synchronized( m_sendRequestLock ) {
            try {
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
                LDAPConstraints cons = conn.getSearchConstraints();
                sendRequest (null, new JDAPUnbindRequest (), null, cons);
                cleanUp();
                if ( m_thread != null ) {
                    m_thread.interrupt();
                }
                this.sleep(100); /* give enough time for threads to shutdown */
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
    private void checkBacklog() {
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
                    if (sl.getMessageCount() >= sl.getConstraints().getMaxBacklog()) {
                        doWait = true;
                    }
                }
                // synch op with non-zero batch size ?
                else if (sl.getConstraints().getBatchSize() != 0) {
                    if (sl.getMessageCount() >= sl.getConstraints().getMaxBacklog()) {
                        doWait = true;
                    }
                }                    
            }
            if ( doWait ) {
                synchronized(this) {
                    try {
                        wait();
                    } catch (InterruptedException e ) {
                    }
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
        if (m_failed) {
            return;
        }

        m_thread = Thread.currentThread();
        LDAPMessage msg = null;
        JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();

        while (true) {
            yield();
            int[] nread = new int[1];
            nread[0] = 0;
            // Avoid too great a backlog of results
            checkBacklog();
            try  {

                if (m_thread.isInterrupted()) {
                    break;
                }
                
                BERElement element = BERElement.getElement(decoder,
                                                           m_serverInput,
                                                           nread);
                msg = LDAPMessage.parseMessage(element);

                // passed in the ber element size to approximate the size of the cache
                // entry, thereby avoiding serialization of the entry stored in the
                // cache
                processResponse (msg, nread[0]);
            } catch (Exception e)  {
                networkError(e);
            }

            if (m_disconnected)
                break;
        }
    }

    /**
     * Allocates a new LDAP message ID.  These are arbitrary numbers used to
     * correlate client requests with server responses.
     * @return new unique msgId
     */
    private synchronized int allocateId () {
        m_highMsgId = (m_highMsgId + 1) % MAXMSGID;
        return m_highMsgId;
    }

    /**
     * When a response arrives from the LDAP server, it is processed by
     * this routine.  It will pass the message on to the listening object
     * associated with the LDAP msgId.
     * @param incoming New message from LDAP server
     */
    private synchronized void processResponse (LDAPMessage incoming, int size) {
        Integer messageID = new Integer (incoming.getId());
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
                int msgid = incoming.getId();
                LDAPConnection ldc = l.getConnection(msgid);
                if (ldc != null) {
                    ldc.setResponseControls( this,
                        new LDAPResponseControl(ldc, msgid, con));
                }                    
            }
        }

        Vector v = null;
        JDAPProtocolOp op = incoming.getProtocolOp ();

        if ((op instanceof JDAPSearchResponse) ||
            (op instanceof JDAPSearchResultReference)) {

            l.addMessage (incoming);
            Long key = ((LDAPSearchListener)l).getKey();

            if ((m_cache != null) && (key != null)) {
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
            }
        } else {
            l.addMessage (incoming);
            if (l instanceof LDAPSearchListener) {
                Long key = ((LDAPSearchListener)l).getKey();

                if (key != null) {
                    boolean fail = false;
                    JDAPProtocolOp protocolOp = incoming.getProtocolOp();
                    if (protocolOp instanceof JDAPSearchResult) {
                        JDAPResult res = (JDAPResult)protocolOp;
                        if (res.getResultCode() > 0) {
                            fail = true;
                        }
                    }

                    if ((!fail) && (m_cache != null))  {

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
            m_requests.remove (messageID);
        }
    }

    /**
     * Stop dispatching responses for a particular message ID.
     * @param id Message ID for which to discard responses.
     */
    synchronized void abandon (int id ) {
        LDAPMessageQueue l = (LDAPMessageQueue)m_requests.remove(new Integer(id));
        if (l != null) {
            l.removeRequest(id);
        }
        notifyAll(); // If LDAPConnThread is blocked in checkBacklog()
    }

    /**
     * Handles network errors.  Basically shuts down the whole connection.
     * @param e The exception which was caught while trying to read from
     * input stream.
     */
    private synchronized void networkError (Exception e) {
        try {

            // notify the Connection Setup Manager that the connection is lost
            m_connMgr.invalidateConnection();
            
            // notify each listener that the server is down.
            Enumeration requests = m_requests.elements();
            while (requests.hasMoreElements()) {
                LDAPMessageQueue listener =
                    (LDAPMessageQueue)requests.nextElement();
                listener.setException(this, new LDAPException("Server down",
                                                        LDAPException.OTHER));
            }
            cleanUp();

        } catch (NullPointerException ee) {
          System.err.println("Exception: "+ee.toString());
        }

        /**
         * Notify all the registered connections.
         * IMPORTANT: This needs to be done last. Otherwise, the socket
         * input stream and output stream might never get closed and the whole
         * task will get stuck in the stop method when we try to stop the
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
    }
}
