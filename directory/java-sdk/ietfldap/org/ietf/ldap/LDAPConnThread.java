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
import org.ietf.ldap.client.*;
import org.ietf.ldap.client.opers.*;
import org.ietf.ldap.ber.stream.*;
import org.ietf.ldap.util.*;
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
class LDAPConnThread extends Thread {

    /**
     * Constants
     */
    private final static int MAXMSGID = Integer.MAX_VALUE;
    private final static int BACKLOG_CHKCNT = 50;

    /**
     * Internal variables
     */
    transient private static int _highMsgId;
    transient private InputStream _serverInput;
    transient private OutputStream _serverOutput;
    transient private Hashtable _requests;
    transient private Hashtable _messages = null;
    transient private Vector _registered;
    transient private boolean _disconnected = false;
    transient private LDAPCache _cache = null;
    transient private boolean _doRun = true;
    private Socket _socket = null;
    transient private Thread _thread = null;
    transient Object _sendRequestLock = new Object();
    transient LDAPConnSetupMgr _connMgr = null;
    transient Object _traceOutput = null;
    transient private int _backlogCheckCounter = BACKLOG_CHKCNT;


    // Time Stamp format Hour(0-23):Minute:Second.Milliseconds used for
    // trace msgs
    static SimpleDateFormat _timeFormat =
        new SimpleDateFormat( "HH:mm:ss.SSS" );
        
    /**
     * Constructs a connection thread that maintains connection to the
     * LDAP server
     *
     * @param connMgr
     * @param cache
     * @param traceOutput
     */
    public LDAPConnThread( LDAPConnSetupMgr connMgr,
                           LDAPCache cache,
                           Object traceOutput )
        throws LDAPException {
        super( "LDAPConnThread " + connMgr.getHost() +
               ":" + connMgr.getPort() );
        _requests = new Hashtable ();
        _registered = new Vector ();
        _connMgr = connMgr;
        _socket = connMgr.getSocket();
        setCache( cache );
        setTraceOutput( traceOutput );

        // Allow the application to exit if only LDAPConnThread threads are
        // running
        setDaemon( true );
        
        try {
            _serverInput =
                new BufferedInputStream( _socket.getInputStream() );
            _serverOutput =
                new BufferedOutputStream( _socket.getOutputStream() );
        } catch ( IOException e ) {
            // a kludge to make the thread go away. Since the thread has already
            // been created, the only way to clean up the thread is to call the
            // start() method. Otherwise, the exit method will be never called
            // because the start() was never called. In the run method, stop
            // is called right away if _doRun is false.
            _doRun = false;
            start();
            throw new LDAPException ( "failed to connect to server " +
                                      _connMgr.getHost(),
                                      LDAPException.CONNECT_ERROR );
        }

        if ( traceOutput != null ) {
            StringBuffer sb = new StringBuffer( " connected to " );
            sb.append( _connMgr.getLDAPUrl() );
            logTraceMessage( sb );
        }
        
        start(); /* start the thread */
    }

    InputStream getInputStream() {
        return _serverInput;
    }

    void setInputStream( InputStream is ) {
        _serverInput = is;
    }

    OutputStream getOutputStream() {
        return _serverOutput;
    }

    void setOutputStream( OutputStream os ) {
        _serverOutput = os;
    }

    void setTraceOutput( Object traceOutput ) {
        synchronized ( _sendRequestLock ) {
            if ( traceOutput == null ) {
               _traceOutput = null;
            } else if ( traceOutput instanceof OutputStream ) {
                _traceOutput = new PrintWriter( (OutputStream)traceOutput );
            } else if ( traceOutput instanceof LDAPTraceWriter ) {
                _traceOutput = traceOutput;
            }
        }            
    }
    
