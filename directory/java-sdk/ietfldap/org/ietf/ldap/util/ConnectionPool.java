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
package org.ietf.ldap.util;

import java.util.*;
import org.ietf.ldap.*;

/**
 * Class to maintain a pool of individual connections to the
 * same server. Specify the initial size and the max size
 * when constructing a pool. Call getConnection() to obtain
 * a connection from the pool and close() to return it. If
 * the pool is fully extended and there are no free connections,
 * getConnection() blocks until a connection has been returned
 * to the pool.<BR>
 * Call destroy() to release all connections.
 *<BR><BR>Example:<BR>
 *<PRE>
 * ConnectionPool pool = null;
 * try {
 *     pool = new ConnectionPool( 10, 30,
 *                                "foo.acme.com",389,
 *                                "uid=me, o=acme.com",
 *                                "password" );
 * } catch ( LDAPException e ) {
 *    System.err.println( "Unable to create connection pool" );
 *    System.exit( 1 );
 * }
 * while ( clientsKnocking ) {
 *     String filter = getSearchFilter();
 *     LDAPConnection ld = pool.getConnection();
 *     try {
 *         LDAPSearchResults res = ld.search( BASE, ld.SCOPE_SUB,
 *                                            filter, attrs,
 *                                            false );
 *         pool.close( ld );
 *         while( res.hasMoreElements() ) {
 *             ...
 *</PRE>
 */

/**
 * Connection pool, typically used by a server to avoid creating
 * a new connection for each client
 *
 * @version 1.0
 **/
public class ConnectionPool {
 
    /**
     * Constructor for specifying all parameters
     *
     * @param min initial number of connections
     * @param max maximum number of connections
     * @param host hostname of LDAP server
     * @param port port number of LDAP server
     * @param authdn DN to authenticate as
     * @param authpw password for authentication
     * @exception LDAPException on failure to create connections
     */
    public ConnectionPool( int min, int max,
                           String host, int port,
                           String authdn, byte[] authpw )
        throws LDAPException {
        this( min, max, host, port, authdn, authpw, null );
    }

    /**
     * Constructor for specifying all parameters, anonymous
     * identity
     *
     * @param min initial number of connections
     * @param max maximum number of connections
     * @param host hostname of LDAP server
     * @param port port number of LDAP server
     * @exception LDAPException on failure to create connections
     */
    public ConnectionPool( int min, int max,
                           String host, int port )
        throws LDAPException {
        this( min, max, host, port, "", new byte[0]); 
    }

    /**
     * Constructor for using default parameters, anonymous identity
     *
     * @param host hostname of LDAP server
     * @param port port number of LDAP server
     * @exception LDAPException on failure to create connections
     */
    public ConnectionPool( String host, int port ) 
        throws LDAPException {
        // poolsize=10,max=20,host,port,
        // noauth,nopswd
        this( 10, 20, host, port, "", new byte[0] );
    }

    /* 
     * Constructor for using an existing connection to clone
     * from
     * 
     * @param min initial number of connections
     * @param max maximum number of connections
     * @param ldc connection to clone 
     * @param authpwd password
     * @exception LDAPException on failure to create connections 
     */ 
    public ConnectionPool( int min, int max,
                           LDAPConnection ldc, byte[] authpwd )
        throws LDAPException {
        this( min, max, ldc.getHost(), ldc.getPort(),
              ldc.getAuthenticationDN(), authpwd,
              ldc );
    } 

    /* 
     * Constructor for using an existing connection to clone
     * from
     * 
     * @param min initial number of connections
     * @param max maximum number of connections
     * @param host hostname of LDAP server
     * @param port port number of LDAP server
     * @param authdn DN to authenticate as
     * @param authpw password for authentication
     * @param ldc connection to clone 
     * @exception LDAPException on failure to create connections 
     */ 
    private ConnectionPool( int min, int max,
                            String host, int port,
                            String authdn, byte[] authpw,
                            LDAPConnection ldc )
        throws LDAPException {
        this.poolSize = min;
        this.poolMax  = max;
        this.host = host;
        this.port = port;
        this.authdn = authdn;
        this.authpw = authpw;
        this.ldc = ldc;
        this.debugMode = false;
        createPool();
    }

    /**
     * Destroy the whole pool - called during a shutdown
     */
    public void destroy() {
        for ( int i = 0; i < pool.size(); i++ ) {
            disconnect(
                (LDAPConnectionObject)pool.elementAt(i) );
        }
        pool.removeAllElements();
    }

