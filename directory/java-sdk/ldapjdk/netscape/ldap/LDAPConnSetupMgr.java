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
import java.io.*;
import java.net.*;

/**
 * Makes a connection to a server from a list using "smart" failover.
 * Connection attempts can be made serially from the same thread, or
 * in parallel by creating a separate thread after the specified delay.
 * Connection setup status is preserved for later attempts, so that servers
 * that are more likely to be available will be tried first.
 * <P>
 * The total time spent opening a connection can be limited with the
 * <CODE>ConnectTimeout</CODE> property.
 * <P>
 * When a connection is successfully created, a socket is opened. The socket 
 * is passed to the LDAPConnThread. The LDAPConnThread must call
 * invalidateConnection() if the connection is lost due to a network or 
 * server error, or closeConnection() if the connection is deliberately terminated
 * by the user.
 */
class LDAPConnSetupMgr implements java.io.Serializable {

    static final long serialVersionUID = 1519402748245755307L;
    /**
     * Policy for opening a connection when multiple servers are used
     */
    private static final int SERIAL = 0;
    private static final int PARALLEL   = 1;
    
    /**
     * ServerEntry.connSetupStatus possible value. The values also represent
     * the likelihood that the connection will be setup to a server. Lower
     * values have higher priority. See sortDsList() method
     */
    private static final int CONNECTED    = 0;  
    private static final int DISCONNECTED = 1;
    private static final int NEVER_USED   = 2;
    private static final int INTERRUPTED  = 3;
    private static final int FAILED       = 4;
    
    /**
     * Representation for a server in the server list.
     */
    class ServerEntry {
        LDAPUrl url;
        int    connSetupStatus;
        Thread connSetupThread;

        ServerEntry(LDAPUrl url, int status) {
            this.url = url;
            connSetupStatus = status;
            connSetupThread = null;
        }    
        public String toString() {
            return "{" + url + " status="+connSetupStatus+"}";
        }
    }
    

    /**
     * Socket to the connected server
     */
    private Socket m_socket = null;
    
    /**
     * Original, underlying socket to the server, see layerSocket()
     */
    private Socket m_origSocket = null;
    

    /**
     * Last exception occured during connection setup
     */
    private LDAPException m_connException = null;

    /**
     * List of server to use for the connection setup
     */
    ServerEntry[] m_dsList;
    
    /**
     * Index of the last connected server
     */    
    private int m_dsIdx = -1;

    /**
     * Socket factory for SSL connections
     */
    LDAPSocketFactory m_factory;
    
    /**
     * Connection setup policy (PARALLEL or SERIAL)
     */
    int m_policy = SERIAL;
    
    /**
     * Delay in ms before another connection setup thread is started.    
     */
    int m_connSetupDelay = -1;

    /**
     * The maximum time to wait to established the connection
     */
    int m_connectTimeout = 0;

    /**
     *  During connection setup, the current count of servers to which
     *  connection attmpt has been made 
     */
    private transient int m_attemptCnt = 0;

    /**
     * Constructor
     * @param host list of host names to which to connect
     * @param port list of port numbers corresponding to the host list
     * @param factory socket factory for SSL connections     
     */
    LDAPConnSetupMgr(String[] hosts, int[] ports, LDAPSocketFactory factory)  throws LDAPException{
        m_dsList = new ServerEntry[hosts.length];
        boolean secure = (factory != null);
        for (int i=0; i < hosts.length; i++) {
            String url = secure ? "ldaps://" : "ldap://";            
            url += hosts[i] + ":" + ports[i];
            try { 
                m_dsList[i] = new ServerEntry(new LDAPUrl(url), NEVER_USED);
            }
            catch (MalformedURLException ex) {
                throw new LDAPException("Invalid host:port " + hosts[i]+":"+ports[i],
                                         LDAPException.PARAM_ERROR);                
            }
        }
        m_factory = factory;
    }