    void logTraceMessage( StringBuffer msg ) {
        if ( _traceOutput == null ) {
            return;
        }
        String timeStamp = _timeFormat.format( new Date() );
        StringBuffer sb = new StringBuffer( timeStamp );
        sb.append( " ldc=" ); 
        sb.append( _connMgr.getID() );

        synchronized( _sendRequestLock ) {
            if ( _traceOutput instanceof PrintWriter ) {
                PrintWriter traceOutput = (PrintWriter)_traceOutput;
                traceOutput.print( sb ); // header
                traceOutput.println( msg );
                traceOutput.flush();
            } else if ( _traceOutput instanceof LDAPTraceWriter ) {
                sb.append( msg );
                ((LDAPTraceWriter)_traceOutput).write( sb.toString() );
            }
        }
    }
    
    /**
     * Set the cache to use for searches.
     * @param cache The cache to use for searches; <CODE>null</CODE> for no
     * cache
     */
    synchronized void setCache( LDAPCache cache ) {
        _cache = cache;
        _messages = (_cache != null) ? new Hashtable() : null;
    }

    /**
     * Allocates a new LDAP message ID. These are arbitrary numbers used to
     * correlate client requests with server responses.
     *
     * @return new unique msgId
     */
    private int allocateId() {
        synchronized( _sendRequestLock ) {
            _highMsgId = (_highMsgId + 1) % MAXMSGID;
            return _highMsgId;
        }            
    }

    /**
     * Sends an LDAP request via this connection thread
     *
     * @param request request to send
     * @param toNotify response queue to receive the response
     * when ready
     */
    void sendRequest( LDAPConnection conn, JDAPProtocolOp request,
                      LDAPMessageQueue toNotify, LDAPConstraints cons )
        throws LDAPException {
        if ( !_doRun ) {
            throw new LDAPException ( "not connected to a server",
                                      LDAPException.SERVER_DOWN );
        }
        LDAPMessage msg = 
            new LDAPMessage( allocateId(), request, cons.getControls() );

        LDAPMessageQueueImpl queue = (LDAPMessageQueueImpl)toNotify;
        if ( queue != null ) {
            if ( !(request instanceof JDAPAbandonRequest ||
                   request instanceof JDAPUnbindRequest) ) {
                /* Only worry about toNotify if we expect a response... */
                this._requests.put( new Integer( msg.getMessageID()),
                                    queue );
                /* Notify the backlog checker that there may be another
                   outstanding request */
                resultRetrieved(); 
            }
            queue.addRequest( msg.getMessageID(), conn, this,
                              cons.getTimeLimit() );
        }

        synchronized( _sendRequestLock ) {
            try {
                if ( _traceOutput != null ) {
                    logTraceMessage( msg.toTraceString() );
                }                    
                msg.write( _serverOutput );
                _serverOutput.flush();
            } catch ( IOException e ) {
                networkError( e );
            }
        }
    }

    /**
     * Registers with this connection thread
     *
     * @param conn LDAP connection
     */
    public synchronized void register( LDAPConnection conn ) {
        if ( !_registered.contains( conn ) )
            _registered.addElement( conn );
    }

    int getClientCount() {
        return _registered.size();
    }

    boolean isRunning() {
        return _doRun;
    }

    /**
     * Deregisters with this connection thread. If all connections
     * are deregistered, this thread is to be killed.
     *
     * @param conn LDAP connection
     */
    public synchronized void deregister(LDAPConnection conn) {
        _registered.removeElement( conn );
        if ( _registered.size() == 0 ) {
            try {
                if ( !_disconnected ) {
                    LDAPSearchConstraints cons = conn.getSearchConstraints();
                    sendRequest( null, new JDAPUnbindRequest(), null, cons );
                }                    
                
                // must be set after the call to sendRequest
                _doRun =false;
                
                if ( (_thread != null) &&
                     (_thread != Thread.currentThread()) ) {
                    
                    _thread.interrupt();
                    
                    // Wait up to 1 sec for thread to accept disconnect
                    // notification. When the interrupt is accepted,
                    // _thread is set to null. See run() method.
                    try {
                        wait( 1000 );
                    }
                    catch ( InterruptedException e ) {
                    }                        
                }
                
            } catch( Exception e ) {
                LDAPConnection.printDebug( e.toString() );
            }
            finally {
                cleanUp();
            }
        }
    }