    /**
     * Gets a connection from the pool
     *
     * If no connections are available, the pool will be
     * extended if the number of connections is less than
     * the maximum; if the pool cannot be extended, the method
     * blocks until a free connection becomes available.
     *
     * @return an active connection.
     */
    public LDAPConnection getConnection() {
        LDAPConnection con;

        while( (con = getConnFromPool()) == null ) {
            synchronized( pool ) {
                try {
                    pool.wait();
                } catch ( InterruptedException e ) {
                }
            }
        }
        return con;
    }

    /**
     * Gets a connection from the pool within a time limit.
     *
     * If no connections are available, the pool will be
     * extended if the number of connections is less than
     * the maximum; if the pool cannot be extended, the method
     * blocks until a free connection becomes available or the
     * time limit is exceeded. 
     *
     * @param timeout timeout in milliseconds
     * @return an active connection or <CODE>null</CODE> if timed out. 
     */
    public LDAPConnection getConnection(int timeout) {
        LDAPConnection con;

        while( (con = getConnFromPool()) == null ) {
            long t1, t0 = System.currentTimeMillis();

            if (timeout <= 0) {
                return con;
            }

            synchronized( pool ) {
                try {
                    pool.wait(timeout);
                } catch ( InterruptedException e ) {
                    return null;
                }
            }

            t1 = System.currentTimeMillis();
            timeout -= (t1 - t0);
        }
        return con;
    }

    /**
     * Gets a connection from the pool
     *
     * If no connections are available, the pool will be
     * extended if the number of connections is less than
     * the maximum; if the pool cannot be extended, the method
     * returns null.
     *
     * @return an active connection or null.
     */
    protected synchronized LDAPConnection getConnFromPool() {
        LDAPConnection con = null;
        LDAPConnectionObject ldapconnobj = null;

        int pSize = pool.size();

        // Get an available connection
        for ( int i = 0; i < pSize; i++ ) {

            // Get the ConnectionObject from the pool
            LDAPConnectionObject co = 
                (LDAPConnectionObject)pool.elementAt(i);

            if ( co.isAvailable() ) {  // Conn available?
                ldapconnobj = co;
                break;
            }
        }

        if ( ldapconnobj == null ) {
            // If there there were no conns in pool, can we grow
            // the pool?
            if ( (poolMax < 0) ||
                 ( (poolMax > 0) &&
                   (pSize < poolMax)) ) {

                // Yes we can grow it
                int i = addConnection();

                // If a new connection was created, use it
                if ( i >= 0 ) {
                    ldapconnobj = 
                        (LDAPConnectionObject)pool.elementAt(i);
                }
            } else {
                debug("All pool connections in use");
            }
        }

        if ( ldapconnobj != null ) {
            ldapconnobj.setInUse( true );  // Mark as in use
            con = ldapconnobj.getLDAPConn();
        }
        return con;
    }

    /**
     * This is our soft close - all we do is mark
     * the connection as available for others to use.
     *
     * @param ld a connection to return to the pool
     */
    public synchronized void close( LDAPConnection ld ) {

        int index = find( ld );
        if ( index != -1 ) {
            LDAPConnectionObject co = 
                (LDAPConnectionObject)pool.elementAt(index);
            co.setInUse( false );  // Mark as available
            synchronized( pool ) {
                pool.notifyAll();
            }
        }
    }
  
    /**
     * Debug method to print the contents of the pool
     */
    public void printPool(){
        System.out.println("--ConnectionPool--");
        for ( int i = 0; i < pool.size(); i++ ) {
            LDAPConnectionObject co =
                (LDAPConnectionObject)pool.elementAt(i);
            System.out.println( "" + i + "=" + co );
        }
    }

    private void disconnect(
        LDAPConnectionObject ldapconnObject ) {
        if ( ldapconnObject != null ) {
            if (ldapconnObject.isAvailable()) {  
                LDAPConnection ld = ldapconnObject.getLDAPConn();
                if ( (ld != null) && (ld.isConnected()) ) {
                    try {
                        ld.disconnect();
                    } catch (LDAPException e) {
                        debug("disconnect: "+e.toString());
                    }
                 }
                 ldapconnObject.setLDAPConn(null); // Clear conn
            }
        }
    }
 