    LDAPConnSetupMgr(String[] urls, LDAPSocketFactory factory) throws LDAPException{
        m_dsList = new ServerEntry[urls.length];
        for (int i=0; i < urls.length; i++) {
            try {
                LDAPUrl url = new LDAPUrl(urls[i]);
                m_dsList[i] = new ServerEntry(url, NEVER_USED);
            }
            catch (MalformedURLException ex) {
                throw new LDAPException("Malformed LDAP URL " + urls[i],
                                         LDAPException.PARAM_ERROR);
            }
        }
        m_factory = factory;
    }

    LDAPConnSetupMgr(LDAPUrl[] urls, LDAPSocketFactory factory) throws LDAPException{
        m_dsList = new ServerEntry[urls.length];
        for (int i=0; i < urls.length; i++) {
            m_dsList[i] = new ServerEntry(urls[i], NEVER_USED);
        }
        m_factory = factory;
    }

    /**
     * Try to open the connection to any of the servers in the list, limiting
     * the time waiting for the connection to be established
     * @return connection socket
    */
    synchronized Socket openConnection() throws LDAPException{
        
        long tcur=0, tmax = Long.MAX_VALUE;
        Thread th = null; 
        
        reset();
        
       // If reconnecting, sort dsList so that servers more likly to
        // be available are tried first
        sortDsList();
    
        if (m_connectTimeout == 0) {
            // No need for a separate thread, connect time not limited
            connect();
        }
        else {
        
            // Wait for connection at most m_connectTimeout milliseconds
            // Run connection setup in a separate thread to monitor the time
            tmax = System.currentTimeMillis() + m_connectTimeout;
            th = new Thread (new Runnable() {
                public void run() {
                    connect();
                }
            }, "ConnSetupMgr");
            th.setDaemon(true);
            th.start();
        
            while  (m_socket==null && (m_attemptCnt < m_dsList.length) &&
                   (tcur = System.currentTimeMillis()) < tmax) {
                try {
                    wait(tmax - tcur);
                }
                catch (InterruptedException e) {
                    th.interrupt();
                    cleanup();
                    throw new LDAPInterruptedException("Interrupted connect operation");
                }
            }
        }

        if (m_socket != null) {
            return m_socket;
        }

        if  ( th != null && (tcur = System.currentTimeMillis()) >= tmax) {
            // We have timed out 
            th.interrupt();
            cleanup();
            throw new LDAPException(
                "Connect timeout, " + getServerList() + " might be unreachable",
                LDAPException.CONNECT_ERROR);
        }

        if (m_connException != null && m_dsList.length == 1) {
            throw m_connException;
        }

        throw new LDAPException(
            "Failed to connect to server " + getServerList(),
            LDAPException.CONNECT_ERROR);
    }

    private void reset() {
        m_socket = null;
        m_origSocket = null;
        m_connException = null;
        m_attemptCnt = 0;
                
        for (int i=0; i < m_dsList.length; i++) {
            m_dsList[i].connSetupThread = null;
        }        
    }
    
    private String getServerList() {
        StringBuffer sb = new StringBuffer();
        for (int i=0; i < m_dsList.length; i++) {
            sb.append(i==0 ? "" : " ");
            sb.append(m_dsList[i].url.getHost());
            sb.append(":");
            sb.append(m_dsList[i].url.getPort());
        }
        return sb.toString();
   }

    private void connect() {
    
        if (m_policy == SERIAL || m_dsList.length == 1) {
            openSerial();
        }
        else {
            openParallel();
        }    
    }

    /**
     * Called when the current connection is lost.
     * Put the connected server at the end of the server list for
     * the next connect attempt.    
     */
    synchronized void invalidateConnection() {
        if (m_socket != null) {
            m_dsList[m_dsIdx].connSetupStatus = FAILED;
        
            // Move the entry to the end of the list
            int srvCnt = m_dsList.length, j=0;        
            ServerEntry[] newDsList = new ServerEntry[m_dsList.length];
            for (int i=0; i < srvCnt; i++) {
                if (i != m_dsIdx) {
                    newDsList[j++] = m_dsList[i];
                }
            }
            newDsList[j] = m_dsList[m_dsIdx];
            m_dsList = newDsList;
            m_dsIdx = j;
            
            try {
                m_socket.close();
            } catch (Exception e) {
            } finally {
                m_socket = null;
            }
            
        }

        if (m_origSocket != null) {

            try {
                m_origSocket.close();
            } catch (Exception e) {
            } finally {
                m_origSocket = null;
            }
        }        
    }
    