    /**
     * Cleans up before shutting down the thread
     */
    private void cleanUp() {
        if ( !_disconnected ) {
            try {
                _serverOutput.close();
            } catch ( Exception e ) {
            } finally {
                _serverOutput = null;
            }

            try {
                _serverInput.close ();
            } catch ( Exception e ) {
            } finally {
                _serverInput = null;
            }

            try {
                _socket.close ();
            } catch ( Exception e ) {
            } finally {
                _socket = null;
            }
        
            _disconnected = true;

            /**
             * Notify the Connection Setup Manager that the connection was
             * terminated by the user
             */
            _connMgr.disconnect();
        
            /**
             * Cancel all outstanding requests
             */
            if ( _requests != null ) {
                Enumeration requests = _requests.elements();
                while ( requests.hasMoreElements() ) {
                    LDAPMessageQueueImpl queue =
                        (LDAPMessageQueueImpl)requests.nextElement();
                    queue.removeAllRequests( this );
                }
            }                

            /**
             * Notify all the registered queues of this mishap.
             * IMPORTANT: This needs to be done last. Otherwise, the socket
             * input stream and output stream might never get closed and the
             * whole task will get stuck when trying to stop the
             * LDAPConnThread.
             */

            if ( _registered != null ) {
                Vector registerCopy = (Vector)_registered.clone();

                Enumeration cancelled = registerCopy.elements();

                while ( cancelled.hasMoreElements() ) {
                    LDAPConnection c = (LDAPConnection)cancelled.nextElement();
                    c.deregisterConnection();
                }
            }
            _registered.clear();
            _registered = null;
            _messages = null;
            _requests.clear();
            _cache = null;
        }
    }

    /**
     * Sleep if there is a backlog of search results
     */
    private void checkBacklog() throws InterruptedException{

        while ( true ) {

            if ( _requests.size() == 0 ) {
                return;
            }
            
            Enumeration queues = _requests.elements();
            while( queues.hasMoreElements() ) {
                LDAPMessageQueue l = (LDAPMessageQueue)queues.nextElement();

                // If there are any threads waiting for a regular response
                // message, we have to go read the next incoming message
                if ( !(l instanceof LDAPSearchQueue ) ) {
                    return;
                }

                LDAPSearchQueue sl = (LDAPSearchQueue)l;
                
                // should never happen, but just in case
                if ( sl.getSearchConstraints() == null ) {
                    return;
                }

                int slMaxBacklog = sl.getSearchConstraints().getMaxBacklog();
                int slBatchSize  = sl.getSearchConstraints().getBatchSize();
                
                // Disabled backlog check ?
                if ( slMaxBacklog == 0 ) {
                    return;
                }
                
                // Synch op with zero batch size ?
                if ( !sl.isAsynchOp() && slBatchSize == 0 ) {
                    return;
                }
                
                // Max backlog not reached for at least one queue ?
                // (if multiple requests are in progress)
                if ( sl.getMessageCount() < slMaxBacklog ) {
                    return;
                }                    
            }
                                   
            synchronized( this ) {
                wait( 3000 );
            }
        }
    }



    /**
     * This is called when a search result has been retrieved from the incoming
     * queue. We use the notification to unblock the queue thread, if it
     * is waiting for the backlog to lighten.
     */
    synchronized void resultRetrieved() {
        notifyAll();
    }