    private void createPool() throws LDAPException {
        // Called by the constructors
        if ( poolSize <= 0 ) {
            throw new LDAPException("ConnectionPoolSize invalid");
        }
        if ( poolMax < poolSize ) {
            debug("ConnectionPoolMax is invalid, set to " +
                  poolSize);
            poolMax = poolSize;
        }

        debug("****Initializing LDAP Pool****");
        debug("LDAP host = "+host+" on port "+port);
        debug("Number of connections="+poolSize);
        debug("Maximum number of connections="+poolMax);
        debug("******");

        pool = new java.util.Vector(); // Create pool vector
        setUpPool( poolSize ); // Initialize it
    }

    private int addConnection() {
        int index = -1;

        debug("adding a connection to pool...");
        try {
            int size = pool.size() + 1; // Add one connection
            setUpPool( size );

            if ( size == pool.size() ) {
                // New size is size requested?
                index = size - 1;
            }
        } catch (Exception ex) {
            debug("Adding a connection: "+ex.toString());
        }
        return index;
    }
  
    private synchronized void setUpPool( int size )
        throws LDAPException {
        // Loop on creating connections
        while( pool.size() < size ) {
            LDAPConnectionObject co =
                new LDAPConnectionObject();
            // Make LDAP connection, using template if available
            LDAPConnection newConn =
                (ldc != null) ? (LDAPConnection)ldc.clone() :
                new LDAPConnection();
            co.setLDAPConn(newConn);
            try {
                if ( newConn.isConnected() ) {
                    // If using a template, then reconnect
                    // to create a separate physical connection
                    newConn.reconnect();
                } else {
                    // Not using a template, so connect with
                    // simple authentication
                    newConn.connect( host, port );
                    newConn.bind( 3, authdn, authpw );
                }
            } catch ( LDAPException le ) {
                debug("Creating pool:"+le.toString());
                debug("aborting....");
                throw le;
            }
            co.setInUse( false ); // Mark not in use
            pool.addElement( co );
        }
    }

    private int find( LDAPConnection con ) {
        // Find the matching Connection in the pool
        if ( con != null ) {
            for ( int i = 0; i < pool.size(); i++ ) {
                LDAPConnectionObject co = 
                    (LDAPConnectionObject)pool.elementAt(i);
                if ( co.getLDAPConn() == con ) {
                    return i;
                }
            }
        }
        return -1;
    }

    /**
      * Sets the debug printout mode.
      *
      * @param mode debug mode to use
      */
    public synchronized void setDebug( boolean mode ) {
        debugMode = mode;
    }

    /**
      * Reports the debug printout mode.
      *
      * @return debug mode in use.
      */
    public boolean getDebug() {
        return debugMode;
    }

    private void debug( String s ) {
        if ( debugMode )
            System.out.println("ConnectionPool ("+
                               new Date()+") : " + s);
    }

    private void debug(String s, boolean severe) {
        if ( debugMode || severe ) {
            System.out.println("ConnectionPool ("+
                               new Date()+") : " + s);
        }
    }

    /**
     * Wrapper for LDAPConnection object in pool
     */
    class LDAPConnectionObject{

        /**
         * Returns the associated LDAPConnection.
         *
         * @return the LDAPConnection.
         * 
         */
        LDAPConnection getLDAPConn() {
            return this.ld;
        }

        /**
         * Sets the associated LDAPConnection
         *
         * @param ld the LDAPConnection
         * 
         */
        void setLDAPConn( LDAPConnection ld ) {
            this.ld = ld;
        }

        /**
         * Marks a connection in use or available
         *
         * @param inUse <code>true</code> to mark in use, <code>false</code> if available
         * 
         */
        void setInUse( boolean inUse ) {
            this.inUse = inUse;
        }

        /**
         * Returns whether the connection is available
         * for use by another user.
         *
         * @return <code>true</code> if available.
         */
        boolean isAvailable() {
            return !inUse;
        }
  
        /**
         * Debug method
         *
         * @return s user-friendly rendering of the object.
         */
        public String toString() {
            return "LDAPConnection=" + ld + ",inUse=" + inUse;
        }

        private LDAPConnection ld; // LDAP Connection
        private boolean inUse; // In use? (true = yes)
    }

    private int poolSize; // Min pool size
    private int poolMax; // Max pool size
    private String host; // LDAP host
    private int port;    // Port to connect at
    private String authdn;  // Identity of connections
    private byte[] authpw;  // Password for authdn
    private LDAPConnection ldc = null; // Connection to clone
    private java.util.Vector pool;  // the actual pool
    private boolean debugMode;
}
