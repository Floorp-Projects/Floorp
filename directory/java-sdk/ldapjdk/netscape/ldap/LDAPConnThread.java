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

import java.util.*;
import netscape.ldap.client.*;
import netscape.ldap.client.opers.*;
import netscape.ldap.ber.stream.*;
import netscape.ldap.util.*;
import java.io.*;
import java.net.*;
import java.text.SimpleDateFormat;

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
class LDAPConnThread implements Runnable {

    /**
     * Constants
     */
    private final static int MAXMSGID = Integer.MAX_VALUE;
    private final static int BACKLOG_CHKCNT = 50;

    /**
     * Internal variables
     */
    transient private static int m_highMsgId;
    transient private InputStream m_serverInput, m_origServerInput;
    transient private OutputStream m_serverOutput, m_origServerOutput;
    transient private Hashtable m_requests;
    transient private Hashtable m_messages = null;
    transient private Vector m_registered;   
    transient private LDAPCache m_cache = null;
    transient private Thread m_thread = null;
    transient private Object m_sendRequestLock = new Object();
    transient private LDAPConnSetupMgr m_connMgr = null;
    transient private Object m_traceOutput = null;
    transient private int m_backlogCheckCounter = BACKLOG_CHKCNT;
    transient private boolean m_bound;

    /**
     * Connection IDs for ldap trace messages
     */
    transient private static int m_nextId;
    transient private int m_id;
    

    // Time Stemp format Hour(0-23):Minute:Second.Milliseconds used for trace msgs
    static SimpleDateFormat m_timeFormat = new SimpleDateFormat("HH:mm:ss.SSS");
        
    /**
     * Constructs a connection thread that maintains connection to the
     * LDAP server.
     * @param connMgr the connection setup manager
     * @param cache cache object or null
     * @param traceOutput trace object or null
     */
    public LDAPConnThread(LDAPConnSetupMgr connMgr, LDAPCache cache, Object traceOutput) {
        m_requests = new Hashtable ();
        m_registered = new Vector ();
        m_connMgr = connMgr;
        setCache( cache );
        setTraceOutput(traceOutput);
    }

    synchronized void connect(LDAPConnection ldc) throws LDAPException{

        if (m_thread != null) {
            return;
        }

        try {
            m_connMgr.openConnection();
            m_serverInput  = new BufferedInputStream (m_connMgr.getSocket().getInputStream());
            m_serverOutput = new BufferedOutputStream(m_connMgr.getSocket().getOutputStream());
            register(ldc);
        } catch (IOException e) {
            throw new LDAPException ( "failed to connect to server " +
                  m_connMgr.getHost(), LDAPException.CONNECT_ERROR );
        }

        m_id = m_nextId++;        
        String url = m_connMgr.getLDAPUrl().getServerUrl();

        if (m_traceOutput != null) {
            StringBuffer sb = new StringBuffer(" Connected to ");
            sb.append(url);
            logTraceMessage(sb);
        }

        String threadID = "LDAPConnThread-" + m_id + " " + url;
        m_thread = new Thread(this, threadID);
        m_thread.setDaemon(true);
        m_thread.start();
    }

    public synchronized String toString() {
        if (m_thread != null) {
            return m_thread.getName();
        }
        else {
            return "LDAPConnThread-" + m_id + " <disconnected>";
        }
    }

    /**
     * Layer a SSL socket over the current non-SSL one
     */
    void layerSocket(LDAPTLSSocketFactory factory) throws Exception {
        synchronized (m_sendRequestLock) {
            try {
                m_connMgr.layerSocket(factory);
                setInputStream(m_connMgr.getSocket().getInputStream());
                setOutputStream(m_connMgr.getSocket().getOutputStream());
            }
            catch (Exception e) {
                m_serverInput  = m_origServerInput;
                m_serverOutput = m_origServerOutput;
                throw e;
            }
        }
    }

    void setBound(boolean bound) {
        m_bound = bound;
    }