    /**
     * Called when the current connection is terminated by the user.
     * Mark the connected server status as DISCONNECTED. This will
     * put it at top of the server list for the next connect attempt.
     */
    void closeConnection() {
        if (m_socket != null) {

            m_dsList[m_dsIdx].connSetupStatus =  DISCONNECTED;
            
            try {
                m_socket.close();
            } catch (Exception e) {
            } finally {
                m_socket = null;
            }
        }

        if (m_origSocket != null) {

            try {
                m_origSocket.close();
            } catch (Exception e) {
            } finally {
                m_origSocket = null;
            }
        }
    }

    Socket getSocket() {
        return m_socket;
    }

    /**
     * Layer a new socket over the existing one (used by startTLS)
     */
    void layerSocket(LDAPTLSSocketFactory factory) throws LDAPException{
        Socket s = factory.makeSocket(m_socket);
        m_origSocket = m_socket;
        m_socket = s;
    }
    
    String getHost() {
        if (m_dsIdx >= 0) {
            return m_dsList[m_dsIdx].url.getHost();
        }
        return m_dsList[0].url.getHost();
    }
    
    int getPort() {
        if (m_dsIdx >= 0) {
            return m_dsList[m_dsIdx].url.getPort();
        }
        return m_dsList[0].url.getPort();
    }

    boolean isSecure() {
        if (m_dsIdx >= 0) {
            return m_dsList[m_dsIdx].url.isSecure();
        }
        return m_dsList[0].url.isSecure();
    }

    LDAPUrl getLDAPUrl() {
        if (m_dsIdx >= 0) {
            return m_dsList[m_dsIdx].url;
        }
        return m_dsList[0].url;
    }

    int  getConnSetupDelay() {
        return m_connSetupDelay/1000;
    }
    
    /**
     * Selects the connection failover policy
     * @param delay in seconds for the parallel connection setup policy.
     * Possible values are: <br>(delay=-1) use serial policy,<br>
     * (delay=0) start immediately concurrent threads to each specified server
     * <br>(delay>0) create a new connection setup thread after delay seconds
     */
    void setConnSetupDelay(int delay) {
        m_policy = (delay < 0) ? SERIAL : PARALLEL;        
        m_connSetupDelay = delay*1000;
        
    }
    
    int getConnectTimeout() {
        return m_connectTimeout/1000;
    }

    /**
     * Sets the maximum time to spend in the openConnection() call
     * @param timeout in seconds to wait for the connection to be established
     */
    void setConnectTimeout(int timeout) {
        m_connectTimeout = timeout*1000;
    }
    
    /**
     * Check if the user has voluntarily closed the connection
     */
    boolean isUserDisconnected() {
        return (m_dsIdx >=0 &&
                m_dsList[m_dsIdx].connSetupStatus == DISCONNECTED);
    }
    
    /**
     * Try  sequentially to open a new connection to a server. 
     */
    private void openSerial() {
        for (int i=0; i < m_dsList.length; i++) {
            m_dsList[i].connSetupThread = Thread.currentThread();
            connectServer(i);
            if (m_socket != null) {
                return;
            }    
        }        
    }
    
    /**
     * Try concurrently to open a new connection a server. Create a separate
     * thread for each connection attempt.
     */
    private synchronized void openParallel() {
        for (int i=0; m_socket==null && i < m_dsList.length; i++) {
        
            //Create a Thread to execute connectSetver()
            final int dsIdx = i;
            String threadName = "ConnSetupMgr " + m_dsList[dsIdx].url;
            Thread t = new Thread(new Runnable() {
                public void run() {
                    connectServer(dsIdx);
                }
            }, threadName);

            m_dsList[dsIdx].connSetupThread = t;
            t.setDaemon(true);
            t.start();
        
            // Wait before starting another thread if the delay is not zero
            if (m_connSetupDelay != 0 && i < (m_dsList.length-1)) {
                try {
                    wait(m_connSetupDelay);
                }
                catch (InterruptedException e) {
                    return;
                }
            }
        }    

        // At this point all threads are started. Wait until first thread
        // succeeds to connect or all threads terminate
    
        while (m_socket == null && (m_attemptCnt < m_dsList.length)) {
        
            // Wait for a thread to terminate
            try {
                wait();
            }
            catch (InterruptedException e) {}
        }
    }