    /**
     * Reads from the LDAP server input stream for incoming LDAP messages.
     */
    public void run() {
        
        // if there is a problem establishing a connection to the server,
        // stop the thread right away...
        if ( !_doRun ) {
            return;
        }

        _thread = Thread.currentThread();
        LDAPMessage msg = null;
        JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();

        while ( _doRun ) {
            yield();
            int[] nread = new int[1];
            nread[0] = 0;
            try  {

                // Check after every BACKLOG_CHKCNT number of messages if the
                // backlog is too high
                if ( --_backlogCheckCounter <= 0 ) {
                    _backlogCheckCounter = BACKLOG_CHKCNT;
                    checkBacklog();
                }

                BERElement element = BERElement.getElement( decoder,
                                                            _serverInput,
                                                            nread );
                msg = LDAPMessage.parseMessage( element );

                if ( _traceOutput != null ) {
                    logTraceMessage( msg.toTraceString() );
                }                    

                // pass in the ber element size to approximate the size of the
                // cache entry, thereby avoiding serialization of the entry
                // stored in the cache
                processResponse( msg, nread[0] );

            } catch ( Exception e ) {
                if (_doRun) {
                    networkError( e );
                } else {
                    // interrupted from deregister()
                    synchronized( this ) {
                        _thread = null;
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
     * @param msg New message from LDAP server
     */
    private void processResponse( LDAPMessage msg, int size ) {
        Integer messageID = new Integer( msg.getMessageID() );
        LDAPMessageQueueImpl l =
            (LDAPMessageQueueImpl)_requests.get( messageID );
        if ( l == null ) {
            return; /* nobody is waiting for this response (!) */
        }

        // For asynch operations controls are to be read from the LDAPMessage
        // For synch operations controls are copied into the LDAPConnection
        // For synch search operations, controls are also copied into
        // LDAPSearchResults (see LDAPConnection.checkSearchMsg())
        if ( ! l.isAsynchOp() ) {
            
            /* Were there any controls for this client? */
            LDAPControl[] con = msg.getControls();
            if ( con != null ) {
                int msgid = msg.getMessageID();
                LDAPConnection ldc = l.getConnection( msgid );
                if ( ldc != null ) {
                    ldc.setResponseControls(
                        this, new LDAPResponseControl(ldc, msgid, con) );
                }                    
            }
        }

        if ( (_cache != null) && (l instanceof LDAPSearchQueue) ) {
            cacheSearchResult( (LDAPSearchQueue)l, msg, size );
        }            
        
        l.addMessage( msg );

        if ( msg instanceof LDAPResponse ) {
            _requests.remove( messageID );
            if ( _requests.size() == 0 ) {
                _backlogCheckCounter = BACKLOG_CHKCNT;
            }            
        }        
    }

    /**
     * Collects search results to be added to the LDAPCache. Search results are
     * packaged in a vector and temporary stored into a hashtable _messages
     * using the message id as the key. The vector first element (at index 0)
     * is a Long integer representing the total size of all LDAPEntries entries.
     * It is followed by the actual LDAPEntries.
     * If the total size of entries exceeds the LDAPCache max size, or a
     * referral has been received, caching of search results is disabled and
     * the entry is not added to the LDAPCache. A disabled search request is
     * denoted by setting the entry size to -1.
     */
    private synchronized void cacheSearchResult( LDAPSearchQueue l,
                                                 LDAPMessage msg, int size ) {
        Integer messageID = new Integer( msg.getMessageID() );
        Long key = l.getKey();
        Vector v = null;

        if ( (_cache == null) || (key == null) ) {
            return;
        }
        
        if ( msg instanceof LDAPSearchResult ) {

            // Get the vector containing the LDAPMessages for the specified
            // messageID
            v = (Vector)_messages.get( messageID );
            if ( v == null ) {
                _messages.put( messageID, v = new Vector() );
                v.addElement( new Long(0) );
            }

            // Return if the entry size is -1, i.e. caching is disabled
            if ( ((Long)v.firstElement()).longValue() == -1L ) {
                return;
            }
            
            // add the size of the current LDAPMessage to the lump sum
            // assume the size of LDAPMessage is more or less the same as
            // the size of LDAPEntry. Eventually an LDAPEntry object gets
            // stored in the cache instead of the LDAPMessage object.
            long entrySize = ((Long)v.firstElement()).longValue() + size;

            // If the entrySize exceeds the cache size, discard the collected
            // entries and disble collecting of entries for this search request
            // by setting the entry size to -1.
            if ( entrySize > _cache.getSize() ) {
                v.removeAllElements();
                v.addElement( new Long(-1L) );
                return;
            }                
                
            // update the lump sum located in the first element of the vector
            v.setElementAt( new Long(entrySize), 0 );

            // convert LDAPMessage object into LDAPEntry which is stored to the
            // end of the Vector
            v.addElement( ((LDAPSearchResult)msg).getEntry() );

        } else if ( msg instanceof LDAPSearchResultReference ) {

            // If a search reference is received disable caching of
            // this search request 
            v = (Vector)_messages.get(messageID);
            if ( v == null ) {
                _messages.put( messageID, v = new Vector() );
            }
            else {
                v.removeAllElements();
            }
            v.addElement( new Long(-1L) );

        } else if ( msg instanceof LDAPResponse ) {

            // The search request has completed. Store the cache entry
            // in the LDAPCache if the operation has succeded and caching
            // is not disabled due to the entry size or referrals
            
            boolean fail = ((LDAPResponse)msg).getResultCode() > 0;
            v = (Vector)_messages.remove( messageID );
            
            if ( !fail )  {
                // If v is null, meaning there are no search results from the
                // server
                if ( v == null ) {
                    v = new Vector();
                    v.addElement(new Long(0));
                }

                // add the new entry if the entry size is not -1 (caching
                // disabled)
                if ( ((Long)v.firstElement()).longValue() != -1L ) {
                    _cache.addEntry( key, v );
                }
            }
        }
    }

    /**
     * Stop dispatching responses for a particular message ID
     *
     * @param id Message ID for which to discard responses
     */
    void abandon( int id ) {
        if ( !_doRun ) {
            return;
        }
        
        LDAPMessageQueueImpl l =
            (LDAPMessageQueueImpl)_requests.remove( new Integer(id) );
        // Clean up cache if enabled
        if ( _messages != null ) {
            _messages.remove( new Integer(id) );
        }
        if ( l != null ) {
            l.removeRequest( id );
        }
        resultRetrieved(); // If LDAPConnThread is blocked in checkBacklog()
    }

    /**
     * Changes the queue for a message ID. Required when
     * LDAPMessageQueue.merge() is invoked.
     *
     * @param id Message ID for which to change the queue
     * @return Previous queue
     */
    LDAPMessageQueue changeQueue( int id, LDAPMessageQueue toNotify ) {
        if ( !_doRun ) {
            LDAPMessageQueueImpl queue = (LDAPMessageQueueImpl)toNotify;
            queue.setException( this,
                                new LDAPException( "Server down",
                                                   LDAPException.OTHER ) );
            return null;
        }
        return (LDAPMessageQueue)_requests.put( new Integer(id), toNotify );
    }

    /**
     * Handles network errors. Basically shuts down the whole connection.
     *
     * @param e The exception which was caught while trying to read from
     * input stream.
     */
    private synchronized void networkError( Exception e ) {
        _doRun = false;

        // notify the Connection Setup Manager that the connection is lost
        _connMgr.invalidateConnection();

        try {
            
            // notify each queue that the server is down.
            Enumeration requests = _requests.elements();
            while ( requests.hasMoreElements() ) {
                LDAPMessageQueueImpl queue =
                    (LDAPMessageQueueImpl)requests.nextElement();
                queue.setException( this,
                                    new LDAPException( "Server down",
                                                       LDAPException.OTHER) );
            }

        } catch( NullPointerException ee ) {
            System.err.println( "Exception: " + ee.toString() );
        }

        cleanUp();
    }
}
