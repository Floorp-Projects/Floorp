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
 * server error, or disconnect() if the connection is deliberately terminated
 * by the user.
 */
class LDAPConnSetupMgr implements Cloneable, Serializable {

    static final long serialVersionUID = 1519402748245755306L;
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
        String host;
        int    port;
        int    connSetupStatus;
        Thread connSetupThread;

        ServerEntry(String host, int port, int status) {
            this.host = host;
            this.port = port;
            connSetupStatus = status;
            connSetupThread = null;
        }
    
        public String toString() {
            return "{" +host+":"+port + " status="+connSetupStatus+"}";
        }
    }

    /**
     * Socket to the connected server
     */
    private Socket _socket = null;
    
    /**
     * Last exception occured during connection setup
     */
    private LDAPException _connException = null;

    /**
     * List of server to use for the connection setup
     */
    ServerEntry[] _dsList;
    
    /**
     * Index of the last connected server
     */    
    private int _dsIdx = -1;

    /**
     * Socket factory for SSL connections
     */
    LDAPSocketFactory _factory;
    
    /**
     * Connection setup policy (PARALLEL or SERIAL)
     */
    int _policy = SERIAL;
    
    /**
     * Delay in ms before another connection setup thread is started.    
     */
    int _connSetupDelay = -1;

    /**
     * The maximum time to wait to established the connection
     */
    int _connectTimeout = 0;

    /**
     *  During connection setup, the current count of servers to which
     *  connection attmpt has been made 
     */
    private transient int _attemptCnt = 0;

    /**
     * Connection IDs for ldap trace messages
     */
    private static int _nextId;
    private int _id;
    
    /**
     * Constructor
     * @param host list of host names to which to connect
     * @param port list of port numbers corresponding to the host list
     * @param factory socket factory for SSL connections     
     */
    LDAPConnSetupMgr(String[] hosts, int[] ports, LDAPSocketFactory factory) {
        _dsList = new ServerEntry[hosts.length];
        for (int i=0; i < hosts.length; i++) {
            _dsList[i] = new ServerEntry(hosts[i], ports[i], NEVER_USED);
        }
        _factory = factory;
        _id = _nextId++;
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
    
        if (_connectTimeout == 0) {
            // No need for a separate thread, connect time not limited
            connect();
        }
        else {
        
            // Wait for connection at most _connectTimeout milliseconds
            // Run connection setup in a separate thread to monitor the time
            tmax = System.currentTimeMillis() + _connectTimeout;
            th = new Thread (new Runnable() {
                public void run() {
                    connect();
                }
            }, "ConnSetupMgr");
            th.setDaemon(true);
            th.start();
        
            while  (_socket==null && (_attemptCnt < _dsList.length) &&
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

        if (_socket != null) {
            return _socket;
        }

        if  ( th != null && (tcur = System.currentTimeMillis()) >= tmax) {
            // We have timed out 
            th.interrupt();
            cleanup();
            throw new LDAPException(
                "connect timeout, " + getServerList() + " might be unreachable",
                LDAPException.CONNECT_ERROR);
        }

        if (_connException != null && _dsList.length == 1) {
            throw _connException;
        }

        throw new LDAPException(
            "failed to connect to server " + getServerList(),
            LDAPException.CONNECT_ERROR);
    }

    private void reset() {
        _socket = null;
        _connException = null;
        _attemptCnt = 0;
                
        for (int i=0; i < _dsList.length; i++) {
            _dsList[i].connSetupThread = null;
        }        
    }
    
    private String getServerList() {
        StringBuffer sb = new StringBuffer();
        for (int i=0; i < _dsList.length; i++) {
            sb.append(i==0 ? "" : " ");
            sb.append(_dsList[i].host);
            sb.append(":");
            sb.append(_dsList[i].port);
        }
        return sb.toString();
   }

    private void connect() {
    
        if (_policy == SERIAL || _dsList.length == 1) {
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
        if (_socket != null) {
            _dsList[_dsIdx].connSetupStatus = FAILED;
        
            // Move the entry to the end of the list
            int srvCnt = _dsList.length, j=0;        
            ServerEntry[] newDsList = new ServerEntry[_dsList.length];
            for (int i=0; i < srvCnt; i++) {
                if (i != _dsIdx) {
                    newDsList[j++] = _dsList[i];
                }
            }
            newDsList[j] = _dsList[_dsIdx];
            _dsList = newDsList;
            _dsIdx = j;
        }
    
        _socket = null;
    }
    
    /**
     * Called when the current connection is terminated by the user.
     * Mark the connected server status as DISCONNECTED. This will
     * put it at top of the server list for the next connect attempt.
     */
    void disconnect() {
        if (_socket != null) {
            _dsList[_dsIdx].connSetupStatus =  DISCONNECTED;
        }
    
        _socket = null;
    }

    Socket getSocket() {
        return _socket;
    }
    
    String getHost() {
        if (_dsIdx >= 0) {
            return _dsList[_dsIdx].host;
        }
        return _dsList[0].host;
    }
    
    int getPort() {
        if (_dsIdx >= 0) {
            return _dsList[_dsIdx].port;
        }
        return _dsList[0].port;
    }

    int  getConnSetupDelay() {
        return _connSetupDelay/1000;
    }
    
    /**
     * Selects the connection failover policy
     * @param delay in seconds for the parallel connection setup policy.
     * Possible values are: <br>(delay=-1) use serial policy,<br>
     * (delay=0) start immediately concurrent threads to each specified server
     * <br>(delay>0) create a new connection setup thread after delay seconds
     */
    void setConnSetupDelay(int delay) {
        _policy = (delay < 0) ? SERIAL : PARALLEL;        
        _connSetupDelay = delay*1000;
        
    }
    
    int getConnectTimeout() {
        return _connectTimeout/1000;
    }

    /**
     * Sets the maximum time to spend in the openConnection() call
     * @param timeout in seconds to wait for the connection to be established
     */
    void setConnectTimeout(int timeout) {
        _connectTimeout = timeout*1000;
    }
    
    /**
     * Check if the user has voluntarily closed the connection
     */
    boolean isUserDisconnected() {
        return (_dsIdx >=0 &&
                _dsList[_dsIdx].connSetupStatus == DISCONNECTED);
    }
    
    /**
     * Try  sequentially to open a new connection to a server. 
     */
    private void openSerial() {
        for (int i=0; i < _dsList.length; i++) {
            _dsList[i].connSetupThread = Thread.currentThread();
            connectServer(i);
            if (_socket != null) {
                return;
            }    
        }        
    }
    
    /**
     * Try concurrently to open a new connection a server. Create a separate
     * thread for each connection attempt.
     */
    private synchronized void openParallel() {
        for (int i=0; _socket==null && i < _dsList.length; i++) {
        
            //Create a Thread to execute connectSetver()
            final int dsIdx = i;
            String threadName = "ConnSetupMgr " +
                _dsList[dsIdx].host + ":" + _dsList[dsIdx].port;
            Thread t = new Thread(new Runnable() {
                public void run() {
                    connectServer(dsIdx);
                }
            }, threadName);

            _dsList[dsIdx].connSetupThread = t;
            t.setDaemon(true);
            t.start();
        
            // Wait before starting another thread if the delay is not zero
            if (_connSetupDelay != 0 && i < (_dsList.length-1)) {
                try {
                    wait(_connSetupDelay);
                }
                catch (InterruptedException e) {
                    return;
                }
            }
        }    

        // At this point all threads are started. Wait until first thread
        // succeeds to connect or all threads terminate
    
        while (_socket == null && (_attemptCnt < _dsList.length)) {
        
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
        ServerEntry entry = _dsList[idx];
        Thread currThread = Thread.currentThread();
        Socket sock = null;
        LDAPException conex = null;
    
        try {
            /* If we are to create a socket ourselves, make sure it has
               sufficient privileges to connect to the desired host */
            if (_factory == null) {
                sock = new Socket (entry.host, entry.port);
                //s.setSoLinger( false, -1 );
            } else {
                sock = _factory.createSocket(entry.host, entry.port);
            }
        }
        catch (IOException e) {    
            conex = new LDAPException("failed to connect to server " 
            + entry.host+":"+entry.port, LDAPException.CONNECT_ERROR);
        }
        catch (LDAPException e) {    
            conex = e;
        }
    
        if (currThread.isInterrupted()) {
            return;
        }
        
        synchronized (this) {
            if (_socket == null && entry.connSetupThread == currThread) {
                entry.connSetupThread = null;
                if (sock != null) {
                    entry.connSetupStatus = CONNECTED;
                    _socket = sock;
                    _dsIdx = idx;
                    cleanup(); // Signal other concurrent threads to terminate
                }
                else {
                    entry.connSetupStatus = FAILED;
                    _connException = conex;
                }
                _attemptCnt++;
                notifyAll();
            }    
        }
    }    

    /**
     * Terminate all concurrently running connection setup threads
     */
    private synchronized void cleanup() {
        Thread currThread = Thread.currentThread();
        for (int i=0; i < _dsList.length; i++) {
            ServerEntry entry = _dsList[i];
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
        int srvCnt = _dsList.length;
        for (int i=1; i < srvCnt; i++) {
            for (int j=0; j < i; j++) {
                if (_dsList[i].connSetupStatus < _dsList[j].connSetupStatus) {
                    // swap entries
                    ServerEntry entry = _dsList[j];
                    _dsList[j] = _dsList[i];
                    _dsList[i] = entry;
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
            _socket.close();
            return true;
        }
        catch (Exception e) {
            return false;
        }
    }            

    public String toString() {
        String str = "dsIdx="+_dsIdx+ " dsList=";
        for (int i=0; i < _dsList.length; i++) {
            str += _dsList[i]+ " ";
        }
        return str;
    }    

    int getID() {
        return _id;
    }
    
    String getLDAPUrl() {
        return ((_factory == null) ? "ldap" : "ldaps")  +
               "://" + getHost() + ":" + getPort();
    }
    
    public Object clone() {
        try {
            LDAPConnSetupMgr cloneMgr = (LDAPConnSetupMgr) super.clone();
            cloneMgr._dsList = new ServerEntry[_dsList.length];
            for (int i=0; i<_dsList.length; i++) {
                ServerEntry e = _dsList[i];
                cloneMgr._dsList[i] = new ServerEntry(e.host, e.port, e.connSetupStatus);
            }
            return  cloneMgr;
        }
        catch (CloneNotSupportedException ex) {
            return null;
        }
    }    
}