    boolean isBound() {
        return (m_thread != null) && m_bound;
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

    int getRequestCount() {
        return m_requests.size();
    }

    void setTraceOutput(Object traceOutput) {
        synchronized (m_sendRequestLock) {
            if (traceOutput == null) {
               m_traceOutput = null;
            }
            else if (traceOutput instanceof OutputStream) {
                m_traceOutput = new PrintWriter((OutputStream)traceOutput);
            }
            else if (traceOutput instanceof LDAPTraceWriter) {
                m_traceOutput = traceOutput;
            }
        }            
    }
    
    void logTraceMessage(StringBuffer msg) {

        String timeStamp = m_timeFormat.format(new Date());
        StringBuffer sb = new StringBuffer(timeStamp);
        sb.append(" ldc="); 
        sb.append(m_id);

        synchronized( m_sendRequestLock ) {
            if (m_traceOutput instanceof PrintWriter) {
                PrintWriter traceOutput = (PrintWriter)m_traceOutput;
                traceOutput.print(sb); // header
                traceOutput.println(msg);
                traceOutput.flush();
            }
            else if (m_traceOutput instanceof LDAPTraceWriter) {
                sb.append(msg);
                ((LDAPTraceWriter)m_traceOutput).write(sb.toString());
            }
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

        if (m_thread == null) {
            throw new LDAPException ( "Not connected to a server",
                                       LDAPException.SERVER_DOWN );
        }

        LDAPMessage msg = 
            new LDAPMessage(allocateId(), request, cons.getServerControls());

        if ( toNotify != null ) {
            /* Only worry about toNotify if we expect a response... */
            m_requests.put (new Integer (msg.getMessageID()), toNotify);

            /* Notify the backlog checker that there may be another outstanding
               request */
            resultRetrieved(); 

            toNotify.addRequest(msg.getMessageID(), conn, this, cons.getTimeLimit());
        }

        if (!sendRequest(msg, /*ignoreErrors=*/false)) {
            throw new LDAPException("Server or network error",
                                     LDAPException.SERVER_DOWN);
        }
    }

    private boolean sendRequest (LDAPMessage msg, boolean ignoreErrors) {
        synchronized( m_sendRequestLock ) {
            try {
                if (m_traceOutput != null) {
                    logTraceMessage(msg.toTraceString());
                }                    
                msg.write (m_serverOutput);
                m_serverOutput.flush();
                return true;
            }
            catch (IOException e) {
                if (!ignoreErrors) {
                    networkError(e);
                }
            }
            catch (NullPointerException e) {
                // m_serverOutput is null bacause of network error
                if (!ignoreErrors) {
                    if (m_thread != null) {
                        throw e;
                    }
                }
            }
            return false;
        }
    }

    private void sendUnbindRequest(LDAPControl[] ctrls) {
        LDAPMessage msg = 
            new LDAPMessage(allocateId(), new JDAPUnbindRequest(), ctrls);
        sendRequest(msg, /*ignoreErrors=*/true);
    }
    
    private void sendAbandonRequest(int id, LDAPControl[] ctrls) {
        LDAPMessage msg = 
            new LDAPMessage(allocateId(), new JDAPAbandonRequest(id), ctrls);
        sendRequest(msg, /*ignoreErrors=*/true);
    }

    /**
     * Register with this connection thread.
     * @param conn LDAP connection
     */
    public synchronized void register(LDAPConnection conn) {
        if (!m_registered.contains(conn))
            m_registered.addElement(conn);
    }

    int getClientCount() {
        return m_registered.size();
    }

    boolean isConnected() {
        return m_thread != null;
    }

    /**
     * De-Register with this connection thread. If all the connection
     * is deregistered. Then, this thread should be killed.
     * @param conn LDAP connection
     */
    /**
     * De-Register with this connection thread. If all the connection
     * is deregistered. Then, this thread should be killed.
     * @param conn LDAP connection
     */
    synchronized void deregister(LDAPConnection conn) {

        if (m_thread == null) {
            return;
        }
        
        m_registered.removeElement(conn);
        if (m_registered.size() == 0) {
            
            // No more request processing
            Thread t = m_thread;               
            m_thread = null;

            try {
                // Notify the server
                sendUnbindRequest(conn.getConstraints().getServerControls());
                
                // interrupt the thread
                try {
                    t.interrupt();
                    wait(500);
                }
                catch (InterruptedException ignore) {}
                 
            } catch (Exception e) {
                LDAPConnection.printDebug(e.toString());
            }
            finally {
                cleanUp(null);
            }
        }
    }


    /**
     * Clean up after the thread shutdown.
     * The list of registered clients m_registered is left in the current state
     * to enable the clients to recover the connection.
     */
    private void cleanUp(LDAPException ex) {

        resultRetrieved();

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

        if (m_origServerInput != null) {
            try {
                m_origServerInput.close ();
            } catch (Exception e) {
            } finally {
                m_origServerInput = null;
            }
        }

        if (m_origServerOutput != null) {
            try {
                m_origServerOutput.close ();
            } catch (Exception e) {
            } finally {
                m_origServerOutput = null;
            }
        }

        // Notify the Connection Manager 
        if (ex != null) {
            // the connection is lost
            m_connMgr.invalidateConnection();
        }
        else {
            // the connection is closed
            m_connMgr.closeConnection();
        }

        // Set the status for all outstanding requests
        Enumeration requests = m_requests.elements();
        while (requests.hasMoreElements()) {
            try { 
                LDAPMessageQueue listener =
                    (LDAPMessageQueue)requests.nextElement();
                if (ex != null) {
                    listener.setException(this, ex);
                }
                else {
                    listener.removeAllRequests(this);
                }
            }
            catch (Exception ignore) {}
        }            
        m_requests.clear();

        if (m_messages != null) {
            m_messages.clear();
        }

        m_bound = false;
    }

    /**
     * Sleep if there is a backlog of search results
     */
    private void checkBacklog() throws InterruptedException{

        while (true) {

            if (m_requests.size() == 0) {
                return;
            }
            
            Enumeration listeners = m_requests.elements();
            while( listeners.hasMoreElements() ) {
                LDAPMessageQueue l = (LDAPMessageQueue)listeners.nextElement();

                // If there are clients waiting for a regular response
                // message, skip backlog checking
                if ( !(l instanceof LDAPSearchListener) ) {
                    return;
                }

                LDAPSearchListener sl = (LDAPSearchListener)l;
                
                // should never happen, but just in case
                if (sl.getSearchConstraints() == null) {
                    return;
                }

                int slMaxBacklog = sl.getSearchConstraints().getMaxBacklog();
                int slBatchSize  = sl.getSearchConstraints().getBatchSize();
                
                // Disabled backlog check ?
                if (slMaxBacklog == 0) {
                    return;
                }
                
                // Synch op with zero batch size ?
                if (!sl.isAsynchOp() && slBatchSize == 0) {
                    return;
                }
                
                // Max backlog not reached for at least one listener ?
                // (if multiple requests are in progress)
                if (sl.getMessageCount() < slMaxBacklog) {
                    return;
                }                    
            }
                                   
            synchronized(this) {
                wait(3000);
            }
        }
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

        LDAPMessage msg = null;
        JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();
        int[] nread = new int[1];        

        while (Thread.currentThread() == m_thread) {
            try  {

                // Check every BACKLOG_CHKCNT messages if the backlog is not too high
                if (--m_backlogCheckCounter <= 0) {
                    m_backlogCheckCounter = BACKLOG_CHKCNT;
                    checkBacklog();
                }

                nread[0] = 0;                
                BERElement element = BERElement.getElement(decoder,
                                                           m_serverInput,
                                                           nread);
                msg = LDAPMessage.parseMessage(element);

                if (m_traceOutput != null) {
                    logTraceMessage(msg.toTraceString());
                }                    

                // passed in the ber element size to approximate the size of the cache
                // entry, thereby avoiding serialization of the entry stored in the
                // cache
                processResponse (msg, nread[0]);
                Thread.yield();                

            } catch (Exception e)  {
                if (Thread.currentThread() == m_thread) {
                    networkError(e);
                }
                else {
                    resultRetrieved();
                }
            }
        }
    }

    /**
     * When a response arrives from the LDAP server, it is processed by
     * this routine.  It will pass the message on to the listening object
     * associated with the LDAP msgId.
     * @param msg New message from LDAP server
     */
    private void processResponse (LDAPMessage msg, int size) {
        Integer messageID = new Integer (msg.getMessageID());
        LDAPMessageQueue l = (LDAPMessageQueue)m_requests.get (messageID);
        if (l == null) {
            return; /* nobody is waiting for this response (!) */
        }

        if (m_cache != null && (l instanceof LDAPSearchListener)) {
            cacheSearchResult((LDAPSearchListener)l, msg, size);
        }            
        
        l.addMessage (msg);

        if (msg instanceof LDAPResponse) { 
            m_requests.remove (messageID);
            if (m_requests.size() == 0) {
                m_backlogCheckCounter = BACKLOG_CHKCNT;
            }

            // Change IO streams if startTLS extended op completed
            if (msg instanceof LDAPExtendedResponse) {
                LDAPExtendedResponse extrsp = (LDAPExtendedResponse) msg;
                String extid = extrsp.getID();
                if (extrsp.getResultCode() == 0 && extid != null &&
                    extid.equals(LDAPConnection.OID_startTLS)) {
                       
                    changeIOStreams();
                }
            }
        }        
    }

    private void changeIOStreams() {

        // Note: For the startTLS, the new streams are layered over the
        // existing ones so current IO streams as well as the socket MUST
        // not be closed.
        m_origServerInput  = m_serverInput;
        m_origServerOutput = m_serverOutput;
        m_serverInput  = null;
        m_serverOutput = null;
        
        while (m_serverInput == null ||  m_serverOutput == null) {
            try {
                if (Thread.currentThread() != m_thread) {
                    return; // user disconnected
                }
                Thread.sleep(200);
            } catch (InterruptedException ignore) {}
        }
    }

    
    /**
     * Collect search results to be added to the LDAPCache. Search results are
     * packaged in a vector and temporary stored into a hashtable m_messages
     * using the message id as the key. The vector first element (at index 0)
     * is a Long integer representing the total size of all LDAPEntries entries.
     * It is followed by the actual LDAPEntries.
     * If the total size of entries exceeds the LDAPCache max size, or a referral
     * has been received, caching of search results is disabled and the entry is 
     * not added to the LDAPCache. A disabled search request is denoted by setting
     * the entry size to -1.
     */
    private synchronized void cacheSearchResult (LDAPSearchListener l, LDAPMessage msg, int size) {
        Integer messageID = new Integer (msg.getMessageID());
        Long key = l.getKey();
        Vector v = null;

        if ((m_cache == null) || (key == null)) {
            return;
        }
        
        if (msg instanceof LDAPSearchResult) {

            // get the vector containing the LDAPMessages for the specified messageID
            v = (Vector)m_messages.get(messageID);
            if (v == null) {
                m_messages.put(messageID, v = new Vector());
                v.addElement(new Long(0));
            }

            // Return if the entry size is -1, i.e. the caching is disabled
            if (((Long)v.firstElement()).longValue() == -1L) {
                return;
            }
            
            // add the size of the current LDAPMessage to the lump sum
            // assume the size of LDAPMessage is more or less the same as the size
            // of LDAPEntry. Eventually LDAPEntry object gets stored in the cache
            // instead of LDAPMessage object.
            long entrySize = ((Long)v.firstElement()).longValue() + size;

            // If the entrySize exceeds the cache size, discard the collected
            // entries and disble collecting of entries for this search request
            // by setting the entry size to -1.
            if (entrySize > m_cache.getSize()) {
                v.removeAllElements();
                v.addElement(new Long(-1L));
                return;
            }                
                
            // update the lump sum located in the first element of the vector
            v.setElementAt(new Long(entrySize), 0);

            // convert LDAPMessage object into LDAPEntry which is stored to the
            // end of the Vector
            v.addElement(((LDAPSearchResult)msg).getEntry());

        } else if (msg instanceof LDAPSearchResultReference) {

            // If a search reference is received disable caching of
            // this search request 
            v = (Vector)m_messages.get(messageID);
            if (v == null) {
                m_messages.put(messageID, v = new Vector());
            }
            else {
                v.removeAllElements();
            }
            v.addElement(new Long(-1L));

        } else if (msg instanceof LDAPResponse) {

            // The search request has completed. Store the cache entry
            // in the LDAPCache if the operation has succeded and caching
            // is not disabled due to the entry size or referrals
            
            boolean fail = ((LDAPResponse)msg).getResultCode() > 0;
            v = (Vector)m_messages.remove(messageID);
            
            if (!fail)  {
                // If v is null, meaning there are no search results from the
                // server
                if (v == null) {
                    v = new Vector();
                    v.addElement(new Long(0));
                }

                // add the new entry if the entry size is not -1 (caching diabled)
                if (((Long)v.firstElement()).longValue() != -1L) {
                    m_cache.addEntry(key, v);
                }
            }
        }
    }

    /**
     * Stop dispatching responses for a particular message ID and send 
     * the abandon request.
     * @param id Message ID for which to discard responses.
     */
    void abandon (int id, LDAPControl[] ctrls) {
        
        if (m_thread == null) {
            return;
        }
        
        LDAPMessageQueue l = (LDAPMessageQueue)m_requests.remove(new Integer(id));
        // Clean up cache if enabled
        if (m_messages != null) {
            m_messages.remove(new Integer(id));
        }
        if (l != null) {
            l.removeRequest(id);
        }
        resultRetrieved(); // If LDAPConnThread is blocked in checkBacklog()
        
        sendAbandonRequest(id, ctrls);
    }

    /**
     * Change listener for a message ID. Required when LDAPMessageQueue.merge()
     * is invoked.
     * @param id Message ID for which to chanage the listener.
     * @return Previous listener.
     */
    LDAPMessageQueue changeListener (int id, LDAPMessageQueue toNotify) {

        if (m_thread == null) {
            toNotify.setException(this, new LDAPException("Server or network error",
                                                           LDAPException.SERVER_DOWN));
            return null;
        }
        return (LDAPMessageQueue) m_requests.put (new Integer (id), toNotify);
    }

    /**
     * Handles network errors.  Basically shuts down the whole connection.
     * @param e The exception which was caught while trying to read from
     * input stream.
     */
    private synchronized void networkError (Exception e) {

        if (m_thread == null) {
            return;
        }
        m_thread = null; // No more request processing
        cleanUp(new LDAPException("Server or network error",
                                  LDAPException.SERVER_DOWN));
    }
}