    /**
     * Connect to the server at the given index
     */    
    void connectServer(int idx) {
        ServerEntry entry = m_dsList[idx];
        Thread currThread = Thread.currentThread();
        Socket sock = null;
        LDAPException conex = null;

        try {
            /* If we are to create a socket ourselves, make sure it has
               sufficient privileges to connect to the desired host */
            if (!entry.url.isSecure()) {
                sock = new Socket (entry.url.getHost(), entry.url.getPort());
            } else {
                LDAPSocketFactory factory = m_factory;
                if (factory == null) {
                    factory = entry.url.getSocketFactory();
                }
                if (factory == null) {
                    throw new LDAPException("Can not connect, no socket factory " + entry.url,
                                            LDAPException.OTHER);
                }
                sock = factory.makeSocket(entry.url.getHost(), entry.url.getPort());
            }

            sock.setTcpNoDelay( true );
        }
        catch (IOException e) {    
            conex = new LDAPException("failed to connect to server " 
            + entry.url, LDAPException.CONNECT_ERROR);
        }
        catch (LDAPException e) {    
            conex = e;
        }
    
        if (currThread.isInterrupted()) {
            return;
        }
        
        synchronized (this) {
            if (m_socket == null && entry.connSetupThread == currThread) {
                entry.connSetupThread = null;
                if (sock != null) {
                    entry.connSetupStatus = CONNECTED;
                    m_socket = sock;
                    m_dsIdx = idx;
                    cleanup(); // Signal other concurrent threads to terminate
                }
                else {
                    entry.connSetupStatus = FAILED;
                    m_connException = conex;
                }
                m_attemptCnt++;
                notifyAll();
            }    
        }
    }    

    /**
     * Terminate all concurrently running connection setup threads
     */
    private synchronized void cleanup() {
        Thread currThread = Thread.currentThread();
        for (int i=0; i < m_dsList.length; i++) {
            ServerEntry entry = m_dsList[i];
            if (entry.connSetupThread != null && entry.connSetupThread != currThread) {
            
                entry.connSetupStatus = INTERRUPTED;
                //Thread.stop() is considered to be dangerous, use Thread.interrupt().
                //interrupt() will however not work if the thread is blocked in the
                //socket library native connect() call, but the connect() will
                //eventually timeout and the thread will die.
                entry.connSetupThread.interrupt();
                
                entry.connSetupThread = null;                
            }
        }
    }    


    /**
     * Sorts Server List so that servers which are more likely to be available
     * are tried first. The likelihood of making a successful connection
     * is determined by the connSetupStatus. Lower values have higher
     * likelihood. Thus, the order of server access is (1) disconnected by
     * the user (2) never used (3) interrupted connection attempt
    *  (4) connection setup failed/connection lost
     */
    private void sortDsList() {
        int srvCnt = m_dsList.length;
        for (int i=1; i < srvCnt; i++) {
            for (int j=0; j < i; j++) {
                if (m_dsList[i].connSetupStatus < m_dsList[j].connSetupStatus) {
                    // swap entries
                    ServerEntry entry = m_dsList[j];
                    m_dsList[j] = m_dsList[i];
                    m_dsList[i] = entry;
                }
            }
        }
    }    

    /**
     * This is used only by the ldapjdk test libaray to simulate a
     *  server problem and to test fail-over and rebind
     * @return A flag whether the connection was closed
     */
    boolean breakConnection() {
        try {                
            m_socket.close();
            return true;
        }
        catch (Exception e) {
            return false;
        }
    }            

    public String toString() {
        String str = "dsIdx="+m_dsIdx+ " dsList=";
        for (int i=0; i < m_dsList.length; i++) {
            str += m_dsList[i]+ " ";
        }
        return str;
    }
}
