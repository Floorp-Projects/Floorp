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

import java.io.*;
import java.net.*;
import java.util.*;
import javax.security.auth.callback.CallbackHandler;

import org.ietf.ldap.client.*;
import org.ietf.ldap.client.opers.*;
import org.ietf.ldap.ber.stream.*;
import org.ietf.ldap.util.*;

/**
 * Represents a connection to an LDAP server. <P>
 *
 * Use objects of this class to perform LDAP operations (such as
 * search, modify, and add) on an LDAP server. <P>
 *
 * To perform an LDAP operation on a server, you need to follow
 * these steps: <P>
 *
 * <OL>
 * <LI>Create a new <CODE>LDAPConnection</CODE> object.
 * <LI>Use the <CODE>connect</CODE> method to connect to the
 * LDAP server.
 * <LI>Use the <CODE>authenticate</CODE> method to authenticate
 * to server.
 * <LI>Perform the LDAP operation.
 * <LI>Use the <CODE>disconnect</CODE> method to disconnect from
 * the server when done.
 * </OL>
 * <P>
 *
 * All operations block until completion (with the exception of
 * the search method in which the results may not all return at
 * the same time).
 * <P>
 *
 * This class also specifies a default set of constraints
 * (such as the maximum length of time to allow for an operation before timing out)
 * which apply to all operations. To get and set these constraints,
 * use the <CODE>getOption</CODE> and <CODE>setOption</CODE> methods.
 * To override these constraints for an individual operation,
 * define a new set of constraints by creating a <CODE>LDAPConstraints</CODE>
 * object and pass the object to the method for the operation. For search 
 * operations, additional constraints are defined in <CODE>LDAPSearchConstraints</CODE>
 * (a subclass of <CODE>LDAPConstraints</CODE>). To override the default search
 * constraints, create an <CODE>LDAPSearchConstraints</CODE> object and pass it
 * to the <CODE>search</CODE> method.
 * <P>
 *
 * If you set up your client to follow referrals automatically,
 * an operation that results in a referral will create a new connection
 * to the LDAP server identified in the referral.  In order to have
 * your client authenticate to that LDAP server automatically, you need
 * to define a class that implements the <CODE>LDAPAuthHandler</CODE> interface.
 * In your definition of the class, you need to define a
 * <CODE>getAuthProvider</CODE> method that creates an <CODE>LDAPAuthProvider</CODE>
 * object containing the distinguished name and password to use for reauthentication.
 * <P>
 *
 * Most errors that occur raise the same exception (<CODE>LDAPException</CODE>).
 * In order to determine the exact problem that occurred, you can retrieve the
 * result code from this exception and compare its value against a set of defined
 * result codes.
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPConstraints
 * @see org.ietf.ldap.LDAPSearchConstraints
 * @see org.ietf.ldap.LDAPAuthHandler
 * @see org.ietf.ldap.LDAPAuthProvider
 * @see org.ietf.ldap.LDAPException
 */
public class LDAPConnection implements Cloneable, Serializable {

    static final long serialVersionUID = -8698420087475771144L;

    /**
     * Version of the LDAP protocol used by default.
     * <CODE>LDAP_VERSION</CODE> is 2, so your client will
     * attempt to authenticate to LDAP servers as an LDAP v2 client.
     * The following is an example of some code that prints the
     * value of this variable:
     * <P>
     *
     * <PRE>
     * LDAPConnection ld = new LDAPConnection();
     * System.out.println( "The default LDAP protocol version used is "
     *                      ld.LDAP_VERSION );
     * </PRE>
     *
     * If you want to authenticate as an LDAP v3 client,
     * use the <A HREF="#bind(int, java.lang.String, byte[])"><CODE>bind(int version, String dn, byte[] passwd)</CODE></A> method.
     * For example:
     * <P>
     *
     * <PRE>
     * ld.bind( 3, myDN, myPW );
     * </PRE>
     *
     * @see org.ietf.ldap.LDAPConnection#bind(int, java.lang.String, byte[])
     */
    public final static int LDAP_VERSION              = 2;
    /**
     * Name of the property specifying the version of the SDK. <P>
     *
     * To get the version number, pass this name to the
     * <CODE>getProperty</CODE> method.  The SDK version number
     * is of the type <CODE>Float</CODE>. For example:<P>
     * <PRE>
     *      ...
     *      Float sdkVersion = ( Float )myConn.getProperty( myConn.LDAP_PROPERTY_SDK );
     *      System.out.println( "SDK version: " + sdkVersion );
     *      ... </PRE>
     * @see org.ietf.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_SDK      = "version.sdk";
    /**
     * Name of the property specifying the highest supported version of
     * the LDAP protocol. <P>
     *
     * To get the version number, pass this name to the
     * <CODE>getProperty</CODE> method.  The LDAP protocol version number
     * is of the type <CODE>Float</CODE>. For example:<P>
     * <PRE>
     *      ...
     *      Float LDAPVersion = ( Float )myConn.getProperty( myConn.LDAP_PROPERTY_PROTOCOL );
     *      System.out.println( "Highest supported LDAP protocol version: " + LDAPVersion );
     *      ... </PRE>
     * @see org.ietf.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_PROTOCOL = "version.protocol";
    /**
     * Name of the property specifying the types of authentication allowed by this
     * API (for example, anonymous authentication and simple authentication). <P>
     *
     * To get the supported types, pass this name to the
     * <CODE>getProperty</CODE> method.  The value of this property is
     * of the type <CODE>String</CODE>. For example:<P>
     * <PRE>
     *      ...
     *      String authTypes = ( String )myConn.getProperty( myConn.LDAP_PROPERTY_SECURITY );
     *      System.out.println( "Supported authentication types: " + authTypes );
     *      ... </PRE>
     * @see org.ietf.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_SECURITY = "version.security";

    /**
     * Name of the property to enable/disable LDAP message trace. <P>
     *
     * The property can be specified either as a system property 
     * (java -D command line option),  or programmatically with the
     * <CODE>setProperty</CODE> method.
     * <P>
     * When -D command line option is used, defining the property with
     * no value will send the trace output to the standard error. If the 
     * value is defined, it is assumed to be the name of an output file.
     * If the file name is prefixed with a '+' character, the file is
     * opened in append mode.
     * <P>
     * When the property is set with the <CODE>setProperty</CODE> method,
     * the property value must be either a String (represents a file name)
     * an OutputStream or an instance of LDAPTraceWriter. To stop tracing,
     * <CODE>null</CODE> should be  passed as the property value.
     * 
     * @see org.ietf.ldap.LDAPConnection#setProperty(java.lang.String, java.lang.Object)
     */
    public final static String TRACE_PROPERTY = "com.org.ietf.ldap.trace";

    /**
     * Specifies the serial connection setup policy when a list of hosts is
     * passed to  the <CODE>connect</CODE> method.
     * @see org.ietf.ldap.LDAPConnection#setConnSetupDelay(int)
     */    
    public final static int NODELAY_SERIAL = -1;
    /**
     * Specifies the parallel connection setup policy with no delay when a
     * list of hosts is passed to the <CODE>connect</CODE> method.
     * For each host in the list, a separate thread is created to attempt
     * to connect to the host. All threads are started simultaneously.
     * @see org.ietf.ldap.LDAPConnection#setConnSetupDelay(int)
     */    
    public final static int NODELAY_PARALLEL = 0;

    /**
     * Constants
     */
    private final static String defaultFilter = "(objectClass=*)";
    private final static LDAPConstraints readConstraints = new
      LDAPSearchConstraints();

    /**
     * The default port number for LDAP servers.  You can specify
     * this identifier when calling the <CODE>LDAPConnection.connect</CODE>
     * method to connect to an LDAP server.
     * @see org.ietf.ldap.LDAPConnection#connect
     */
    public final static int DEFAULT_PORT = 389;

    /**
     * Option specifying how aliases are dereferenced.
     * <P>
     *
     * This option can have one of the following values:
     * <UL>
     * <LI><A HREF="#DEREF_NEVER"><CODE>DEREF_NEVER</CODE></A>
     * <LI><A HREF="#DEREF_FINDING"><CODE>DEREF_FINDING</CODE></A>
     * <LI><A HREF="#DEREF_SEARCHING"><CODE>DEREF_SEARCHING</CODE></A>
     * <LI><A HREF="#DEREF_ALWAYS"><CODE>DEREF_ALWAYS</CODE></A>
     * </UL>
     * <P>
     *
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int DEREF = 2;

    /**
     * Option specifying the maximum number of search results to
     * return.
     * <P>
     *
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int SIZELIMIT = 3;

    /**
     * Option specifying the maximum number of milliseconds to
     * wait for an operation to complete.
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int TIMELIMIT = 4;

    /**
     * Option specifying the maximum number of milliseconds the 
     * server should spend returning search results before aborting
     * the search. 
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int SERVER_TIMELIMIT = 5;

    /**
     * Option specifying whether or not referrals to other LDAP
     * servers are followed automatically.
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     * @see org.ietf.ldap.LDAPAuthHandler
     * @see org.ietf.ldap.LDAPAuthProvider
     */
    public static final int REFERRALS = 8;

    /**
     * Option specifying the object containing the method for
     * getting authentication information (the distinguished name
     * and password) used during a referral.  For example, when
     * referred to another LDAP server, your client uses this object
     * to obtain the DN and password.  Your client authenticates to
     * the LDAP server using this DN and password.
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     * @see org.ietf.ldap.LDAPAuthHandler
     * @see org.ietf.ldap.LDAPAuthProvider
     */
    public static final int REFERRALS_REBIND_PROC = 9;

    /**
     * Option specifying the maximum number of referrals to follow
     * in a sequence when requesting an LDAP operation.
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int REFERRALS_HOP_LIMIT   = 10;
    
    /**
     * Option specifying the object containing the method for
     * authenticating to the server.  
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     * @see org.ietf.ldap.LDAPBindHandler
     */
    public static final int BIND = 13;

    /**
     * Option specifying the version of the LDAP protocol
     * used by your client when interacting with the LDAP server.
     * If no version is set, the default version is 2.  If you
     * are planning to use LDAP v3 features (such as controls
     * or extended operations), you should set this version to 3
     * or specify version 3 as an argument to the <CODE>authenticate</CODE>
     * method of the <CODE>LDAPConnection</CODE> object.
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     * @see org.ietf.ldap.LDAPConnection#bind(int, java.lang.String, byte[])
     */
    public static final int PROTOCOL_VERSION = 17;

    /**
     * Option specifying the number of results to return at a time.
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int BATCHSIZE = 20;


    /*
     * Valid options for Scope
     */

    /**
     * Specifies that the scope of a search includes
     * only the base DN (distinguished name).
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, org.ietf.ldap.LDAPSearchConstraints)
     */
    public static final int SCOPE_BASE = 0;

    /**
     * Specifies that the scope of a search includes
     * only the entries one level below the base DN (distinguished name).
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, org.ietf.ldap.LDAPSearchConstraints)   */
    public static final int SCOPE_ONE = 1;

    /**
     * Specifies that the scope of a search includes
     * the base DN (distinguished name) and all entries at all levels
     * beneath that base.
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, org.ietf.ldap.LDAPSearchConstraints)   */
    public static final int SCOPE_SUB = 2;


    /*
     * Valid options for Dereference
     */

    /**
     * Specifies that aliases are never dereferenced.
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int DEREF_NEVER = 0;

    /**
     * Specifies that aliases are dereferenced when searching the
     * entries beneath the starting point of the search (but
     * not when finding the starting entry).
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int DEREF_SEARCHING = 1;

    /**
     * Specifies that aliases are dereferenced when finding the
     * starting point for the search (but not when searching
     * under that starting entry).
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int DEREF_FINDING = 2;

    /**
     * Specifies that aliases are always dereferenced.
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int DEREF_ALWAYS = 3;

    /**
     * Option specifying server controls for LDAP operations. These
     * controls are passed to the LDAP server. They may also be returned by
     * the server.
     * @see org.ietf.ldap.LDAPControl
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int SERVERCONTROLS   = 12;

    /**
     * Attribute type that you can specify in the LDAPConnection
     * search method if you don't want to retrieve any of the
     * attribute types for entries found by the search.
     * @see org.ietf.ldap.LDAPConnection#search
     */
    public static final String NO_ATTRS = "1.1";

    /**
     * Attribute type that you can specify in the LDAPConnection
     * search method if you want to retrieve all attribute types.
     * You can use this if you want to retrieve all attributes in
     * addition to an operational attribute.  For example:
     * <P>
     *
     * <PRE>
     * ...
     * String [] MY_ATTRS = { LDAPConnection.ALL_USER_ATTRS, "modifiersName",
     *     "modifyTimestamp" };
     * LDAPSearchResults res = ld.search( MY_SEARCHBASE,
     *     LDAPConnection.SCOPE_SUB, MY_FILTER, MY_ATTRS, false, cons );
     * ...
     * </PRE>
     * @see org.ietf.ldap.LDAPConnection#search
     */
    public static final String ALL_USER_ATTRS = "*";

    /**
     * Internal variables
     */
    private LDAPSearchConstraints _defaultConstraints =
        new LDAPSearchConstraints();
    
    // A clone of constraints for the successful bind. Used by 
    // "smart failover" for the automatic rebind
    private LDAPConstraints _rebindConstraints;

    private Vector _responseListeners;
    private Vector _searchListeners;
    private boolean _bound;
    private String _prevBoundDN;
    private byte[] _prevBoundPasswd;
    private String _boundDN;
    private byte[] _boundPasswd;
    private int _protocolVersion = LDAP_VERSION;
    private LDAPConnSetupMgr _connMgr;
    private int _connSetupDelay = -1;
    private int _connectTimeout = 0;
    private LDAPSocketFactory _factory;
    /* _thread does all socket i/o for the object and any clones */
    private transient LDAPConnThread _thread = null;
    /* To manage received server controls on a per-thread basis,
       we keep a table of active threads and a table of controls,
       indexed by thread */
    private Vector _attachedList = new Vector();
    private Hashtable _responseControlTable = new Hashtable();
    private LDAPCache _cache = null;
    static Hashtable _threadConnTable = new Hashtable();

    // This handles the case when the client lost the connection to the
    // server. After the client reconnects with the server, the bound resets
    // to false. If the client used to have anonymous bind, then this boolean
    // will take care of the case whether the client should send anonymous bind
    // request to the server.
    private boolean _anonymousBound = false;

    private Object _security = null;
    private LDAPSaslBind _saslBinder = null;
    private CallbackHandler _saslCallbackHandler = null;
    private Properties _securityProperties;
    private Hashtable _properties = new Hashtable();
    private LDAPConnection _referralConnection;
    private String _authMethod = "none";

    /**
     * Properties
     */
    private final static Float SdkVersion = new Float(4.14f);
    private final static Float ProtocolVersion = new Float(3.0f);
    private final static String SecurityVersion = new String("none,simple,sasl");
    private final static Float MajorVersion = new Float(4.0f);
    private final static Float MinorVersion = new Float(0.14f);
    private final static String DELIM = "#";
    private final static String PersistSearchPackageName =
      "org.ietf.ldap.controls.LDAPPersistSearchControl";
    final static String EXTERNAL_MECHANISM = "external";
    private final static String EXTERNAL_MECHANISM_PACKAGE =
      "com.netscape.sasl";
    final static String DEFAULT_SASL_PACKAGE =
      "com.netscape.sasl";
    final static String SCHEMA_BUG_PROPERTY =
      "com.org.ietf.ldap.schema.quoting";
    final static String SASL_PACKAGE_PROPERTY =
      "com.org.ietf.ldap.saslpackage";

   /**
    * Constructs a new <CODE>LDAPConnection</CODE> object,
    * which represents a connection to an LDAP server. <P>
    *
    * Calling the constructor does not actually establish
    * the connection.  To connect to the LDAP server, use the
    * <CODE>connect</CODE> method.
    *
    * @see org.ietf.ldap.LDAPConnection#connect(java.lang.String, int)
    * @see org.ietf.ldap.LDAPConnection#bind(int, java.lang.String, byte[])
    */
    public LDAPConnection() {
        super();
        _factory = null;

        _properties.put(LDAP_PROPERTY_SDK, SdkVersion); 
        _properties.put(LDAP_PROPERTY_PROTOCOL, ProtocolVersion); 
        _properties.put(LDAP_PROPERTY_SECURITY, SecurityVersion); 
        _properties.put("version.major", MajorVersion); 
        _properties.put("version.minor", MinorVersion);
    }

    /**
     * Constructs a new <CODE>LDAPConnection</CODE> object that
     * will use the specified socket factory class to create
     * socket connections. The socket factory class must implement
     * the <CODE>LDAPSocketFactory</CODE> interface. <BR>
     * (For example, the <CODE>LDAPSSLSocketFactory</CODE>
     * class implements this interface.)
     * <P>
     *
     * Note that calling the <CODE>LDAPConnection</CODE> constructor
     * does not actually establish a connection to an LDAP server.
     * To connect to an LDAP server, use the
     * <CODE>connect</CODE> method.  The socket connection will be
     * constructed when this method is called.
     * <P>
     *
     * @see org.ietf.ldap.LDAPSocketFactory
     * @see org.ietf.ldap.LDAPSSLSocketFactory
     * @see org.ietf.ldap.LDAPConnection#connect(java.lang.String, int)
     * @see org.ietf.ldap.LDAPConnection#bind(int, java.lang.String, byte[])
     * @see org.ietf.ldap.LDAPConnection#getSocketFactory
     * @see org.ietf.ldap.LDAPConnection#setSocketFactory(org.ietf.ldap.LDAPSocketFactory)
     */
    public LDAPConnection( LDAPSocketFactory factory ) {
        this();
        _factory = factory;
    }

    /**
     * Abandons a current search operation, notifying the server not
     * to send additional search results.
     *
     * @param searchResults the search results returned when the search
     * was started
     * @exception LDAPException Failed to notify the LDAP server.
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, org.ietf.ldap.LDAPSearchConstraints)
     * @see org.ietf.ldap.LDAPSearchResults
     */
    public void abandon( LDAPSearchResults searchResults )
        throws LDAPException {
        abandon( searchResults, _defaultConstraints );
    }

    /**
     * Abandons a current search operation, notifying the server not
     * to send additional search results.
     *
     * @param searchResults the search results returned when the search
     * was started
     * @param cons preferences for the operation
     * @exception LDAPException Failed to notify the LDAP server.
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, org.ietf.ldap.LDAPSearchConstraints)
     * @see org.ietf.ldap.LDAPSearchResults
     */
    public void abandon( LDAPSearchResults searchResults,
                         LDAPConstraints cons )
        throws LDAPException {
        if ( (!isConnected()) || (searchResults == null) ) {
            return;
        }

        int id = searchResults.getMessageID();
        if ( id != -1 ) {
            abandon( id );
        }
    }

    /**
     * Cancels the ldap request with the specified id and discards
     * any results already received.
     * 
     * @param id an LDAP request id
     * @exception LDAPException Failed to send request.
     */
    public void abandon( int id ) throws LDAPException {
        abandon( id, _defaultConstraints );
    }
    
    /**
     * Cancels the ldap request with the specified id and discards
     * any results already received.
     * 
     * @param id an LDAP request id
     * @param cons preferences for the operation
     * @exception LDAPException Failed to send request.
     */
    public void abandon( int id,
                         LDAPConstraints cons ) throws LDAPException {
        if (!isConnected()) {
            return;
        }
        
        for (int i=0; i<3; i++) {
            try {
                /* Tell listener thread to discard results */
                _thread.abandon( id );

                /* Tell server to stop sending results */
                _thread.sendRequest(null, new JDAPAbandonRequest(id), null, _defaultConstraints);

                /* No response is forthcoming */
                break;
            } catch (NullPointerException ne) {
                // do nothing
            }
        }
        if (_thread == null) {
            throw new LDAPException("Failed to send abandon request to the server.",
              LDAPException.OTHER);
        }
    }
    
    /**
     * Cancels all outstanding search requests associated with this
     * LDAPSearchQueue object and discards any results already received.
     * 
     * @param searchlistener a search listener returned from a search
     * @exception LDAPException Failed to send request.
     */
    public void abandon( LDAPSearchQueue searchlistener )
        throws LDAPException {
        abandon( searchlistener, _defaultConstraints );
    }
    
    /**
     * Cancels all outstanding search requests associated with this
     * LDAPSearchQueue object and discards any results already received.
     * 
     * @param searchlistener a search listener returned from a search
     * @param cons preferences for the operation
     * @exception LDAPException Failed to send request.
     */
    public void abandon( LDAPSearchQueue searchlistener,
                         LDAPConstraints cons )
        throws LDAPException {
        int[] ids = searchlistener.getMessageIDs();
        for (int i=0; i < ids.length; i++) {
            searchlistener.removeRequest(ids[i]);
            abandon(ids[i]);
        }
    }
    
    /**
     * Adds an entry to the directory. <P>
     *
     * Before using this method, you need to create an
     * <CODE>LDAPEntry</CODE> object and use it to specify the
     * distinguished name and attributes of the new entry. Make sure
     * to specify values for all required attributes in the
     * entry. If all required attributes are not specified and the LDAP server
     * checks the entry against the schema, an <CODE>LDAPException</CODE>
     * may be thrown (where the LDAP result code is
     * <CODE>OBJECT_CLASS_VIOLATION</CODE>).<P>
     *
     * For example, the following section of code creates an
     * <CODE>LDAPEntry</CODE> object for a new entry and uses the object
     * to add the new entry to the directory. Because the definition of
     * the LDAP <CODE>inetOrgPerson</CODE> class specifies that the
     * <CODE>cn</CODE>, <CODE>sn</CODE>, and <CODE>objectclass</CODE>
     * attributes are required, these attributes are specified as part
     * of the new entry.  (<CODE>mail</CODE> is not required but is shown
     * here as an example of specifying additional attributes.)
     * <P>
     *
     * <PRE>
     * ...
     * String myDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     *
     * LDAPAttribute attr1 = new LDAPAttribute( "cn", "Barbara Jensen" );
     * LDAPAttribute attr2 = new LDAPAttribute( "sn", "Jensen" );
     * LDAPAttribute attr3 = new LDAPAttribute( "objectclass", "top" );
     * LDAPAttribute attr4 = new LDAPAttribute( "objectclass", "person" );
     * LDAPAttribute attr5 = new LDAPAttribute( "objectclass", "organizationalPerson" );
     * LDAPAttribute attr6 = new LDAPAttribute( "objectclass", "inetOrgPerson" );
     * LDAPAttribute attr7 = new LDAPAttribute( "mail", "bjensen@aceindustry.com" );
     *
     * LDAPAttributeSet myAttrs = new LDAPAttributeSet();
     * myAttrs.add( attr1 );
     * myAttrs.add( attr2 );
     * myAttrs.add( attr3 );
     * myAttrs.add( attr4 );
     * myAttrs.add( attr5 );
     * myAttrs.add( attr6 );
     * myAttrs.add( attr7 );
     *
     * LDAPEntry myEntry = new LDAPEntry( myDN, myAttrs );
     *
     * myConn.add( myEntry );
     * ... </PRE>
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry
     * @exception LDAPException Failed to add the specified entry to the
     * directory.
     * @see org.ietf.ldap.LDAPEntry
     */
    public void add( LDAPEntry entry ) throws LDAPException {
        add(entry, _defaultConstraints);
    }

    /**
     * Adds an entry to the directory and allows you to specify preferences
     * for this LDAP add operation by using an
     * <CODE>LDAPConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to add the specified entry to the
     * directory.
     * @see org.ietf.ldap.LDAPEntry
     * @see org.ietf.ldap.LDAPConstraints
     */
    public void add( LDAPEntry entry, LDAPConstraints cons )
        throws LDAPException {
        internalBind (cons);

        LDAPResponseQueue myListener = getResponseListener ();
        LDAPAttributeSet attrs = entry.getAttributeSet ();
        LDAPAttribute[] attrList = new LDAPAttribute[attrs.size()];
        Iterator it = attrs.iterator();
        for( int i = 0; i < attrs.size(); i++ ) {
            attrList[i] = (LDAPAttribute)it.next();
        }
        int attrPosition = 0;
        LDAPMessage response;
        try {
            sendRequest (new JDAPAddRequest (entry.getDN(), attrList),
                         myListener, cons);
            response = myListener.getResponse();
            checkMsg (response);
        } catch (LDAPReferralException e) {
            performReferrals(e, cons, JDAPProtocolOp.ADD_REQUEST,
                             null, 0, null, null, false, null, entry, null, null);
        } finally {
            releaseResponseListener (myListener);
        }
    }

    /**
     * Adds an entry to the directory.
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPEntry
     * @see org.ietf.ldap.LDAPResponseQueue
     */
    public LDAPResponseQueue add( LDAPEntry entry,
                                  LDAPResponseQueue listener )
        throws LDAPException{
        return add( entry, listener, _defaultConstraints );
    }
 
    /**
     * Adds an entry to the directory and allows you to specify constraints
     * for this LDAP add operation by using an <CODE>LDAPConstraints</CODE>
     * object. For example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPEntry
     * @see org.ietf.ldap.LDAPResponseQueue
     * @see org.ietf.ldap.LDAPConstraints
     */
    public LDAPResponseQueue add( LDAPEntry entry,
                                  LDAPResponseQueue listener,
                                  LDAPConstraints cons )
        throws LDAPException{

        if (cons == null) {
            cons = _defaultConstraints;
        }
        
        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseQueue(/*asynchOp=*/true);
        }

        LDAPAttributeSet attrs = entry.getAttributeSet ();
        LDAPAttribute[] attrList = new LDAPAttribute[attrs.size()];
        Iterator it = attrs.iterator();
        for( int i = 0; i < attrs.size(); i++ ) {
            attrList[i] = (LDAPAttribute)it.next();
        }
        int attrPosition = 0;

        sendRequest (new JDAPAddRequest (entry.getDN(), attrList),
                    listener, cons);

        return listener;
    }

    /**
     * Registers an object to be notified on arrival of an unsolicited 
     * message from a server
     *
     * @param listener An object to be notified on arrival of an 
     * unsolicited message from a server
     */
    public void addUnsolicitedNotificationListener( 
        LDAPUnsolicitedNotificationListener listener ) {
    }

    /** 
     * Authenticates to the LDAP server (to which you are currently
     * connected) using the specified name and password.
     * If you are not already connected to the LDAP server, this
     * method attempts to reconnect to the server.
     * <P>
     *
     * For example, the following section of code authenticates the
     * client as Barbara Jensen.  The code assumes that the client
     * has already established a connection with an LDAP server.
     * <P>
     *
     * <PRE>
     * String myDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String myPW = "hifalutin";
     * try {
     *     myConn.bind( myDN, myPW.getBytes() );
     * } catch ( LDAPException e ) {
     *     switch( e.getResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified user does not exist." );
     *             break;
     *         case e.INVALID_CREDENTIALS:
     *             System.out.println( "Invalid password." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getResultCode() );
     *             System.out.println( "Failed to authentice as " + myDN );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Authenticated as " + myDN );
     * </PRE>
     *
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    private void bind( String dn, byte[] passwd) throws LDAPException {
        bind( _protocolVersion, dn, passwd, _defaultConstraints );
    }

    /**
     * Authenticates to the LDAP server (to which you are currently
     * connected) using the specified name and password. The
     * default protocol version (version 2) is used. If the server
     * doesn't support the default version, an LDAPException is thrown
     * with the error code PROTOCOL_ERROR. This method allows the
     * user to specify the preferences for the bind operation.
     *
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @param cons preferences for the bind operation
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    private void bind( String dn, byte[] passwd,
                      LDAPConstraints cons ) throws LDAPException {
        bind( _protocolVersion, dn, passwd, cons );
    }

    /**
     * Authenticates to the LDAP server (to which you are currently
     * connected) using the specified name and password, and
     * requests that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR.
     *
     * @param version required LDAP protocol version
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind( int version, String dn, byte[] passwd )
        throws LDAPException {
        bind( version, dn, passwd, _defaultConstraints );
    }

    /**
     * Authenticates to the LDAP server (to which you are currently
     * connected) using the specified name and password, and
     * requesting that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR. This method allows the user to specify the
     * preferences for the bind operation.
     *
     * @param version required LDAP protocol version
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @param cons preferences for the bind operation
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind( int version, String dn, byte[] passwd,
                      LDAPConstraints cons ) throws LDAPException {
        _prevBoundDN = _boundDN;
        _prevBoundPasswd = _boundPasswd;
        _boundDN = dn;
        _boundPasswd = passwd;

        _anonymousBound =
            ((_prevBoundDN == null) || (_prevBoundPasswd == null));

        internalBind (version, true, cons);
    }

    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and whatever SASL mechanisms
     * are supported by the server. Each supported mechanism in turn
     * is tried until authentication succeeds or an exception is thrown.
     * If the object has been disconnected from an LDAP server, this
     * method attempts to reconnect to the server. If the object had
     * already authenticated, the old authentication is discarded.
     *
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param authzid If not null and not empty, an LDAP authzID [AUTH] 
     * to be passed to the SASL layer. If null or empty, 
     * the authzId will be treated as an empty string 
     * and processed as per RFC 2222 [SASL].
     * @param props Optional qualifiers for the authentication session
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind( String dn,
                      String authzid,
                      Map props,
                      CallbackHandler cbh )
        throws LDAPException {
        bind( dn, authzid, props, cbh, _defaultConstraints );
    }

    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and whatever SASL mechanisms
     * are supported by the server. Each supported mechanism in turn
     * is tried until authentication succeeds or an exception is thrown.
     * If the object has been disconnected from an LDAP server, this
     * method attempts to reconnect to the server. If the object had
     * already authenticated, the old authentication is discarded.
     *
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param authzid If not null and not empty, an LDAP authzID [AUTH] 
     * to be passed to the SASL layer. If null or empty, 
     * the authzId will be treated as an empty string 
     * and processed as per RFC 2222 [SASL].
     * @param props Optional qualifiers for the authentication session
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @param cons preferences for the bind operation
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind( String dn,
                      String authzid,
                      Map props,
                      CallbackHandler cbh,
                      LDAPConstraints cons )
        throws LDAPException {
        if ( !isConnected() ) {
            connect();
        }
        // Get the list of mechanisms from the server
        String[] attrs = { "supportedSaslMechanisms" };
        LDAPEntry entry = read( "", attrs );
        LDAPAttribute attr = entry.getAttribute( attrs[0] );
        if ( attr == null ) {
            throw new LDAPException( "Not found in root DSE: " +
                                     attrs[0],
                                     LDAPException.NO_SUCH_ATTRIBUTE );
        }
        bind( dn, authzid, attr.getStringValueArray(), props, cbh, cons );
    }

    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param authzid If not null and not empty, an LDAP authzID [AUTH] 
     * to be passed to the SASL layer. If null or empty, 
     * the authzId will be treated as an empty string 
     * and processed as per RFC 2222 [SASL].
     * @param props Optional qualifiers for the authentication session
     * @param mechanisms a list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @see org.ietf.ldap.LDAPConnection#bind(String, String, Map,
     * CallbackHandler)
     */
    public void bind( String dn,
                      String authzid,
                      String[] mechanisms,
                      Map props,
                      CallbackHandler cbh )
        throws LDAPException {
        bind( dn, authzid, mechanisms, props, cbh, _defaultConstraints );
    }

    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param authzid If not null and not empty, an LDAP authzID [AUTH] 
     * to be passed to the SASL layer. If null or empty, 
     * the authzId will be treated as an empty string 
     * and processed as per RFC 2222 [SASL].
     * @param props Optional qualifiers for the authentication session
     * @param mechanisms a list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @param cons preferences for the bind operation
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @see org.ietf.ldap.LDAPConnection#bind(String, String, Map,
     * CallbackHandler, LDAPConstraints)
     */
    public void bind( String dn,
                      String authzid,
                      String[] mechanisms,
                      Map props,
                      CallbackHandler cbh,
                      LDAPConstraints cons )
        throws LDAPException {

        if ( !isConnected() ) {
            connect();
        } else {
            // If the thread has more than one LDAPConnection, then
            // we should disconnect first. Otherwise,
            // we can reuse the same thread for the rebind.
            if (_thread.getClientCount() > 1) {
                disconnect();
                connect();
            }
        }
        _boundDN = null;
        _protocolVersion = 3; // Must be 3 for SASL
        if ( props == null ) {
            props = new Hashtable();
        }
        _saslBinder = new LDAPSaslBind( dn, mechanisms,
                                        props, cbh );
        _saslCallbackHandler = cbh;
        _saslBinder.bind( this );
        _authMethod = "sasl";
        _boundDN = dn;
    }

    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and password. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param version required LDAP protocol version
     * @param dn if non-null and non-empty, specifies that the connection
     * and all operations through it should authenticate with dn as the
     * distinguished name
     * @param passwd if non-null and non-empty, specifies that the connection
     * and all operations through it should authenticate with dn as the
     * distinguished name and passwd as password
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     */
    public LDAPResponseQueue bind( int version,
                                   String dn,
                                   byte[] passwd,
                                   LDAPResponseQueue listener )
        throws LDAPException{
        return bind( version, dn, passwd, listener, _defaultConstraints );
    }

    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and password. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param dn if non-null and non-empty, specifies that the connection
     * and all operations through it should authenticate with dn as the
     * distinguished name
     * @param passwd if non-null and non-empty, specifies that the connection
     * and all operations through it should authenticate with dn as the
     * distinguished name and passwd as password
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     */
    private LDAPResponseQueue bind( String dn,
                                    byte[] passwd,
                                    LDAPResponseQueue listener)
        throws LDAPException{
        return bind( _protocolVersion, dn, passwd, listener,
                     _defaultConstraints );
    }

    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and password and allows you
     * to specify constraints for this LDAP add operation by using an
     *  <CODE>LDAPConstraints</CODE> object. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param dn if non-null and non-empty, specifies that the connection
     * and all operations through it should authenticate with dn as the
     * distinguished name
     * @param passwd if non-null and non-empty, specifies that the connection
     * and all operations through it should authenticate with dn as the
     * distinguished name and passwd as password
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     * @see org.ietf.ldap.LDAPConstraints
     */
    private LDAPResponseQueue bind( String dn,
                                    byte[] passwd,
                                    LDAPResponseQueue listener,
                                    LDAPConstraints cons )
        throws LDAPException{
        return bind( _protocolVersion, dn, passwd, listener, cons );
    }

    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and password and allows you
     * to specify constraints for this LDAP add operation by using an
     *  <CODE>LDAPConstraints</CODE> object. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param version required LDAP protocol version
     * @param dn if non-null and non-empty, specifies that the connection
     * and all operations through it should authenticate with dn as the
     * distinguished name
     * @param passwd if non-null and non-empty, specifies that the connection
     * and all operations through it should authenticate with dn as the
     * distinguished name and passwd as password
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     * @see org.ietf.ldap.LDAPConstraints
     */
    public LDAPResponseQueue bind( int version,
                                   String dn,
                                   byte[] passwd,
                                   LDAPResponseQueue listener,
                                   LDAPConstraints cons ) 
        throws LDAPException{
        if (cons == null) {
            cons = _defaultConstraints;
        }

        _prevBoundDN = _boundDN;
        _prevBoundPasswd = _boundPasswd;
        _boundDN = dn;
        _boundPasswd = passwd;
        _protocolVersion = version;
        
        if (_thread == null) {
            connect();
        }
        else if (_thread.getClientCount() > 1) {
            disconnect();
            connect();
        }

        if (listener == null) {
            listener = new LDAPResponseQueue(/*asynchOp=*/true);
        }

        sendRequest(new JDAPBindRequest(version, _boundDN, _boundPasswd),
            listener, cons);

        return listener;
    }
    
    /**
     * Authenticates to the LDAP server (to which the object is currently
     * connected) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param authzid If not null and not empty, an LDAP authzID [AUTH] 
     * to be passed to the SASL layer. If null or empty, 
     * the authzId will be treated as an empty string 
     * and processed as per RFC 2222 [SASL].
     * @param mechanisms a list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param packageName a package containing a SASL ClientFactory,
     * e.g. "myclasses.SASL". If null, a system default is used.
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @deprecated Please use authenticate without packageName
     * instead.
     */
    public void bind( String dn,
                      String authzid,
                      String[] mechanisms,
                      String packageName,
                      Map props,
                      CallbackHandler cbh,
                      LDAPConstraints cons )
        throws LDAPException {
        if ( props == null ) {
            props = new Hashtable();
        }
        props.put( LDAPSaslBind.CLIENTPKGS, packageName );
        bind( dn, authzid, mechanisms, props, cbh, cons );
    }
    
    /**
     * Creates and returns a copy of the object. The new
     * <CODE>LDAPConnection</CODE> object contains the same information as
     * the original connection, including:
     * <UL>
     * <LI>the default search constraints
     * <LI>host name and port number of the LDAP server
     * <LI>the DN and password used to authenticate to the LDAP server
     * </UL>
     * <P>
     * @return the <CODE>LDAPconnection</CODE> object representing the
     * new object.
     */
    public synchronized Object clone() {
        try {
            LDAPConnection c = (LDAPConnection)super.clone();

            if (!isConnected()) {
                this.internalBind(_defaultConstraints);
            }

            c._defaultConstraints =
                (LDAPSearchConstraints)_defaultConstraints.clone();
            c._responseListeners = null;
            c._searchListeners = null;
            c._bound = this._bound;
            c._connMgr = _connMgr;
            c._connSetupDelay = _connSetupDelay;
            c._boundDN = this._boundDN;
            c._boundPasswd = this._boundPasswd;
            c._prevBoundDN = this._prevBoundDN;
            c._prevBoundPasswd = this._prevBoundPasswd;
            c._anonymousBound = this._anonymousBound;
            c.setCache(this._cache); // increments cache reference cnt
            c._factory = this._factory;
            c._thread = this._thread; /* share current connection thread */

            synchronized (_threadConnTable) {
                Vector v = (Vector)_threadConnTable.get(this._thread);
                if (v != null) {
                    v.addElement(c);
                } else {
                    printDebug("Failed to clone");
                    return null;
                }
            }

            c._thread.register(c);
            return c;
        } catch (Exception e) {
        }
        return null;
    }

    /**
     * Checks to see if an entry contains an attribute with a specified value.
     * Returns <CODE>true</CODE> if the entry has the value. Returns
     * <CODE>false</CODE> if the entry does not have the value or the
     * attribute. To represent the value that you want compared, you need
     * to create an <CODE>LDAPAttribute</CODE> object.<P>
     *
     * Note that only string values can be compared. <P>
     *
     * For example, the following section of code checks to see if the entry
     * "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US" contains
     * the attribute "mail" with the value "bjensen@aceindustry.com".
     *
     * <PRE>
     * ...
     * LDAPConnection myConn = new LDAPConnection();
     * ...
     * String myDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String nameOfAttr = "mail";
     * String valOfAttr = "bjensen@aceindustry.com";
     * LDAPAttribute cmpThisAttr = new LDAPAttribute( nameOfAttr, valOfAttr );
     * boolean hasValue = myConn.compare( myDN, cmpThisAttr );
     * if ( hasValue ) {
     *     System.out.println( "Attribute and value found in entry." );
     * } else {
     *     System.out.println( "Attribute and value not found in entry." );
     * }
     * ...</PRE>
     *
     * @param DN the distinguished name of the entry to use in
     * the comparison
     * @param attr the attribute to compare against the entry.
     * (The method checks to see if the entry has an attribute with the same name
     * and value as this attribute.)
     * @return true if the entry contains the specified attribute and value.
     * @exception LDAPException Failed to perform the comparison.
     * @see org.ietf.ldap.LDAPAttribute
     */
    public boolean compare( String DN, LDAPAttribute attr )
        throws LDAPException {
        return compare( DN, attr, _defaultConstraints );
    }

    public boolean compare( String DN, LDAPAttribute attr,
                            LDAPConstraints cons ) throws LDAPException {
        internalBind(cons);

        LDAPResponseQueue myListener = getResponseListener ();
        Enumeration en = attr.getStringValues();
        String val = (String)en.nextElement();
        JDAPAVA ass = new JDAPAVA(attr.getName(), val);

        LDAPMessage response;
        try {
            sendRequest (new JDAPCompareRequest (DN, ass), myListener, cons);
            response = myListener.getResponse ();

            int resultCode = ((JDAPResult)response.getProtocolOp()).getResultCode();
            if (resultCode == JDAPResult.COMPARE_FALSE) {
                return false;
            }
            if (resultCode == JDAPResult.COMPARE_TRUE) {
                return true;
            }

            checkMsg (response);

        } catch (LDAPReferralException e) {
            Vector res = new Vector();
            performReferrals(e, cons, JDAPProtocolOp.COMPARE_REQUEST,
                             DN, 0, null, null, false, null, null, attr, res);
            boolean bool = false;
            if (res.size() > 0) {
               bool = ((Boolean)res.elementAt(0)).booleanValue();
            }
            return bool;
        } finally {
            releaseResponseListener (myListener);
        }
        return false; /* this should never be executed */
    }

    /**
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public boolean compare( String DN,
                            LDAPAttribute attr,
                            LDAPSearchConstraints cons ) throws LDAPException {
        return compare( DN, attr, (LDAPConstraints)cons );
    }        
    
    /**
     * Compare an attribute value with one in the directory. The result can 
     * be obtained by calling <CODE>getResultCode</CODE> on the 
     * <CODE>LDAPResponse</CODE> from the <CODE>LDAPResponseQueue</CODE>.
     * The code will be <CODE>LDAPException.COMPARE_TRUE</CODE> or 
     * <CODE>LDAPException.COMPARE_FALSE</CODE>. 
     * 
     * @param dn distinguished name of the entry to compare
     * @param attr attribute with a value to compare
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     */
    public LDAPResponseQueue compare( String dn, 
                                      LDAPAttribute attr, 
                                      LDAPResponseQueue listener )
        throws LDAPException {

        return compare( dn, attr, listener, _defaultConstraints );
    }
    
    /**
     * Compare an attribute value with one in the directory. The result can 
     * be obtained by calling <CODE>getResultCode</CODE> on the 
     * <CODE>LDAPResponse</CODE> from the <CODE>LDAPResponseQueue</CODE>.
     * The code will be <CODE>LDAPException.COMPARE_TRUE</CODE> or 
     * <CODE>LDAPException.COMPARE_FALSE</CODE>. 
     * 
     * @param dn distinguished name of the entry to compare
     * @param attr attribute with a value to compare
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to this operation
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     */
    public LDAPResponseQueue compare( String dn, 
                                      LDAPAttribute attr, 
                                      LDAPResponseQueue listener,
                                      LDAPConstraints cons )
        throws LDAPException {
        if (cons == null) {
            cons = _defaultConstraints;
        }
        
        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseQueue(/*asynchOp=*/true);
        }

        Enumeration en = attr.getStringValues();
        String val = (String)en.nextElement();
        JDAPAVA ava = new JDAPAVA(attr.getName(), val);
        
        sendRequest (new JDAPCompareRequest (dn, ava), listener, cons);
        return listener;
    }
    
    /**
     * Connects to the specified host and port.  If this LDAPConnection object
     * represents an open connection, the connection is closed first
     * before the new connection is opened.
     * <P>
     *
     * For example, the following section of code establishes a connection with
     * the LDAP server running on the host ldap.netscape.com and the port 389.
     * <P>
     *
     * <PRE>
     * String ldapHost = "ldap.netscape.com";
     * int ldapPort = 389;
     * LDAPConnection myConn = new LDAPConnection();
     * try {
     *     myConn.connect( ldapHost, ldapPort );
     * } catch ( LDAPException e ) {
     *     System.out.println( "Unable to connect to " + ldapHost +
     *                         " at port " + ldapPort );
     *     return;
     * }
     * System.out.println( "Connected to " + ldapHost + " at port " + ldapPort )
     * </PRE>
     *<P>
     * You can limit the time spent waiting for the connection to be established
     * by calling <CODE>setConnectTimeout</CODE> before <CODE>connect</CODE>. 
     * <P>
     * @param host host name of the LDAP server to which you want to connect.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). For example, you can specify
     * the following values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * If multiple servers are specified in the <CODE>host</CODE> list, the connection
     *  setup policy specified with the <CODE>ConnSetupDelay</CODE> property controls
     * whether connection attempts are made serially or concurrently.
     * <P>
     * @param port port number of the LDAP server to which you want to connect.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @exception LDAPException The connection failed.
     * @see org.ietf.ldap.LDAPConnection#setConnSetupDelay
     * @see org.ietf.ldap.LDAPConnection#setConnectTimeout
     */
    public void connect(String host, int port) throws LDAPException {
        connect( host, port, null, null, _defaultConstraints, false );
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server.  If this LDAPConnection object
     * represents an open connection, the connection is closed first
     * before the new connection is opened.
     * <P>
     *
     * For example, the following section of code establishes a connection
     * with the LDAP server running on ldap.netscape.com at port 389.  The
     * example also attempts to authenticate the client as Barbara Jensen.
     * <P>
     *
     * <PRE>
     * String ldapHost = "ldap.netscape.com";
     * int ldapPort = 389;
     * String myDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String myPW = "hifalutin";
     * LDAPConnection myConn = new LDAPConnection();
     * try {
     *     myConn.connect( ldapHost, ldapPort, myDN, myPW );
     * } catch ( LDAPException e ) {
     *     switch( e.getResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified user does not exist." );
     *             break;
     *         case e.INVALID_CREDENTIALS:
     *             System.out.println( "Invalid password." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getResultCode() );
     *             System.out.println( "Failed to connect to " + ldapHost + " at port " + ldapPort );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Connected to " + ldapHost + " at port " + ldapPort );
     * </PRE>
     *<P>
     * You can limit the time spent waiting for the connection to be established
     * by calling <CODE>setConnectTimeout</CODE> before <CODE>connect</CODE>. 
     * <P>
     * @param host host name of the LDAP server to which you want to connect.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). For example, you can specify
     * the following values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * If multiple servers are specified in the <CODE>host</CODE> list, the connection
     *  setup policy specified with the <CODE>ConnSetupDelay</CODE> property controls
     * whether connection attempts are made serially or concurrently.
     * <P>
     * @param port port number of the LDAP server to which you want to connect.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @exception LDAPException The connection or authentication failed.
     * @see org.ietf.ldap.LDAPConnection#setConnSetupDelay
     * @see org.ietf.ldap.LDAPConnection#setConnectTimeout
     */
    private void connect(String host, int port, String dn, byte[] passwd)
        throws LDAPException {
        connect(host, port, dn, passwd, _defaultConstraints, true);
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server.  If this LDAPConnection object
     * represents an open connection, the connection is closed first
     * before the new connection is opened. This method allows the user to 
     * specify the preferences for the bind operation.
     *<P>
     * You can limit the time spent waiting for the connection to be established
     * by calling <CODE>setConnectTimeout</CODE> before <CODE>connect</CODE>. 
     * <P>
     * @param host host name of the LDAP server to which you want to connect.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). For example, you can specify
     * the following values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * If multiple servers are specified in the <CODE>host</CODE> list, the connection
     *  setup policy specified with the <CODE>ConnSetupDelay</CODE> property controls
     * whether connection attempts are made serially or concurrently.
     * <P>
     * @param port port number of the LDAP server to which you want to connect.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @param cons preferences for the bind operation
     * @exception LDAPException The connection or authentication failed.
     * @see org.ietf.ldap.LDAPConnection#setConnSetupDelay
     * @see org.ietf.ldap.LDAPConnection#setConnectTimeout
     */
    private void connect(String host, int port, String dn, byte[] passwd,
        LDAPConstraints cons) throws LDAPException {
        connect(host, port, dn, passwd, cons, true);
    }

    private void connect(String host, int port, String dn, byte[] passwd,
      LDAPConstraints cons, boolean doAuthenticate) 
        throws LDAPException {
        if ( isConnected() ) {
            disconnect ();
        }
        if ((host == null) || (host.equals(""))) {
            throw new LDAPException ( "no host for connection",
                                      LDAPException.PARAM_ERROR );
        }

        /* Parse the list of hosts */    
        int defaultPort = port;
        StringTokenizer st = new StringTokenizer( host );
        String hostList[] = new String[st.countTokens()];
        int portList[] = new int[st.countTokens()];
        int i = 0;
        while( st.hasMoreTokens() ) {
            String s = st.nextToken();
            int colon = s.indexOf( ':' );
            if ( colon > 0 ) {
                hostList[i] = s.substring( 0, colon );
                portList[i] = Integer.parseInt( s.substring( colon+1 ) );
            } else {
                hostList[i] = s;
                portList[i] = defaultPort;
            }
            i++;
        }

        /* Create the Connection Setup Manager */
        _connMgr = new LDAPConnSetupMgr(hostList, portList, _factory);
        _connMgr.setConnSetupDelay(_connSetupDelay);
        _connMgr.setConnectTimeout(_connectTimeout);
    
        connect();

        if ( doAuthenticate ) {
            bind( dn, passwd, cons );
        }
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server, with the specified LDAP
     * protocol version. If the server does not support the requested
     * protocol version, an exception is thrown. If this LDAPConnection
     * object represents an open connection, the connection is closed first
     * before the new connection is opened. This is equivalent to
     * <CODE>connect(host, port)</CODE> followed by <CODE>bind(version, dn, passwd)</CODE>.<P>
     *
     * @param version requested version of LDAP: currently 2 or 3
     * @param host a hostname to which to connect or a dotted string representing
     * the IP address of this host.
     * Alternatively, this can be a space-delimited list of host names.
     * Each host name may include a trailing colon and port number. In the
     * case where more than one host name is specified, the connection setup
     * policy specified with the <CODE>ConnSetupDelay</CODE> property controls
     * whether connection attempts are made serially or concurrently.<P>
     *
     * <PRE>
     *   Examples:
     *      "directory.knowledge.com"
     *      "199.254.1.2"
     *      "directory.knowledge.com:1050 people.catalog.com 199.254.1.2"
     * </PRE>
     * @param port the TCP or UDP port number to which to connect or contact.
     * The default LDAP port is 389. "port" is ignored for any host name which
     * includes a colon and port number.
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param passwd if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name and passwd as password.
     * @exception LDAPException The connection or authentication failed.
     * @see org.ietf.ldap.LDAPConnection#setConnSetupDelay
     */
    private void connect(int version, String host, int port, String dn,
        byte[] passwd) throws LDAPException {

        connect(version, host, port, dn, passwd, _defaultConstraints);
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server, with the specified LDAP
     * protocol version. If the server does not support the requested
     * protocol version, an exception is thrown. This method allows the user
     * to specify preferences for the bind operation. If this LDAPConnection
     * object represents an open connection, the connection is closed first
     * before the new connection is opened. This is equivalent to
     * <CODE>connect(host, port)</CODE> followed by <CODE>bind(version, dn, passwd)</CODE>.<P>
     *
     * @param version requested version of LDAP: currently 2 or 3
     * @param host a hostname to which to connect or a dotted string representing
     * the IP address of this host.
     * Alternatively, this can be a space-delimited list of host names.
     * Each host name may include a trailing colon and port number. In the
     * case where more than one host name is specified, the connection setup
     * policy specified with the <CODE>ConnSetupDelay</CODE> property controls
     * whether connection attempts are made serially or concurrently.<P>
     *
     * <PRE>
     *   Examples:
     *      "directory.knowledge.com"
     *      "199.254.1.2"
     *      "directory.knowledge.com:1050 people.catalog.com 199.254.1.2"
     * </PRE>
     * @param port the TCP or UDP port number to which to connect or contact.
     * The default LDAP port is 389. "port" is ignored for any host name which
     * includes a colon and port number.
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param passwd if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name and passwd as password
     * @param cons preferences for the bind operation
     * @exception LDAPException The connection or authentication failed.
     * @see org.ietf.ldap.LDAPConnection#setConnSetupDelay
     */
    private void connect(int version, String host, int port, String dn,
        byte[] passwd, LDAPConstraints cons) throws LDAPException {

        setProtocolVersion(version);
        connect(host, port, dn, passwd, cons);
    }

    /**
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    private void connect(int version, String host, int port, String dn,
        byte[] passwd, LDAPSearchConstraints cons) throws LDAPException {

        connect(version, host, port, dn, passwd, (LDAPConstraints)cons);
    }

    /**
     * Internal routine to connect with internal params
     * @exception LDAPException failed to connect
     */
    private synchronized void connect () throws LDAPException {
        if ( isConnected() ) {
            return;
        }

        if (_connMgr == null) {
            throw new LDAPException ( "no connection parameters",
                                      LDAPException.PARAM_ERROR );
        }        

        _connMgr.openConnection();
        _thread = getNewThread(_connMgr, _cache);
        authenticateSSLConnection();
    }

    /**
     * Deletes the entry for the specified DN from the directory. <P>
     *
     * For example, the following section of code deletes the entry for
     * Barbara Jensen from the directory. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * myConn.delete( myEntryDN );
     * ... </PRE>
     *
     * @param DN distinguished name identifying the entry
     * to remove from the directory
     * @exception LDAPException Failed to delete the specified entry from
     * the directory.
     */
    public void delete( String DN ) throws LDAPException {
        delete(DN, _defaultConstraints);
    }

    /**
     * Deletes the entry for the specified DN from the directory and
     * allows you to specify preferences for this LDAP delete operation
     * by using an <CODE>LDAPConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN distinguished name identifying the entry 
     * to remove from the directory
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to delete the specified entry from
     * the directory.
     * @see org.ietf.ldap.LDAPConstraints
     */
    public void delete( String DN, LDAPConstraints cons )
        throws LDAPException {
        internalBind (cons);

        LDAPResponseQueue myListener = getResponseListener ();
        LDAPMessage response;
        try {
            sendRequest (new JDAPDeleteRequest (DN), myListener, cons);
            response = myListener.getResponse();
            checkMsg (response);
        } catch (LDAPReferralException e) {
            performReferrals(e, cons, JDAPProtocolOp.DEL_REQUEST,
                             DN, 0, null, null, false, null, null, null, null);
        } finally {
            releaseResponseListener (myListener);
        }
    }

    /**
     * Deletes the entry for the specified DN from the directory.
     * 
     * @param dn distinguished name of the entry to delete
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     * @see org.ietf.ldap.LDAPConstraints
     */
    public LDAPResponseQueue delete( String dn,
                                     LDAPResponseQueue listener )
        throws LDAPException {
        
        return delete( dn, listener, _defaultConstraints );
    }

    /**
     * Deletes the entry for the specified DN from the directory.
     * 
     * @param dn distinguished name of the entry to delete
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     * @see org.ietf.ldap.LDAPConstraints
     */
    public LDAPResponseQueue delete( String dn,
                                     LDAPResponseQueue listener,
                                     LDAPConstraints cons )
        throws LDAPException {
        if (cons == null) {
            cons = _defaultConstraints;
        }

        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseQueue(/*asynchOp=*/true);
        }
        
        sendRequest (new JDAPDeleteRequest(dn), listener, cons);
        
        return listener;

    }
    
    /**
     * Disconnects from the LDAP server. Before you can perform LDAP operations
     * again, you need to reconnect to the server by calling
     * <CODE>connect</CODE>.
     * @exception LDAPException Failed to disconnect from the LDAP server.
     * @see org.ietf.ldap.LDAPConnection#connect(java.lang.String, int)
     */
    public synchronized void disconnect() throws LDAPException {
        if (!isConnected())
            throw new LDAPException ( "unable to disconnect() without connecting",
                                      LDAPException.OTHER );
        
        // Clone the Connection Setup Manager if the connection is shared
        if (_thread.isRunning() && _thread.getClientCount() > 1) {
            _connMgr = (LDAPConnSetupMgr) _connMgr.clone();
            _connMgr.disconnect();
        }

        if (_referralConnection != null && _referralConnection.isConnected()) {
            _referralConnection.disconnect();
        }
        _referralConnection = null;

        if (_cache != null) {
            _cache.removeReference();
            _cache = null;
        }

        deleteThreadConnEntry();
        deregisterConnection();
    }

    /**
     * Performs an extended operation on the directory.  Extended operations
     * are part of version 3 of the LDAP protocol.<P>
     *
     * Note that in order for the extended operation to work, the server
     * that you are connecting to must support LDAP v3 and must be configured
     * to process the specified extended operation.
     *
     * @param op LDAPExtendedOperation object specifying the OID of the
     * extended operation and the data to use in the operation
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see org.ietf.ldap.LDAPExtendedOperation
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op )
        throws LDAPException {

        return extendedOperation(op, _defaultConstraints);
    }

    /**
     * Performs an extended operation on the directory.  Extended operations
     * are part of version 3 of the LDAP protocol. This method allows the
     * user to set the preferences for the operation.<P>
     *
     * Note that in order for the extended operation to work, the server
     * that you are connecting to must support LDAP v3 and must be configured
     * to process the specified extended operation.
     *
     * @param op LDAPExtendedOperation object specifying the OID of the
     * extended operation and the data to use in the operation
     * @param cons preferences for the extended operation
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see org.ietf.ldap.LDAPExtendedOperation
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op,
                                                    LDAPConstraints cons )
        throws LDAPException {
        internalBind (cons);

        LDAPResponseQueue myListener = getResponseListener ();
        LDAPMessage response = null;
        byte[] results = null;
        String resultID;

        try {
            sendRequest ( new JDAPExtendedRequest( op.getID(),
                                                   op.getValue() ),
                          myListener, cons );
            response = myListener.getResponse();
            checkMsg (response);
            JDAPExtendedResponse res = (JDAPExtendedResponse)response.getProtocolOp();
            results = res.getValue();
            resultID = res.getID();
        } catch (LDAPReferralException e) {
            return performExtendedReferrals( e, cons, op );
        } finally {
            releaseResponseListener (myListener);
        }
        return new LDAPExtendedOperation( resultID, results );
    }

    /**
     * Performs an extended operation on the directory.  Extended operations
     * are part of version 3 of the LDAP protocol.<P>
     *
     * Note that in order for the extended operation to work, the server
     * that you are connecting to must support LDAP v3 and must be configured
     * to process the specified extended operation.
     *
     * @param op LDAPExtendedOperation object specifying the OID of the
     * extended operation and the data to use in the operation
     * @param queue handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see org.ietf.ldap.LDAPExtendedOperation
     */
    public LDAPResponseQueue extendedOperation( 
        LDAPExtendedOperation op, 
        LDAPResponseQueue queue )
        throws LDAPException {
        return extendedOperation( op, queue, _defaultConstraints );
    }
    
    /**
     * Performs an extended operation on the directory.  Extended operations
     * are part of version 3 of the LDAP protocol.<P>
     *
     * Note that in order for the extended operation to work, the server
     * that you are connecting to must support LDAP v3 and must be configured
     * to process the specified extended operation.
     *
     * @param op LDAPExtendedOperation object specifying the OID of the
     * extended operation and the data to use in the operation
     * @param queue handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see org.ietf.ldap.LDAPExtendedOperation
     */
    public LDAPResponseQueue extendedOperation( 
        LDAPExtendedOperation op, 
        LDAPResponseQueue queue,
        LDAPConstraints cons )
        throws LDAPException {
        return null;
    }

    /**
     * Finalize method, which disconnects from the LDAP server
     * @exception LDAPException Thrown on error in disconnecting
     */
    protected void finalize() throws LDAPException {
        if (isConnected()) {
            disconnect();
        }
    }

    /**
     * Returns the distinguished name (DN) used for authentication over
     * this connection.
     * @return distinguished name used for authentication over this connection.
     */
    public String getAuthenticationDN () {
        return _boundDN;
    }

    /**
     * Returns the password used for authentication over this connection.
     * @return password used for authentication over this connection.
     */
    byte[] getAuthenticationPassword () {
        return _boundPasswd;
    }
    

    /**
     * Gets the authentication method used to bind:<BR>
     * "none", "simple", or "sasl"
     *
     * @return the authentication method, or "none"
     */
    public String getAuthenticationMethod() {
        return isConnected() ? _authMethod : "none";
    }

    /**
     * Gets the <CODE>LDAPCache</CODE> object associated with
     * the current <CODE>LDAPConnection</CODE> object.
     * <P>
     *
     * @return the <CODE>LDAPCache</CODE> object representing
     * the cache that the current connection should use
     * @see org.ietf.ldap.LDAPCache
     * @see org.ietf.ldap.LDAPConnection#setCache(org.ietf.ldap.LDAPCache)
     */
    public LDAPCache getCache() {
        return _cache;
    }

    /**
     * Returns the maximum time to wait for the connection to be established.
     * @return the maximum connect time in seconds or 0 (unlimited)
     * @see org.ietf.ldap.LDAPConnection#setConnectTimeout
     */
    public int getConnectTimeout () {
        return _connectTimeout;
    }

    /**
     * Returns the delay in seconds when making concurrent connection attempts to
     * multiple servers.
     * @return the delay in seconds between connection attempts:<br>
     * <CODE>NODELAY_SERIAL</CODE> The serial connection setup policy is enabled
     * (no concurrency).<br>
     * <CODE>NODELAY_PARALLEL</CODE> The parallel connection setup policy with no delay
     *  is enabled.<br>
     * <CODE>delay > 0</CODE> The parallel connection setup policy with the delay of 
     * <CODE>delay</CODE> seconds is enabled.
     * @see org.ietf.ldap.LDAPConnection#setConnSetupDelay
     */
    public int getConnSetupDelay () {
        return _connSetupDelay;
    }

    /**
     * Returns the set of constraints that apply to all operations
     * performed through this connection (unless you specify a different
     * set of constraints when calling a method). 
     * <P>
     *
     * Note that if you want to get individual constraints (rather than
     * getting the
     * entire set of constraints), call the <CODE>getOption</CODE> method.
     * <P>
     *
     * Typically, you might call the <CODE>getConstraints</CODE> method
     * to create a slightly different set of constraints for a particular 
     * operation.
     * <P>
     *
     * For example, the following section of code changes the timeout
     * to 3000 milliseconds for a specific rename. Rather than construct a new
     * set of constraints from scratch, the example gets the current
     * settings for the connections and just changes the setting for the
     * timeout.
     * <P>
     *
     * Note that this change only applies to the searches performed with this
     * custom set of constraints.  All other searches performed through this
     * connection use the original set of search constraints.
     * <P>
     *
     * <PRE>
     * ...
     * LDAPConstraints myOptions = ld.getConstraints();
     * myOptions.setTimeout( 3000 );
     * ld.search( "cn=William Jensen, ou=Accounting, o=Ace Industry,c=US",
     *            "cn=Will Jensen",
     *            null,
     *            false,
     *            myOptions );
     * ...
     * </PRE>
     *
     * @return a copy of the <CODE>LDAPConstraints</CODE> object representing the
     * set of constraints that apply (by default) to all operations
     * performed through this connection.
     * @see org.ietf.ldap.LDAPConstraints
     * @see org.ietf.ldap.LDAPConnection#getOption
     */
    public LDAPConstraints getConstraints () {
        return (LDAPConstraints)getSearchConstraints();
    }
   
    /**
     * Returns the host name of the LDAP server to which you are connected.
     * @return host name of the LDAP server.
     */
    public String getHost () {
        if (_connMgr != null) {
            return _connMgr.getHost();
        }
        return null;
    }

    /**
     * Gets the stream for reading from the listener socket
     *
     * @return the stream for reading from the listener socket, or
     * <CODE>null</CODE> if there is none
     */
    public InputStream getInputStream() {
        return (_thread != null) ? _thread.getInputStream() : null;
    }

    /**
     * Gets the stream for writing to the socket
     *
     * @return the stream for writing to the socket, or
     * <CODE>null</CODE> if there is none
     */
    public OutputStream getOutputStream() {
        return (_thread != null) ? _thread.getOutputStream() : null;
    }

    /**
     * Returns the port number of the LDAP server to which you are connected.
     * @return port number of the LDAP server.
     */
    public int getPort () {
        if (_connMgr != null) {
            return _connMgr.getPort();
        }
        return -1;
    }

    /**
     * Gets a property of a connection. <P>
     *
     * You can get the following properties for a given connection: <P>
     * <UL TYPE="DISC">
     * <LI><CODE>LDAP_PROPERTY_SDK</CODE> <P>
     * To get the version of this SDK, get this property.  The value of
     * this property is of the type <CODE>Float</CODE>. <P>
     * <LI><CODE>LDAP_PROPERTY_PROTOCOL</CODE> <P>
     * To get the highest supported version of the LDAP protocol, get
     * this property.
     * The value of this property is of the type <CODE>Float</CODE>. <P>
     * <LI><CODE>LDAP_PROPERTY_SECURITY</CODE> <P>
     * To get a comma-separated list of the types of authentication
     * supported, get this property.  The value of this property is of the
     * type <CODE>String</CODE>. <P>
     * </UL>
     * <P>
     *
     * For example, the following section of code gets the version of
     * the SDK.<P>
     *
     * <PRE>
     *       ...
     *       Float sdkVersion = ( Float )myConn.getProperty( myConn.LDAP_PROPERTY_SDK );
     *       System.out.println( "SDK version: " + sdkVersion );
     *       ... </PRE>
     *
     * @param name name of the property (for example, <CODE>LDAP_PROPERTY_SDK</CODE>) <P>
     *
     * @return the value of the property. <P>
     *
     * Since the return value is an object, you should recast it as the appropriate type.
     * (For example, when getting the <CODE>LDAP_PROPERTY_SDK</CODE> property,
     * recast the return value as a <CODE>Float</CODE>.) <P>
     *
     * If you pass this method an unknown property name, the method
     * returns null. <P>
     *
     * @exception LDAPException Unable to get the value of the
     * specified property. <P>
     *
     * @see org.ietf.ldap.LDAPConnection#LDAP_PROPERTY_SDK
     * @see org.ietf.ldap.LDAPConnection#LDAP_PROPERTY_PROTOCOL
     * @see org.ietf.ldap.LDAPConnection#LDAP_PROPERTY_SECURITY
     */
    public Object getProperty(String name) throws LDAPException {
        return _properties.get( name );
    }

    /**
     * Returns the protocol version that the connection is bound to (which 
     * currently is 3). If the connection is not bound, it returns 3.
     *
     * @return the protocol version that the connection is bound to
     */
    public int getProtocolVersion() {
        return _protocolVersion;
    }

    /**
     * Returns an array of the latest controls (if any) from server.
     * <P>
     * To retrieve the controls from a search result, call the 
     * <CODE>getResponseControls</CODE> method from the <CODE>LDAPSearchResults
     * </CODE> object returned with the result.
     * @return an array of the controls returned by an operation, or
     * null if none.
     * @see org.ietf.ldap.LDAPControl
     * @see org.ietf.ldap.LDAPSearchResults#getResponseControls
     */
    public LDAPControl[] getResponseControls() {
      LDAPControl[] controls = null;

      /* Get the latest controls returned for our thread */
      synchronized(_responseControlTable) {
          Vector responses = (Vector)_responseControlTable.get(_thread);

          if (responses != null) {
              // iterate through each response control
              for (int i=0,size=responses.size(); i<size; i++) {
                  LDAPResponseControl responseObj =
                    (LDAPResponseControl)responses.elementAt(i);

                  // check if the response belongs to this connection
                  if (responseObj.getConnection().equals(this)) {
                      controls = responseObj.getControls();
                      responses.removeElementAt(i);
                      break;
                  }
              }
          }
      }
      
      return controls;
    }

    /**
     * Returns an array of the latest controls associated with the 
     * particular request. Used internally by LDAPSearchResults to
     * get response controls returned for a search request.
     * <P>
     * @param msdid Message ID
     */
    LDAPControl[] getResponseControls(int msgID) {
      LDAPControl[] controls = null;

      /* Get the latest controls returned for our thread */
      synchronized(_responseControlTable) {
          Vector responses = (Vector)_responseControlTable.get(_thread);

          if (responses != null) {
              // iterate through each response control
              for (int i=0,size=responses.size(); i<size; i++) {
                  LDAPResponseControl responseObj =
                    (LDAPResponseControl)responses.elementAt(i);

                  // check if the response matches msgID
                  if (responseObj.getMsgID() == msgID) {
                      controls = responseObj.getControls();
                      responses.removeElementAt(i);
                      break;
                  }
              }
          }
      }
      
      return controls;
    }

    /**
     * Returns the callback handler, if any, specified on binding with a 
     * SASL mechanism
     *
     * @return the callback handler, if any, specified on binding with a 
     * SASL mechanism
     */
    public CallbackHandler getSaslBindCallbackHandler() {
        return _saslCallbackHandler;
    }

    /**
     * Returns the properties, if any, specified on binding with a 
     * SASL mechanism
     *
     * @return the properties, if any, specified on binding with a 
     * SASL mechanism
     */
    public Map getSaslBindProperties() {
        return _securityProperties;
    }

    /**
     * Returns the set of search constraints that apply to all searches
     * performed through this connection (unless you specify a different
     * set of search constraints when calling the <CODE>search</CODE>
     * method). 
     * <P>
     *
     * Note that if you want to get individual constraints (rather than
     * getting the
     * entire set of constraints), call the <CODE>getOption</CODE> method.
     * <P>
     *
     * Typically, you might call the <CODE>getSearchConstraints</CODE> method
     * to create a slightly different set of search constraints
     * to apply to a particular search.
     * <P>
     *
     * For example, the following section of code changes the maximum number
     * of results to 10 for a specific search. Rather than construct a new
     * set of search constraints from scratch, the example gets the current
     * settings for the connections and just changes the setting for the
     * maximum results.
     * <P>
     *
     * Note that this change only applies to the searches performed with this
     * custom set of constraints.  All other searches performed through this
     * connection use the original set of search constraints.
     * <P>
     *
     * <PRE>
     * ...
     * LDAPSearchConstraints myOptions = ld.getSearchConstraints();
     * myOptions.setMaxResults( 10 );
     * String[] myAttrs = { "objectclass" };
     * LDAPSearchResults myResults = ld.search( "o=Ace Industry,c=US",
     *                                          LDAPConnection.SCOPE_SUB,
     *                                          "(objectclass=*)",
     *                                          myAttrs,
     *                                          false,
     *                                          myOptions );
     * ...
     * </PRE>
     *
     * @return a copy of the <CODE>LDAPSearchConstraints</CODE> object 
     * representing the set of search constraints that apply (by default) to 
     * all searches performed through this connection.
     * @see org.ietf.ldap.LDAPSearchConstraints
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, org.ietf.ldap.LDAPSearchConstraints)  
     */
    public LDAPSearchConstraints getSearchConstraints () {
        return (LDAPSearchConstraints)_defaultConstraints.clone();
    }

    /**
     * Gets the object representing the socket factory used to establish
     * a connection to the LDAP server.
     * <P>
     *
     * @return the object representing the socket factory used to
     * establish a connection to a server.
     * @see org.ietf.ldap.LDAPSocketFactory
     * @see org.ietf.ldap.LDAPSSLSocketFactory
     * @see org.ietf.ldap.LDAPConnection#setSocketFactory(org.ietf.ldap.LDAPSocketFactory)
     */
    public LDAPSocketFactory getSocketFactory () {
        return _factory;
    }

    /**
     * Indicates whether this client has authenticated to the LDAP server
     * (other than anonymously with simple bind)
     *
     * @return <CODE>false</CODE> initially, <CODE>false</CODE> upon a bind
     * request, and <CODE>true</CODE> after successful completion of the last
     * outstanding non-anonymous simple bind
     */
    public boolean isBound() {
        if (_bound) {
            if ((_boundDN == null) || _boundDN.equals("") ||
                (_boundPasswd == null) || (_boundPasswd.length < 1)) {
                return false;
            }
        }
        return _bound;
    }

    /**
     * Indicates if the connection represented by this object
     * is open at this time
     *
     * @return <CODE>true</CODE> if connected to an LDAP server over this
     * connection.
     * If not connected to an LDAP server, returns <CODE>false</CODE>.
     */
    public boolean isConnected() {
        // This is the hack: If the user program calls isConnected() when
        // the thread is about to shut down, the isConnected might get called
        // before the deregisterConnection(). We add the yield() so that 
        // the deregisterConnection() will get called first. 
        // This problem only exists on Solaris.
        Thread.yield();
        return (_thread != null);
    }

    /**
     * Indicates if the connection is currently protected by TLS
     *
     * @return <CODE>true</CODE> if the connection is currently protected
     * by TLS; if not connected to an LDAP server, returns <CODE>false</CODE>.
     */
    public boolean isTLS() {
        return false;
    }

    /**
     * Makes a single change to an existing entry in the directory
     * (for example, changes the value of an attribute, adds a new
     * attribute value, or removes an existing attribute value). <P>
     *
     * Use the <CODE>LDAPModification</CODE> object to specify the change
     * to make and the <CODE>LDAPAttribute</CODE> object
     * to specify the attribute value to change. The
     * <CODE>LDAPModification</CODE> object allows you add an attribute
     * value, change an attibute value, or remove an attribute
     * value. <P>
     *
     * For example, the following section of code changes Barbara Jensen's email
     * address in the directory to babs@aceindustry.com. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     *
     * LDAPAttribute attrEmail = new LDAPAttribute( "mail", "babs@aceindustry.com" );
     * LDAPModification singleChange = new LDAPModification( LDAPModification.REPLACE, attrEmail );
     *
     * myConn.modify( myEntryDN, singleChange );
     * ... </PRE>
     *
     * @param DN the distinguished name of the entry to modify
     * @param mod a single change to make to the entry
     * @exception LDAPException Failed to make the specified change to the
     * directory entry.
     * @see org.ietf.ldap.LDAPModification
     */
    public void modify( String DN, LDAPModification mod ) throws LDAPException {
        modify( DN, mod, _defaultConstraints );
    }

   /**
    * Makes a single change to an existing entry in the directory and
    * allows you to specify preferences for this LDAP modify operation
    * by using an <CODE>LDAPConstraints</CODE> object. For
    * example, you can specify whether or not to follow referrals.
    * You can also apply LDAP v3 controls to the operation.
    * <P>
    *
    * @param DN the distinguished name of the entry to modify
    * @param mod a single change to make to the entry
    * @param cons the set of preferences to apply to this operation
    * @exception LDAPException Failed to make the specified change to the
    * directory entry.
    * @see org.ietf.ldap.LDAPModification
    * @see org.ietf.ldap.LDAPConstraints
    */
    public void modify( String DN, LDAPModification mod,
                        LDAPConstraints cons ) throws LDAPException {
        LDAPModification[] mods = { mod };
        modify( DN, mods, cons );
    }

    /**
     * Makes a set of changes to an existing entry in the directory
     * (for example, changes attribute values, adds new attribute values,
     * or removes existing attribute values). <P>
     *
     * Use an array of <CODE>LDAPModification</CODE> objects to specify the
     * changes to make.  Each change must be specified by
     * an <CODE>LDAPModification</CODE> object, and you must specify each 
     * attribute value to modify, add, or remove by an <CODE>LDAPAttribute</CODE> 
     * object. <P>
     *
     * @param DN the distinguished name of the entry to modify
     * @param mods an array of objects representing the changes to make
     * to the entry
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see org.ietf.ldap.LDAPModification
     */
    public void modify( String DN, LDAPModification[] mods )
        throws LDAPException {
        modify( DN, mods, _defaultConstraints );
    }

    /**
     * Makes a set of changes to an existing entry in the directory and
     * allows you to specify preferences for this LDAP modify operation
     * by using an <CODE>LDAPConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN the distinguished name of the entry to modify
     * @param mods an array of objects representing the changes to make
     * to the entry
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see org.ietf.ldap.LDAPModification
     * @see org.ietf.ldap.LDAPConstraints
     */
    public void modify( String DN, LDAPModification[] mods,
                        LDAPConstraints cons ) throws LDAPException {
         internalBind (cons);

         LDAPResponseQueue myListener = getResponseListener ();
         LDAPMessage response = null;
         try {
             sendRequest (new JDAPModifyRequest (DN, mods), myListener, cons);
             response = myListener.getResponse();
             checkMsg (response);
         } catch (LDAPReferralException e) {
             performReferrals(e, cons, JDAPProtocolOp.MODIFY_REQUEST,
                              DN, 0, null, null, false, mods, null, null, null);
         } finally {
             releaseResponseListener (myListener);
         }
    }

    /**
     * Makes a single change to an existing entry in the directory. For
     * example, it changes the value of an attribute, adds a new attribute
     * value, or removes an existing attribute value). <BR>
     * The LDAPModification object specifies both the change to make and
     * the LDAPAttribute value to be changed.
     * 
     * @param dn distinguished name of the entry to modify
     * @param mod a single change to make to an entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPModification
     * @see org.ietf.ldap.LDAPResponseQueue
     */
    public LDAPResponseQueue modify( String dn,
                                     LDAPModification mod,
                                     LDAPResponseQueue listener )
        throws LDAPException{

        return modify( dn, mod, listener, _defaultConstraints );
    }
    
    /**
     * Makes a single change to an existing entry in the directory. For
     * example, it changes the value of an attribute, adds a new attribute
     * value, or removes an existing attribute value). <BR>
     * The LDAPModification object specifies both the change to make and
     * the LDAPAttribute value to be changed.
     * 
     * @param dn distinguished name of the entry to modify
     * @param mod a single change to make to an entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPModification
     * @see org.ietf.ldap.LDAPResponseQueue
     * @see org.ietf.ldap.LDAPConstraints
     */
    public LDAPResponseQueue modify( String dn,
                                     LDAPModification mod,
                                     LDAPResponseQueue listener,
                                     LDAPConstraints cons )
        throws LDAPException{
        if (cons == null) {
            cons = _defaultConstraints;
        }

        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseQueue(/*asynchOp=*/true);
        }

        LDAPModification[] modList = { mod };
        sendRequest (new JDAPModifyRequest (dn, modList), listener, cons);        

        return listener;
    }

    /**
     * Reads the entry for the specified distiguished name (DN) and retrieves
     * all attributes for the entry.
     * <P>
     *
     * For example, the following section of code reads the entry for
     * Barbara Jensen and retrieves all attributes for that entry.
     * <P>
     *
     * <PRE>
     * String findDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * LDAPEntry foundEntry = null;
     * try {
     *     foundEntry = myConn.read( findDN );
     * } catch ( LDAPException e ) {
     *     switch( e.getResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified entry does not exist." );
     *             break;
     *         case e.LDAP_PARTIAL_RESULTS:
     *             System.out.println( "Entry served by a different LDAP server." );
     *             break;
     *         case e.INSUFFICIENT_ACCESS_RIGHTS:
     *             System.out.println( "You do not have the access rights to perform this operation." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getResultCode() );
     *             System.out.println( "Could not read the specified entry." );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Found the specified entry." );
     * </PRE>
     *
     * @param DN distinguished name of the entry  to retrieve
     * @exception LDAPException Failed to find or read the specified entry
     * from the directory.
     * @return LDAPEntry returns the specified entry or raises an exception
     * if the entry is not found.
     */
    public LDAPEntry read( String DN ) throws LDAPException {
        return read( DN, null, _defaultConstraints );
    }

    /**
     * Reads the entry for the specified distiguished name (DN) and retrieves all
     * attributes for the entry. This method allows the user to specify the
     * preferences for the read operation.
     * <P>
     *
     * For example, the following section of code reads the entry for
     * Barbara Jensen and retrieves all attributes for that entry.
     * <P>
     *
     * <PRE>
     * String findDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * LDAPEntry foundEntry = null;
     * try {
     *     foundEntry = myConn.read( findDN );
     * } catch ( LDAPException e ) {
     *     switch( e.getResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified entry does not exist." );
     *             break;
     *         case e.LDAP_PARTIAL_RESULTS:
     *             System.out.println( "Entry served by a different LDAP server." );
     *             break;
     *         case e.INSUFFICIENT_ACCESS_RIGHTS:
     *             System.out.println( "You do not have the access rights to perform this operation." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getResultCode() );
     *             System.out.println( "Could not read the specified entry." );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Found the specified entry." );
     * </PRE>
     *
     * @param DN distinguished name of the entry to retrieve
     * @param cons preferences for the read operation
     * @exception LDAPException Failed to find or read the specified entry
     * from the directory.
     * @return LDAPEntry returns the specified entry or raises an exception
     * if the entry is not found.
     */
    public LDAPEntry read( String DN, LDAPSearchConstraints cons )
        throws LDAPException {
        return read( DN, null, cons );
    }

    /**
     * Reads the entry for the specified distinguished name (DN) and
     * retrieves only the specified attributes from the entry.
     * <P>
     *
     * For example, the following section of code reads the entry for
     * Barbara Jensen and retrieves only the <CODE>cn</CODE> and
     * <CODE>sn</CODE> attributes.
     * The example prints out all attributes that have been retrieved
     * (the two specified attributes).
     * <P>
     *
     * <PRE>
     * String findDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * LDAPEntry foundEntry = null;
     * String getAttrs[] = { "cn", "sn" };
     * try {
     *      foundEntry = myConn.read( findDN, getAttrs );
     * } catch ( LDAPException e ) {
     *      switch( e.getResultCode() ) {
     *           case e.NO_SUCH_OBJECT:
     *               System.out.println( "The specified entry does not exist." );
     *               break;
     *           case e.LDAP_PARTIAL_RESULTS:
     *               System.out.println( "Entry served by a different LDAP server." );
     *               break;
     *           case e.INSUFFICIENT_ACCESS_RIGHTS:
     *               System.out.println( "You do not have the access " +
     *                                   "rights to perform this operation." );
     *               break;
     *           default:
     *               System.out.println( "Error number: " + e.getResultCode() );
     *               System.out.println( "Could not read the specified entry." );
     *               break;
     *      }
     *      return;
     * }
     *
     * LDAPAttributeSet foundAttrs = foundEntry.getAttributeSet();
     * int size = foundAttrs.size();
     * Iterator enumAttrs = foundAttrs.iterator();
     * System.out.println( "Attributes: " );
     *
     * while ( enumAttrs.hasMore() ) {
     *      LDAPAttribute anAttr = ( LDAPAttribute )enumAttrs.next();
     *      String attrName = anAttr.getName();
     *      System.out.println( "\t" + attrName );
     *      Enumeration enumVals = anAttr.getStringValues();
     *      while ( enumVals.hasMoreElements() ) {
     *           String aVal = ( String )enumVals.nextElement();
     *           System.out.println( "\t\t" + aVal );
     *      }
     * }
     * </PRE>
     *
     * @param DN distinguished name of the entry to retrieve
     * @param attrs names of attributes to retrieve
     * @return LDAPEntry returns the specified entry (or raises an
     * exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     */
    public LDAPEntry read( String DN, String attrs[] ) throws LDAPException {
        return read( DN, attrs, _defaultConstraints );
    }

    /**
     * Reads the entry for the specified distinguished name (DN) and
     * retrieves only the specified attributes from the entry.
     * <P>
     *
     * For example, the following section of code reads the entry for
     * Barbara Jensen and retrieves only the <CODE>cn</CODE> and
     * <CODE>sn</CODE> attributes.
     * The example prints out all attributes that have been retrieved
     * (the two specified attributes).
     * <P>
     *
     * <PRE>
     * String findDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * LDAPEntry foundEntry = null;
     * String getAttrs[] = { "cn", "sn" };
     * try {
     *      foundEntry = myConn.read( findDN, getAttrs );
     * } catch ( LDAPException e ) {
     *      switch( e.getResultCode() ) {
     *           case e.NO_SUCH_OBJECT:
     *               System.out.println( "The specified entry does not exist." );
     *               break;
     *           case e.LDAP_PARTIAL_RESULTS:
     *               System.out.println( "Entry served by a different LDAP server." );
     *               break;
     *           case e.INSUFFICIENT_ACCESS_RIGHTS:
     *               System.out.println( "You do not have the access " +
     *                                   "rights to perform this operation." );
     *               break;
     *           default:
     *               System.out.println( "Error number: " + e.getResultCode() );
     *               System.out.println( "Could not read the specified entry." );
     *               break;
     *      }
     *      return;
     * }
     *
     * LDAPAttributeSet foundAttrs = foundEntry.getAttributeSet();
     * int size = foundAttrs.size();
     * Iterator enumAttrs = foundAttrs.iterator();
     * System.out.println( "Attributes: " );
     *
     * while ( enumAttrs.hasMore() ) {
     *      LDAPAttribute anAttr = ( LDAPAttribute )enumAttrs.next();
     *      String attrName = anAttr.getName();
     *      System.out.println( "\t" + attrName );
     *      Enumeration enumVals = anAttr.getStringValues();
     *      while ( enumVals.hasMoreElements() ) {
     *           String aVal = ( String )enumVals.nextElement();
     *           System.out.println( "\t\t" + aVal );
     *      }
     * }
     * </PRE>
     *
     * @param DN distinguished name of the entry to retrieve
     * @param attrs names of attributes to retrieve
     * @param cons preferences for the read operation
     * @return LDAPEntry returns the specified entry (or raises an
     * exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     */
    public LDAPEntry read( String DN, String attrs[],
                           LDAPSearchConstraints cons ) throws LDAPException {
        LDAPSearchResults results =
            search (DN, SCOPE_BASE,
                    "(|(objectclass=*)(objectclass=ldapsubentry))",
                    attrs, false, cons);
        if ( results == null ) {
            return null;
        }
        LDAPEntry entry = results.next();
        
        // cleanup required for referral connections
        while( results.hasMore() ) {
            results.next();
        }
        
        return entry;
    }

    /**
     * Reads the entry specified by the LDAP URL. <P>
     *
     * When you call this method, a new connection is created automatically,
     * using the host and port specified in the URL. After finding the entry,
     * the method closes this connection (in other words, it disconnects from
     * the LDAP server). <P>
     *
     * If the URL specifies a filter and scope, these are not used.
     * Of the information specified in the URL, this method only uses
     * the LDAP host name and port number, the base distinguished name (DN),
     * and the list of attributes to return. <P>
     *
     * The method returns the entry specified by the base DN. <P>
     *
     * (Note: If you want to search for more than one entry, use the
     * <CODE>search( LDAPUrl )</CODE> method instead.) <P>
     *
     * For example, the following section of code reads the entry specified
     * by the LDAP URL.
     * <P>
     *
     * <PRE>
     * String flatURL = "ldap://alway.mcom.com:3890/cn=Barbara Jenson,ou=Product Development,o=Ace Industry,c=US?cn,sn,mail";
     * LDAPUrl myURL;
     * try {
     *    myURL = new LDAPUrl( flatURL );
     * } catch ( java.net.MalformedURLException e ) {
     *    System.out.println( "BAD URL!!!  BAD, BAD, BAD URL!!!" );
     *    return;
     * }
     * LDAPEntry myEntry = null;
     * try {
     *    myEntry = myConn.read( myURL );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getResultCode();
     *    switch( errCode ) {
     *        case ( e.NO_SUCH_OBJECT ):
     *            System.out.println( "The specified entry " + myDN +
     *                                " does not exist in the directory." );
     *            return;
     *        default:
     *            System.out.println( "An internal error occurred." );
     *            return;
     *    }
     * }
     * </PRE>
     *
     * @param toGet LDAP URL specifying the entry to read
     * @return LDAPEntry returns the entry specified by the URL (or raises
     * an exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     * @see org.ietf.ldap.LDAPUrl
     * @see org.ietf.ldap.LDAPConnection#search(org.ietf.ldap.LDAPUrl)
     */
    public static LDAPEntry read( LDAPUrl toGet ) throws LDAPException {
        return read( toGet, null );
    }

    /**
     * Reads the entry specified by the LDAP URL. <P>
     *
     * When you call this method, a new connection is created automatically,
     * using the host and port specified in the URL. After finding the entry,
     * the method closes this connection (in other words, it disconnects from
     * the LDAP server). <P>
     *
     * If the URL specifies a filter and scope, these are not used.
     * Of the information specified in the URL, this method only uses
     * the LDAP host name and port number, the base distinguished name (DN),
     * and the list of attributes to return. <P>
     *
     * The method returns the entry specified by the base DN. <P>
     *
     * (Note: If you want to search for more than one entry, use the
     * <CODE>search( LDAPUrl )</CODE> method instead.) <P>
     *
     * For example, the following section of code reads the entry specified
     * by the LDAP URL.
     * <P>
     *
     * <PRE>
     * String flatURL = "ldap://alway.mcom.com:3890/cn=Barbara Jenson,ou=Product Development,o=Ace Industry,c=US?cn,sn,mail";
     * LDAPUrl myURL;
     * try {
     *    myURL = new LDAPUrl( flatURL );
     * } catch ( java.net.MalformedURLException e ) {
     *    System.out.println( "BAD URL!!!  BAD, BAD, BAD URL!!!" );
     *    return;
     * }
     * LDAPEntry myEntry = null;
     * try {
     *    myEntry = myConn.read( myURL );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getResultCode();
     *    switch( errCode ) {
     *        case ( e.NO_SUCH_OBJECT ):
     *            System.out.println( "The specified entry " + myDN +
     *                                " does not exist in the directory." );
     *            return;
     *        default:
     *            System.out.println( "An internal error occurred." );
     *            return;
     *    }
     * }
     * </PRE>
     *
     * @param toGet LDAP URL specifying the entry to read
     * @param cons preferences for the read operation
     * @return LDAPEntry returns the entry specified by the URL (or raises
     * an exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     * @see org.ietf.ldap.LDAPUrl
     * @see org.ietf.ldap.LDAPConnection#search(org.ietf.ldap.LDAPUrl)
     */
    public static LDAPEntry read( LDAPUrl toGet,
                                  LDAPSearchConstraints cons )
        throws LDAPException {
        if (cons == null) {
            cons = new LDAPSearchConstraints();
        }
        String host = toGet.getHost ();
        int port = toGet.getPort();

        if (host == null) {
            throw new LDAPException ( "no host for connection",
                                    LDAPException.PARAM_ERROR );
        }

        String[] attributes = toGet.getAttributeArray ();
        String DN = toGet.getDN();
        LDAPEntry returnValue;

        LDAPConnection connection = new LDAPConnection ();
        if (toGet.isSecure()) {
            LDAPSocketFactory factory = toGet.getSocketFactory();
            if (factory == null) {
                throw new LDAPException("No socket factory for LDAPUrl",
                                         LDAPException.OTHER);
            }
            connection.setSocketFactory(factory);
        }
        connection.connect (host, port);

        returnValue = connection.read (DN, attributes, cons);
        connection.disconnect ();

        return returnValue;
    }

    /**
     * Disconnect from the server and then reconnect using the current
     * credentials and authentication method
     * @exception LDAPException if not previously connected, or if
     * there is a failure on disconnecting or on connecting 
     */
    public void reconnect() throws LDAPException {
        
        disconnect();
        connect();
        
        if (_saslBinder != null) {
            _saslBinder.bind(this, true);
            _authMethod = "sasl";
        } else {
            internalBind (_protocolVersion, true, _defaultConstraints);
        }
    }

    /**
     * Deregisters an object so that it will no longer be notified on 
     * arrival of an unsolicited message from a server. If the object is 
     * null or was not previously registered for unsolicited notifications, 
     * the method does nothing.
     */
    public void removeUnsolicitedNotificationListener(
        LDAPUnsolicitedNotificationListener listener ) {
    }

    /**
     * Renames an existing entry in the directory. <P>
     *
     * You can specify whether or not the original name of the entry is
     * retained as a value in the entry. For example, suppose you rename
     * the entry "cn=Barbara" to "cn=Babs".  You can keep "cn=Barbara"
     * as a value in the entry so that the cn attribute has two values: <P>
     *
     * <PRE>
     *       cn=Barbara
     *       cn=Babs
     * </PRE>
     * The following example renames an entry.  The old name of the entry
     * is kept as a value in the entry. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String newRDN = "cn=Babs Jensen";
     * myConn.rename( myEntryDN, newRDN, false );
     * ... </PRE>
     *
     * @param DN current distinguished name of the entry
     * @param newRDN new relative distinguished name for the entry (for example,
     * "cn=newName")
     * @param deleteOldRDN if <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @exception LDAPException Failed to rename the specified entry.
     */
    public void rename( String DN, String newRDN, boolean deleteOldRDN )
        throws LDAPException {
        rename( DN, newRDN, null, deleteOldRDN );
    }

    /**
     * Renames an existing entry in the directory. <P>
     *
     * You can specify whether or not the original name of the entry is
     * retained as a value in the entry. For example, suppose you rename
     * the entry "cn=Barbara" to "cn=Babs".  You can keep "cn=Barbara"
     * as a value in the entry so that the cn attribute has two values: <P>
     *
     * <PRE>
     *       cn=Barbara
     *       cn=Babs
     * </PRE>
     * The following example renames an entry.  The old name of the entry
     * is kept as a value in the entry. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     * String newRDN = "cn=Babs Jensen";
     * myConn.rename( myEntryDN, newRDN, false );
     * ... </PRE>
     *
     * @param DN current distinguished name of the entry
     * @param newRDN new relative distinguished name for the entry (for example,
     * "cn=newName")
     * @param deleteOldRDN if <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to rename the specified entry.
     */
    public void rename( String DN, String newRDN, boolean deleteOldRDN,
                        LDAPConstraints cons )
        throws LDAPException {
        rename( DN, newRDN, null, deleteOldRDN, cons );
    }

    /**
     * Renames an existing entry in the directory and (optionally)
     * changes the location of the entry in the directory tree.<P>
     *
     * <B>NOTE: </B>Netscape Directory Server 3.0 does not support the
     * capability to move an entry to a different location in the
     * directory tree.  If you specify a value for the <CODE>newParentDN</CODE>
     * argument, an <CODE>LDAPException</CODE> will be thrown.
     * <P>
     *
     * @param DN current distinguished name of the entry
     * @param newRDN new relative distinguished name for the entry (for example,
     * "cn=newName")
     * @param newParentDN if not null, the distinguished name for the
     * entry under which the entry should be moved (for example, to move
     * an entry under the Accounting subtree, specify this argument as
     * "ou=Accounting, o=Ace Industry, c=US")
     * @param deleteOldRDN if <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @exception LDAPException Failed to rename the specified entry.
     */
     public void rename( String DN,
                         String newRDN,
                         String newParentDN,
                         boolean deleteOldRDN ) throws LDAPException {
          rename( DN, newRDN, newParentDN, deleteOldRDN, _defaultConstraints );
     }

    /**
     * Renames an existing entry in the directory and (optionally)
     * changes the location of the entry in the directory tree. Also
     * allows you to specify preferences for this LDAP modify DN operation
     * by using an <CODE>LDAPConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * <B>NOTE: </B>Netscape Directory Server 3.0 does not support the
     * capability to move an entry to a different location in the
     * directory tree.  If you specify a value for the <CODE>newParentDN</CODE>
     * argument, an <CODE>LDAPException</CODE> will be thrown.
     * <P>
     *
     * @param DN current distinguished name of the entry
     * @param newRDN new relative distinguished name for the entry (for example,
     * "cn=newName")
     * @param newParentDN if not null, the distinguished name for the
     * entry under which the entry should be moved (for example, to move
     * an entry under the Accounting subtree, specify this argument as
     * "ou=Accounting, o=Ace Industry, c=US")
     * @param deleteOldRDN if <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to rename the specified entry.
     * @see org.ietf.ldap.LDAPConstraints
     */
    public void rename ( String DN,
                         String newRDN,
                         String newParentDN,
                         boolean deleteOldRDN,
                         LDAPConstraints cons )
        throws LDAPException {
        internalBind (cons);

        LDAPResponseQueue myListener = getResponseListener ();
        try {
            JDAPModifyRDNRequest request = null;
            if ( newParentDN != null ) {
                request = new JDAPModifyRDNRequest( DN,
                                                    newRDN,
                                                    deleteOldRDN,
                                                    newParentDN );
            } else {
                request = new JDAPModifyRDNRequest( DN,
                                                    newRDN,
                                                    deleteOldRDN );
            }
            sendRequest (request, myListener, cons);
            LDAPMessage response = myListener.getResponse();
            checkMsg (response);
        } catch (LDAPReferralException e) {
            performReferrals(e, cons, JDAPProtocolOp.MODIFY_RDN_REQUEST,
                             DN, 0, newRDN, null, deleteOldRDN, null, null,
                             null, null);
        } finally {
            releaseResponseListener (myListener);
        }
    }

    /**
     * Renames an existing entry in the directory.
     * 
     * @param DN current distinguished name of the entry
     * @param newRDN new relative distinguished name for the entry
     * @param deleteOldRDN if true, the old name is not retained as an
     * attribute value
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     */
    public LDAPResponseQueue rename( String DN,
                                     String newRDN,
                                     boolean deleteOldRDN,
                                     LDAPResponseQueue listener )
        throws LDAPException{
        return rename( DN, newRDN, deleteOldRDN, listener,
                       _defaultConstraints );
    }

    /**
     * Renames an existing entry in the directory.
     * 
     * @param DN current distinguished name of the entry
     * @param newRDN new relative distinguished name for the entry
     * @param deleteOldRDN if true, the old name is not retained as an attribute
     * value
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     * @see org.ietf.ldap.LDAPConstraints
     */
    public LDAPResponseQueue rename( String DN,
                                     String newRDN,
                                     boolean deleteOldRDN,
                                     LDAPResponseQueue listener,
                                     LDAPConstraints cons )
        throws LDAPException{
        if (cons == null) {
            cons = _defaultConstraints;
        }
        
        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseQueue(/*asynchOp=*/true);
        }

        sendRequest (new JDAPModifyRDNRequest (DN, newRDN, deleteOldRDN),
                     listener, cons);
        
        return listener;
    }
        
    /**
     * Renames an existing entry in the directory.
     * 
     * @param DN current distinguished name of the entry
     * @param newRDN new relative distinguished name for the entry
     * @param newParentDN if not null, the distinguished name for the
     * entry under which the entry should be moved (for example, to move
     * an entry under the Accounting subtree, specify this argument as
     * "ou=Accounting, o=Ace Industry, c=US")
     * @param deleteOldRDN if true, the old name is not retained as an
     * attribute value
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     */
    public LDAPResponseQueue rename( String DN,
                                     String newRDN,
                                     String newParentDN,
                                     boolean deleteOldRDN,
                                     LDAPResponseQueue listener )
        throws LDAPException{
        return rename( DN, newRDN, newParentDN, deleteOldRDN, listener,
                       _defaultConstraints );
    }

    /**
     * Renames an existing entry in the directory.
     * 
     * @param DN current distinguished name of the entry
     * @param newRDN new relative distinguished name for the entry
     * @param newParentDN if not null, the distinguished name for the
     * entry under which the entry should be moved (for example, to move
     * an entry under the Accounting subtree, specify this argument as
     * "ou=Accounting, o=Ace Industry, c=US")
     * @param deleteOldRDN if true, the old name is not retained as an
     * attribute value
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPResponseQueue
     */
    public LDAPResponseQueue rename( String DN,
                                     String newRDN,
                                     String newParentDN,
                                     boolean deleteOldRDN,
                                     LDAPResponseQueue listener,
                                     LDAPConstraints cons )
        throws LDAPException{
        if (cons == null) {
            cons = _defaultConstraints;
        }
        
        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseQueue(/*asynchOp=*/true);
        }

        JDAPModifyRDNRequest request;
        if ( newParentDN != null ) {
            request = new JDAPModifyRDNRequest( DN,
                                                newRDN,
                                                deleteOldRDN,
                                                newParentDN );
        } else {
            request = new JDAPModifyRDNRequest( DN,
                                                newRDN,
                                                deleteOldRDN );
        }
        sendRequest( request, listener, cons );
        
        return listener;
    }

    /**
     * Performs the search specified by the LDAP URL. <P>
     *
     * For example, the following section of code searches for all entries under
     * the <CODE>ou=Product Development,o=Ace Industry,c=US</CODE> subtree of a
     * directory.  The example gets and prints the mail attribute for each entry
     * found. <P>
     *
     * <PRE>
     * String flatURL = "ldap://alway.mcom.com:3890/ou=Product Development,o=Ace Industry,c=US?mail?sub?objectclass=*";
     * LDAPUrl myURL;
     * try {
     *    myURL = new LDAPUrl( flatURL );
     * } catch ( java.net.MalformedURLException e ) {
     *    System.out.println( "Incorrect URL syntax." );
     *    return;
     * }
     *
     * LDAPSearchResults myResults = null;
     * try {
     *    myResults = myConn.search( myURL );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     *
     * while ( myResults.hasMore() ) {
     *    LDAPEntry myEntry = myResults.next();
     *    String nextDN = myEntry.getDN();
     *    System.out.println( nextDN );
     *    LDAPAttributeSet entryAttrs = myEntry.getAttributeSet();
     *    Iterator attrsInSet = entryAttrs.iterator();
     *    while ( attrsInSet.hasMore() ) {
     *        LDAPAttribute nextAttr = (LDAPAttribute)attrsInSet.next();
     *        String attrName = nextAttr.getName();
     *        System.out.print( "\t" + attrName + ": " );
     *        Enumeration valsInAttr = nextAttr.getStringValues();
     *        while ( valsInAttr.hasMoreElements() ) {
     *            String nextValue = (String)valsInAttr.nextElement();
     *            System.out.println( nextValue );
     *        }
     *    }
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param toGet LDAP URL representing the search to perform
     * @return LDAPSearchResults the results of the search as an enumeration.
     * @exception LDAPException Failed to complete the search specified by
     * the LDAP URL.
     * @see org.ietf.ldap.LDAPUrl
     * @see org.ietf.ldap.LDAPSearchResults
     * @see org.ietf.ldap.LDAPConnection#abandon(org.ietf.ldap.LDAPSearchResults)
     */
    public static LDAPSearchResults search( LDAPUrl toGet )
        throws LDAPException {
        return search( toGet, null );
    }

    /**
     * Performs the search specified by the LDAP URL. This method also
     * allows you to specify constraints for the search (such as the
     * maximum number of entries to find or the
     * maximum time to wait for search results). <P>
     *
     * As part of the search constraints, you can specify whether or not you
     * want the results delivered all at once or in smaller batches.
     * If you specify the results delivered in smaller
     * batches, each iteration blocks until the next batch of results is
     * returned. <P>
     *
     * For example, the following section of code retrieves the first 5
     * matching entries for the search specified by the LDAP URL.  The
     * example accomplishes this by creating a new set of search
     * constraints where the maximum number of search results is 5. <P>
     *
     * <PRE>
     * LDAPSearchConstraints mySearchConstraints = myConn.getSearchConstraints();
     * mySearchConstraints.setMaxResults( 5 );
     * String flatURL = "ldap://alway.mcom.com:3890/ou=Product Development,o=Ace Industry,c=US?mail?sub?objectclass=*";
     * LDAPUrl myURL;
     * try {
     *    myURL = new LDAPUrl( flatURL );
     * } catch ( java.net.MalformedURLException e ) {
     *    System.out.println( "Incorrect URL syntax." );
     *    return;
     * }
     * LDAPSearchResults myResults = null;
     * try {
     *    myResults = myConn.search( myURL, mySearchConstraints );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param toGet LDAP URL representing the search to run
     * @param cons constraints specific to the search
     * @return LDAPSearchResults the results of the search as an enumeration.
     * @exception LDAPException Failed to complete the search specified
     * by the LDAP URL.
     * @see org.ietf.ldap.LDAPUrl
     * @see org.ietf.ldap.LDAPSearchResults
     * @see org.ietf.ldap.LDAPConnection#abandon(org.ietf.ldap.LDAPSearchResults)
     */
    public static LDAPSearchResults search( LDAPUrl toGet,
                                            LDAPSearchConstraints cons )
        throws LDAPException {
        String host = toGet.getHost ();
        int port = toGet.getPort();

        if (host == null) {
            throw new LDAPException ( "no host for connection",
                                    LDAPException.PARAM_ERROR );
        }

        String[] attributes = toGet.getAttributeArray ();
        String DN = toGet.getDN();
        String filter = toGet.getFilter();
        if (filter == null) {
            filter = defaultFilter;
        }
        int scope = toGet.getScope ();

        LDAPConnection connection = new LDAPConnection ();
        if (toGet.isSecure()) {
            LDAPSocketFactory factory = toGet.getSocketFactory();
            if (factory == null) {
                throw new LDAPException("No socket factory for LDAPUrl",
                                         LDAPException.OTHER);
            }
            connection.setSocketFactory(factory);
        }        
        connection.connect (host, port);

        LDAPSearchResults results;
        if (cons != null) {
            results = connection.search (DN, scope, filter, attributes, false, cons);
        } else {
            results = connection.search (DN, scope, filter, attributes, false);
        }

        results.closeOnCompletion(connection);

        return results;
    }

    /**
     * Performs the search specified by the criteria that you enter. <P>
     *
     * For example, the following section of code searches for all entries under
     * the <CODE>ou=Product Development,o=Ace Industry,c=US</CODE> subtree of a
     * directory.  The example gets and prints the mail attribute for each entry
     * found. <P>
     *
     * <PRE>
     * String myBaseDN = "ou=Product Development,o=Ace Industry,c=US";
     * String myFilter="(objectclass=*)";
     * String[] myAttrs = { "mail" };
     *
     * LDAPSearchResults myResults = null;
     * try {
     *    myResults = myConn.search( myBaseDN, LDAPConnection.SCOPE_SUB, myFilter, myAttrs, false );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     *
     * while ( myResults.hasMore() ) {
     *    LDAPEntry myEntry = myResults.next();
     *    String nextDN = myEntry.getDN();
     *    System.out.println( nextDN );
     *    LDAPAttributeSet entryAttrs = myEntry.getAttributeSet();
     *    Iterator attrsInSet = entryAttrs.iterator();
     *    while ( attrsInSet.hasMore() ) {
     *        LDAPAttribute nextAttr = (LDAPAttribute)attrsInSet.next();
     *        String attrName = nextAttr.getName();
     *        System.out.println( "\t" + attrName + ":" );
     *        Enumeration valsInAttr = nextAttr.getStringValues();
     *        while ( valsInAttr.hasMoreElements() ) {
     *            String nextValue = (String)valsInAttr.nextElement();
     *            System.out.println( "\t\t" + nextValue );
     *        }
     *    }
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPConnection.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPConnection.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPConnection.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter search filter specifying the search criteria
     * @param attrs list of attributes that you want returned in the
     * search results
     * @param attrsOnly if true, returns the names but not the values of the
     * attributes found.  If false, returns the names and values for
     * attributes found
     * @return LDAPSearchResults the results of the search as an enumeration.
     * @exception LDAPException Failed to complete the specified search.
     * @see org.ietf.ldap.LDAPConnection#abandon(org.ietf.ldap.LDAPSearchResults)
     */
    public LDAPSearchResults search( String base,
                                     int scope,
                                     String filter,
                                     String[] attrs,
                                     boolean attrsOnly ) throws LDAPException {
        return search( base, scope, filter, attrs, attrsOnly,
                       _defaultConstraints );
    }

    /**
     * Performs the search specified by the criteria that you enter.
     * This method also allows you to specify constraints for the search
     * (such as the maximum number of entries to find or the
     * maximum time to wait for search results). <P>
     *
     * As part of the search constraints, you can specify whether or not
     * you want the
     * results delivered all at once or in smaller batches.  If you
     * specify that you want the results delivered in smaller batches,
     * each iteration blocks until the
     * next batch of results is returned. <P>
     *
     * For example, the following section of code retrieves the first 5 entries
     * matching the specified search criteria.  The example accomplishes
     * this by creating a new set of search constraints where the maximum
     * number of search results is 5. <P>
     *
     * <PRE>
     * String myBaseDN = "ou=Product Development,o=Ace Industry,c=US";
     * String myFilter="(objectclass=*)";
     * String[] myAttrs = { "mail" };
     * LDAPSearchConstraints mySearchConstraints = myConn.getSearchConstraints();
     * mySearchConstraints.setMaxResults( 5 );
     *
     * LDAPSearchResults myResults = null;
     * try {
     *    myResults = myConn.search( myBaseDN, LDAPConnection.SCOPE_SUB, myFilter, myAttrs, false, mySearchConstraints );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPConnection.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPConnection.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPConnection.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter search filter specifying the search criteria
     * @param attrs list of attributes to return in the search
     * results
     * @param cons constraints specific to this search (for example, the
     * maximum number of entries to return)
     * @param attrsOnly if true, returns the names but not the values of the
     * attributes found.  If false, returns the names and  values for
     * attributes found
     * @return LDAPSearchResults the results of the search as an enumeration.
     * @exception LDAPException Failed to complete the specified search.
     * @see org.ietf.ldap.LDAPConnection#abandon(org.ietf.ldap.LDAPSearchResults)
     */
    public LDAPSearchResults search( String base,
                                     int scope,
                                     String filter,
                                     String[] attrs,
                                     boolean attrsOnly,
                                     LDAPSearchConstraints cons )
        throws LDAPException {
        if (cons == null) {
            cons = _defaultConstraints;
        }

        LDAPSearchResults returnValue =
            new LDAPSearchResults( this, cons, base, scope, filter,
                                   attrs, attrsOnly );
        Vector cacheValue = null;
        Long key = null;
        boolean isKeyValid = true;

        try {
            // get entry from cache which is a vector of JDAPMessages
            if ( _cache != null ) {
                // create key for cache entry using search arguments
                key = _cache.createKey( getHost(), getPort(),base, filter,
                                        scope, attrs, _boundDN, cons );

                cacheValue = (Vector)_cache.getEntry(key);

                if ( cacheValue != null ) {
                    return new LDAPSearchResults(
                        cacheValue, this, cons, base, scope,
                        filter, attrs, attrsOnly );
                }
            }
        } catch ( LDAPException e ) {
            isKeyValid = false;
            printDebug("Exception: "+e);
        }

        internalBind( cons );

        LDAPSearchQueue myListener = getSearchListener( cons );
        int deref = cons.getDereference();

        JDAPSearchRequest request = null;        
        try {
            request = new JDAPSearchRequest( base, scope, deref,
                                             cons.getMaxResults(),
                                             cons.getServerTimeLimit(),
                                             attrsOnly, filter, attrs );
        } catch ( IllegalArgumentException e ) {
            throw new LDAPException(e.getMessage(), LDAPException.PARAM_ERROR);
        }

        // if using cache, then we need to set the key of the search listener
        if ( (_cache != null) && isKeyValid ) {
            myListener.setKey( key );
        }
        
        try {
            sendRequest( request, myListener, cons );
        }
        catch ( LDAPException e ) {
            releaseSearchListener( myListener );
            throw e;                    
        }

        /* Is this a persistent search? */
        boolean isPersistentSearch = false;
        LDAPControl[] controls =
            (LDAPControl[])getOption( SERVERCONTROLS, cons );
        for ( int i = 0; (controls != null) && (i < controls.length); i++ ) {
            if ( controls[i] instanceof
                 org.ietf.ldap.controls.LDAPPersistSearchControl ) {
                isPersistentSearch = true;
                break;
            }
        }

        /* For a persistent search, don't wait for a first result, because
           there may be none at this time if changesOnly was specified in
           the control.
        */
        if ( isPersistentSearch ) {
            returnValue.associatePersistentSearch (myListener);

        } else if ( cons.getBatchSize() == 0 ) {
            /* Synchronous search if all requested at once */
            try {
                /* Block until all results are in */
                LDAPMessage response = myListener.completeRequest();
                Iterator results = myListener.getAllMessages().iterator();

                checkSearchMsg(returnValue, response, cons, base, scope,
                               filter, attrs, attrsOnly);

                while ( results.hasNext() ) {
                    LDAPMessage msg = (LDAPMessage)results.next();
                    checkSearchMsg( returnValue, msg, cons, base, scope,
                                    filter, attrs, attrsOnly );
                }
            } finally {
                releaseSearchListener( myListener );
            }
        } else {
            /*
            * Asynchronous to retrieve one at a time, check to make sure
            * the search didn't fail
            */
            LDAPMessage firstResult = myListener.getResponse();
            if ( firstResult instanceof LDAPResponse ) {
                try {
                    checkSearchMsg( returnValue, firstResult, cons, base,
                                    scope, filter, attrs, attrsOnly );
                } finally {
                    releaseSearchListener( myListener );
                }
            } else {
                try {
                    checkSearchMsg( returnValue, firstResult, cons, base,
                                    scope, filter, attrs, attrsOnly );
                } catch ( LDAPException ex ) {
                    releaseSearchListener( myListener );
                    throw ex;
                }

                /* we let this listener get garbage collected.. */
                returnValue.associate( myListener );
            }
        }
        return returnValue;
    }

    /**
     * Performs the search specified by the criteria that you enter. <P>
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPConnection.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPConnection.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPConnection.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter search filter specifying the search criteria
     * @param attrs list of attributes that you want returned in the
     * search results
     * @param typesOnly if true, returns the names but not the values of the
     * attributes found.  If false, returns the names and values for
     * attributes found
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPSearchQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPConnection#abandon(org.ietf.ldap.LDAPSearchQueue)
     */
    public LDAPSearchQueue search( String base,
                                   int scope,
                                   String filter,
                                   String attrs[],
                                   boolean typesOnly,
                                   LDAPSearchQueue listener )
        throws LDAPException {
        
        return search( base, scope, filter, attrs, typesOnly,
                       listener, _defaultConstraints );
    }

    /**
     * Performs the search specified by the criteria that you enter.
     * This method also allows you to specify constraints for the search
     * (such as the maximum number of entries to find or the
     * maximum time to wait for search results). <P>
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPConnection.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPConnection.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPConnection.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter search filter specifying the search criteria
     * @param attrs list of attributes that you want returned in the search
     * results
     * @param typesOnly if true, returns the names but not the values of the
     * attributes found.  If false, returns the names and  values for
     * attributes found.
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to this search (for example, the
     * maximum number of entries to return)
     * @return LDAPSearchQueue handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see org.ietf.ldap.LDAPConnection#abandon(org.ietf.ldap.LDAPSearchQueue)
     */
    public LDAPSearchQueue search( String base,
                                   int scope,
                                   String filter,
                                   String attrs[],
                                   boolean typesOnly,
                                   LDAPSearchQueue listener,
                                   LDAPSearchConstraints cons )
        throws LDAPException {
        if ( cons == null ) {
            cons = _defaultConstraints;
        }

        internalBind( cons );
        
        if ( listener == null ) {
            listener = new LDAPSearchQueue( /*asynchOp=*/true, cons );
        }
        
        JDAPSearchRequest request = null;        
        try {
            request = new JDAPSearchRequest( base, scope,
                                             cons.getDereference(),
                                             cons.getMaxResults(),
                                             cons.getServerTimeLimit(),
                                             typesOnly, filter, attrs );
        }
        catch ( IllegalArgumentException e ) {
            throw new LDAPException( e.getMessage(),
                                     LDAPException.PARAM_ERROR );
        }

        sendRequest( request, listener, cons );
        return listener;
        
    }
    
    /**
     *  Sets the specified <CODE>LDAPCache</CODE> object as the
     *  cache for the <CODE>LDAPConnection</CODE> object.
     *  <P>
     *
     *  @param cache the <CODE>LDAPCache</CODE> object representing
     *  the cache that the current connection should use
     *  @see org.ietf.ldap.LDAPCache
     *  @see org.ietf.ldap.LDAPConnection#getCache
     */
    public void setCache( LDAPCache cache ) {
        if ( _cache != null ) {
            _cache.removeReference();
        }
        if ( cache != null ) {
            cache.addReference();
        }
        _cache = cache;
        if ( _thread != null ) {
            _thread.setCache( cache );
        }
    }

    /**
     * Specifies the maximum time to wait for the connection to be established.
     * If the value is 0, the time is not limited.
     * @param timeout the maximum connect time in seconds or 0 (unlimited)
     */
    public void setConnectTimeout (int timeout) {
        if ( timeout < 0 ) {
            throw new IllegalArgumentException(
                "Timeout value can not be negative" );
        }
        _connectTimeout = timeout;
        if ( _connMgr != null ) {
            _connMgr.setConnectTimeout( _connectTimeout );
        }
    }
        
    /**
     * Specifies the delay in seconds when making concurrent connection attempts to
     * multiple servers.
     * <P>Effectively, selects the connection setup policy when a list of hosts is passed
     * to the <CODE>connect</CODE> method.
     * 
     * <br>If the serial policy is selected, the default one, an attempt is made to
     * connect to the first host in the list. The next entry in
     * the list is tried only if the attempt to connect to the current host fails.
     * This might cause your application to block for unacceptably long time if a host is down.
     *
     * <br>If the parallel policy is selected, multiple connection attempts may run
     * concurrently on a separate thread. A new connection attempt to the next entry
     * in the list can be started with or without delay.
     * <P>You must set the <CODE>ConnSetupDelay</CODE> before making the call to the 
     * <CODE>connect</CODE> method.
     * 
     * @param delay the delay in seconds between connection attempts. Possible values are:<br>
     * <CODE>NODELAY_SERIAL</CODE> Use the serial connection setup policy.<br>
     * <CODE>NODELAY_PARALLEL</CODE> Use the parallel connection setup policy with no delay.
     * Start all connection setup threads immediately.<br>
     * <CODE>delay > 0</CODE> Use the parallel connection setup policy with delay.
     * Start another connection setup thread after <CODE>delay</CODE> seconds.<br>
     * @see org.ietf.ldap.LDAPConnection#NODELAY_SERIAL
     * @see org.ietf.ldap.LDAPConnection#NODELAY_PARALLEL
     * @see org.ietf.ldap.LDAPConnection#connect(java.lang.String, int)
     */
    public void setConnSetupDelay( int delay ) {
        _connSetupDelay = delay;
        if ( _connMgr != null ) {
            _connMgr.setConnSetupDelay( delay );
        }
    }

    /**
     * Set the default constraint set for all operations. 
     * @param cons <CODE>LDAPConstraints</CODE> object to use as the default
     * constraint set
     * @see org.ietf.ldap.LDAPConnection#getConstraints
     * @see org.ietf.ldap.LDAPConstraints
     */
    public void setConstraints( LDAPConstraints cons ) {
        _defaultConstraints.setHopLimit( cons.getHopLimit() );
        _defaultConstraints.setReferralFollowing( cons.getReferralFollowing() );
        _defaultConstraints.setTimeLimit( cons.getTimeLimit() );
        _defaultConstraints.setReferralHandler( cons.getReferralHandler() );

        LDAPControl[] tServerControls = cons.getControls();
        LDAPControl[] oServerControls = null;
        if ( (tServerControls != null) && 
             (tServerControls.length > 0) ) {
            oServerControls =
                new LDAPControl[tServerControls.length];
            for( int i = 0; i < tServerControls.length; i++ ) {
                oServerControls[i] = (LDAPControl)tServerControls[i].clone();
            }
        }
        _defaultConstraints.setControls( oServerControls );

        if ( cons instanceof LDAPSearchConstraints ) {
            LDAPSearchConstraints scons = (LDAPSearchConstraints)cons;
            _defaultConstraints.setServerTimeLimit( scons.getServerTimeLimit() );
            _defaultConstraints.setDereference( scons.getDereference() );
            _defaultConstraints.setMaxResults( scons.getMaxResults() );
            _defaultConstraints.setBatchSize( scons.getBatchSize() );
            _defaultConstraints.setMaxBacklog( scons.getMaxBacklog() );
        }
    }
    
    /**
     * Sets the stream for reading from the listener socket if
     * there is one
     *
     * @param is the stream for reading from the listener socket
     */
    public void setInputStream( InputStream is ) {
        if ( _thread != null ) {
            _thread.setInputStream( is );
        }
    }

    /**
     * Sets the stream for writing to the socket
     *
     * @param os the stream for writing to the socket, if there is one
     */
    public void setOutputStream( OutputStream os ) {
        if ( _thread != null ) {
            _thread.setOutputStream( os );
        }
    }

    /**
     * Sets a global property of the connection.
     * The following properties are defined:<BR> 
     * com.org.ietf.ldap.schema.quoting - "standard" or "NetscapeBug"<BR> 
     * Note: if this property is not set, the SDK will query the server 
     * to determine if attribute syntax values and objectclass superior 
     * values must be quoted when adding schema.<BR>
     * com.org.ietf.ldap.saslpackage - the default is "com.netscape.sasl"<BR> 
     * <P>
     *
     * @param name name of the property to set
     * @param val value to set
     * @exception LDAPException Unable to set the value of the specified
     * property.
     */
    public void setProperty( String name, Object val ) throws LDAPException {
        if ( name.equalsIgnoreCase( SCHEMA_BUG_PROPERTY ) ) { 
            _properties.put( SCHEMA_BUG_PROPERTY, val ); 
        } else if ( name.equalsIgnoreCase( SASL_PACKAGE_PROPERTY ) ) {
            _properties.put( SASL_PACKAGE_PROPERTY, val ); 
        } else if ( name.equalsIgnoreCase( "debug" ) ) {
            debug = ((String)val).equalsIgnoreCase( "true" ); 

        } else if ( name.equalsIgnoreCase( TRACE_PROPERTY ) ) {

            Object traceOutput = null;
            if (val == null) {
                _properties.remove(TRACE_PROPERTY);
            } else {
                if ( _thread != null ) {
                    traceOutput = createTraceOutput( val );
                }
                _properties.put( TRACE_PROPERTY, val ); 
            }

            if ( _thread != null ) {
                _thread.setTraceOutput( traceOutput );
            }

        // This is used only by the ldapjdk test cases to simulate a
        // server problem and to test fail-over and rebind            
        } else if ( name.equalsIgnoreCase( "breakConnection" ) ) {
            _connMgr.breakConnection();

        } else {
            throw new LDAPException( "Unknown property: " + name );
        }
    }

    /**
     * Set the default constraint set for all search operations. 
     * @param cons <CODE>LDAPSearchConstraints</CODE> object to use as the
     * default constraint set
     * @see org.ietf.ldap.LDAPConnection#getSearchConstraints
     * @see org.ietf.ldap.LDAPSearchConstraints
     */
    public void setSearchConstraints( LDAPSearchConstraints cons ) {
        _defaultConstraints = (LDAPSearchConstraints)cons.clone();
    }

    /**
     * Specifies the object representing the socket factory that you
     * want to use to establish a connection to a server.
     * <P>
     *
     * @param factory the object representing the socket factory that
     * you want to use to establish a connection to a server
     * @see org.ietf.ldap.LDAPSocketFactory
     * @see org.ietf.ldap.LDAPSSLSocketFactory
     * @see org.ietf.ldap.LDAPConnection#getSocketFactory
     */
    public void setSocketFactory( LDAPSocketFactory factory ) {
        _factory = factory;
    }

    /**
     * Begin using the Transport Layer Security (TLS) protocol for session 
     * privacy [TLS][LDAPTLS]. If the socket factory of the connection is 
     * not capable of initiating a TLS session, an LDAPException is thrown 
     * with the error code TLS_NOT_SUPPORTED. If the server does not support 
     * the transition to a TLS session, an LDAPException is thrown with the 
     * error code returned by the server. If there are outstanding LDAP 
     * operations on the connection, an LDAPException is thrown.
     */
    public void startTLS() throws LDAPException {
    }

    /**
     * Stop using the Transport Layer Security (TLS) protocol for session 
     * privacy [LDAPTLS]. If the server does not support the termination of 
     * a TLS session, an LDAPException is thrown with the error code 
     * returned by the server. If there are outstanding LDAP operations on 
     * the connection, an LDAPException is thrown.
     */
    public void stopTLS() throws LDAPException {
    }

    /**
     * Returns the value of the specified option for this
     * <CODE>LDAPConnection</CODE> object. <P>
     *
     * These options represent the constraints for the current connection.
     * To get all constraints for the current connection, call the
     * <CODE>getSearchConstraints</CODE> method.
     * <P>
     *
     * By default, the constraints apply to all operations performed
     * through the current connection.  You can change these constraints:
     * <P>
     *
     * <UL>
     * <LI> If you want to set a constraint only for a particular operation, 
     * create an <CODE>LDAPConstraints</CODE> object (or a 
     * <CODE>LDAPSearchConstraints</CODE> object for a search or find operation)
     * with your new constraints
     * and pass it to the <CODE>LDAPConnection</CODE> method that performs the
     * operation.
     * <P>
     *
     * <LI>If you want to override these constraints for all operations
     * performed under the current connection, call the
     * <CODE>setOption</CODE> method to change the constraint.
     * <P>
     *
     * </UL>
     * <P>
     *
     * For example, the following section of code gets and prints the
     * maximum number of search results that are returned for searches
     * performed through this connection.  (This applies to all searches
     * unless a different set of search constraints is specified in an
     * <CODE>LDAPSearchConstraints</CODE> object.)
     * <P>
     *
     * <PRE>
     * LDAPConnection ld = new LDAPConnection();
     * int sizeLimit = ( (Integer)ld.getOption( LDAPConnection.SIZELIMIT ) ).intValue();
     * System.out.println( "Maximum number of results: " + sizeLimit );
     * </PRE>
     *
     * @param option you can specify one of the following options:
     * <TABLE CELLPADDING=5>
     * <TR VALIGN=BASELINE ALIGN=LEFT>
     * <TH>Option</TH><TH>Data Type</TH><TH>Description</TH></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.PROTOCOL_VERSION</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the version of the LDAP protocol used by the
     * client.
     * <P>By default, the value of this option is 2.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.DEREF</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies when your client dereferences aliases.
     *<PRE>
     * Legal values for this option are:
     *
     * DEREF_NEVER       Aliases are never dereferenced.
     *
     * DEREF_FINDING     Aliases are dereferenced when find-
     *                   ing the starting point for the
     *                   search (but not when searching
     *                   under that starting entry).
     *
     * DEREF_SEARCHING   Aliases are dereferenced when
     *                   searching the entries beneath the
     *                   starting point of the search (but
     *                   not when finding the starting
     *                   entry).
     *
     * DEREF_ALWAYS      Aliases are always dereferenced.
     *</PRE>
     * <P>By default, the value of this option is
     * <CODE>DEREF_NEVER</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.SIZELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to return.
     * If this option is set to 0, there is no maximum limit.
     * <P>By default, the value of this option is 1000.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.TIMELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of milliseconds to wait for results
     * before timing out. If this option is set to 0, there is no maximum
     * time limit.
     * <P>By default, the value of this option is 0.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.REFERRALS</CODE></TD>
     * <TD><CODE>Boolean</CODE></TD>
     * <TD>Specifies whether or not your client follows referrals automatically.
     * If <CODE>true</CODE>, your client follows referrals automatically.
     * If <CODE>false</CODE>, an <CODE>LDAPReferralException</CODE> is raised
     * when referral is detected.
     * <P>By default, the value of this option is <CODE>false</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.REFERRALS_REBIND_PROC</CODE></TD>
     * <TD><CODE>LDAPAuthHandler</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPAuthHandler</CODE> interface.  You must define this class and
     * the <CODE>getAuthProvider</CODE> method that will be used to
     * get the distinguished name and password to use for authentication.
     * Modifying this option sets the <CODE>LDAPConnection.BIND</CODE> option to null.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.BIND</CODE></TD>
     * <TD><CODE>LDAPBind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPBind</CODE>
     * interface.  You must define this class and the
     * <CODE>bind</CODE> method that will be used to authenticate
     * to the server on referrals. Modifying this option sets the 
     * <CODE>LDAPConnection.REFERRALS_REBIND_PROC</CODE> to null.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.REFERRALS_HOP_LIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of referrals in a sequence that
     * your client will follow.  (For example, if REFERRALS_HOP_LIMIT is 5,
     * your client will follow no more than 5 referrals in a row when resolving
     * a single LDAP request.)
     * <P>By default, the value of this option is 10.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.BATCHSIZE</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the number of search results to return at a time.
     * (For example, if BATCHSIZE is 1, results are returned one at a time.)
     * <P>By default, the value of this option is 1.</TD></TR>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the client controls that may affect the handling of LDAP
     * operations in the LDAP classes. These controls are used by the client
     * and are not passed to the LDAP server. At this time, no client controls
     * are defined for clients built with the Netscape LDAP classes. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.SERVERCONTROLS</CODE></TD>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the server controls that are passed to the LDAP
     * server on each LDAP operation. Not all servers support server
     * controls; a particular server may or may not support a given
     * server control.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>MAXBACKLOG</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to accumulate in an
     * LDAPSearchResults before suspending the reading of input from the server.
     * <P>By default, the value of this option is 100. The value 0 means there
     *  is no limit.</TD></TR>
     * </TABLE><P>
     * @return the value for the option wrapped in an object.  (You
     * need to cast the returned value as its appropriate type. For
     * example, when getting the SIZELIMIT option, cast the returned
     * value as an <CODE>Integer</CODE>.)
     * @exception LDAPException Failed to get the specified option.
     * @see org.ietf.ldap.LDAPAuthHandler
     * @see org.ietf.ldap.LDAPConstraints
     * @see org.ietf.ldap.LDAPSearchConstraints
     * @see org.ietf.ldap.LDAPReferralException
     * @see org.ietf.ldap.LDAPControl
     * @see org.ietf.ldap.LDAPConnection#getSearchConstraints
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, org.ietf.ldap.LDAPSearchConstraints)
     */
    public Object getOption( int option ) throws LDAPException {
        if (option == LDAPConnection.PROTOCOL_VERSION) {
            return new Integer(_protocolVersion);
        }

        return getOption(option, _defaultConstraints);
    }

    private static Object getOption( int option, LDAPSearchConstraints cons )
        throws LDAPException {
        switch (option) {
            case LDAPConnection.DEREF:
              return new Integer (cons.getDereference());
            case LDAPConnection.SIZELIMIT:
              return new Integer (cons.getMaxResults());
            case LDAPConnection.TIMELIMIT:
              return new Integer (cons.getServerTimeLimit());
            case LDAPConnection.REFERRALS:
              return new Boolean (cons.getReferralFollowing());
            case LDAPConnection.REFERRALS_REBIND_PROC:
              return cons.getReferralHandler();
            case LDAPConnection.REFERRALS_HOP_LIMIT:
              return new Integer (cons.getHopLimit());
            case LDAPConnection.BATCHSIZE:
              return new Integer (cons.getBatchSize());
            case LDAPConnection.SERVERCONTROLS:
              return cons.getControls();
            case MAXBACKLOG:
              return new Integer (cons.getMaxBacklog());
            default:
              throw new LDAPException ( "invalid option",
                                        LDAPException.PARAM_ERROR );
        }
    }
    
    /**
     * Sets the value of the specified option for this
     * <CODE>LDAPConnection</CODE> object. <P>
     *
     * These options represent the constraints for the current
     * connection.
     * To get all constraints for the current connection, call the
     * <CODE>getSearchConstraints</CODE> method. 
     * <P>
     *
     * By default, the option that you set applies to all subsequent
     * operations performed through the current connection. If you want to
     * set a constraint only for a particular operation, create an
     * <CODE>LDAPConstraints</CODE> object (or a 
     * <CODE>LDAPSearchConstraints</CODE> object for a search or find operation)
     * with your new constraints
     * and pass it to the <CODE>LDAPConnection</CODE> method that performs the
     * operation.
     * <P>
     *
     * For example, the following section of code changes the constraint for
     * the maximum number of search results that are returned for searches
     * performed through this connection.  (This applies to all searches
     * unless a different set of search constraints is specified in an
     * <CODE>LDAPSearchConstraints</CODE> object.)
     * <P>
     *
     * <PRE>
     * LDAPConnection ld = new LDAPConnection();
     * Integer newLimit = new Integer( 20 );
     * ld.setOption( LDAPConnection.SIZELIMIT, newLimit );
     * System.out.println( "Changed the maximum number of results to " + newLimit.intValue() );
     * </PRE>
     *
     * @param option you can specify one of the following options:
     * <TABLE CELLPADDING=5>
     * <TR VALIGN=BASELINE ALIGN=LEFT>
     * <TH>Option</TH><TH>Data Type</TH><TH>Description</TH></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.PROTOCOL_VERSION</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the version of the LDAP protocol used by the
     * client.
     * <P>By default, the value of this option is 2.  If you want
     * to use LDAP v3 features (such as extended operations or
     * controls), you need to set this value to 3. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.DEREF</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies when your client dereferences aliases.
     *<PRE>
     * Legal values for this option are:
     *
     * DEREF_NEVER       Aliases are never dereferenced.
     *
     * DEREF_FINDING     Aliases are dereferenced when find-
     *                   ing the starting point for the
     *                   search (but not when searching
     *                   under that starting entry).
     *
     * DEREF_SEARCHING   Aliases are dereferenced when
     *                   searching the entries beneath the
     *                   starting point of the search (but
     *                   not when finding the starting
     *                   entry).
     *
     * DEREF_ALWAYS      Aliases are always dereferenced.
     *</PRE>
     * <P>By default, the value of this option is
     * <CODE>DEREF_NEVER</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.SIZELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to return.
     * If this option is set to 0, there is no maximum limit.
     * <P>By default, the value of this option is 1000.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.TIMELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of milliseconds to wait for results
     * before timing out. If this option is set to 0, there is no maximum
     * time limit.
     * <P>By default, the value of this option is 0.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.REFERRALS</CODE></TD>
     * <TD><CODE>Boolean</CODE></TD>
     * <TD>Specifies whether or not your client follows referrals automatically.
     * If <CODE>true</CODE>, your client follows referrals automatically.
     * If <CODE>false</CODE>, an <CODE>LDAPReferralException</CODE> is
     * raised when a referral is detected.
     * <P>By default, the value of this option is <CODE>false</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.REFERRALS_REBIND_PROC</CODE></TD>
     * <TD><CODE>LDAPAuthHandler</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPAuthHandler</CODE>
     * interface.  You must define this class and the
     * <CODE>getAuthProvider</CODE> method that will be used to get
     * the distinguished name and password to use for authentication. 
     * Modifying this option sets the <CODE>LDAPConnection.BIND</CODE> option to null.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.BIND</CODE></TD>
     * <TD><CODE>LDAPBind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPBind</CODE>
     * interface.  You must define this class and the
     * <CODE>bind</CODE> method that will be used to autheniticate
     * to the server on referrals. Modifying this option sets the 
     * <CODE>LDAPConnection.REFERRALS_REBIND_PROC</CODE> to null.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.REFERRALS_HOP_LIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of referrals in a sequence that
     * your client will follow.  (For example, if REFERRALS_HOP_LIMIT is 5,
     * your client will follow no more than 5 referrals in a row when resolving
     * a single LDAP request.)
     * <P>By default, the value of this option is 10.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.BATCHSIZE</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the number of search results to return at a time.
     * (For example, if BATCHSIZE is 1, results are returned one at a time.)
     * <P>By default, the value of this option is 1.</TD></TR>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the client controls that may affect handling of LDAP
     * operations in the LDAP classes. These controls are used by the client
     * and are not passed to the server. At this time, no client controls
     * are defined for clients built with the Netscape LDAP classes. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPConnection.SERVERCONTROLS</CODE></TD>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the server controls that are passed to the LDAP
     * server on each LDAP operation. Not all servers support server
     * controls; a particular server may or may not support a particular
     * control.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>MAXBACKLOG</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to accumulate in an
     * LDAPSearchResults before suspending the reading of input from the server.
     * <P>By default, the value of this option is 100. The value 0 means there
     *  is no limit.</TD></TR>
     * </TABLE><P>
     * @param value the value to assign to the option.  The value must be
     * the java.lang object wrapper for the appropriate parameter
     * (e.g. boolean->Boolean,
     *   integer->Integer)
     * @exception LDAPException Failed to set the specified option.
     * @see org.ietf.ldap.LDAPAuthHandler
     * @see org.ietf.ldap.LDAPConstraints
     * @see org.ietf.ldap.LDAPSearchConstraints
     * @see org.ietf.ldap.LDAPReferralException
     * @see org.ietf.ldap.LDAPControl
     * @see org.ietf.ldap.LDAPConnection#getSearchConstraints
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, org.ietf.ldap.LDAPSearchConstraints)
     */
    public void setOption( int option, Object value ) throws LDAPException {
        if (option == LDAPConnection.PROTOCOL_VERSION) {
            setProtocolVersion(((Integer)value).intValue());
            return;
        }
        setOption(option, value, _defaultConstraints);
    }

    private static void setOption( int option,
                                   Object value,
                                   LDAPSearchConstraints cons )
        throws LDAPException {
      try {
        switch (option) {
        case LDAPConnection.DEREF:
          cons.setDereference(((Integer)value).intValue());
          return;
        case LDAPConnection.SIZELIMIT:
          cons.setMaxResults(((Integer)value).intValue());
          return;
        case LDAPConnection.TIMELIMIT:
          cons.setTimeLimit(((Integer)value).intValue());
          return;
        case LDAPConnection.SERVER_TIMELIMIT:
          cons.setServerTimeLimit(((Integer)value).intValue());
          return;
        case LDAPConnection.REFERRALS:
          cons.setReferralFollowing(((Boolean)value).booleanValue());
          return;
        case LDAPConnection.REFERRALS_REBIND_PROC:
          cons.setReferralHandler((LDAPReferralHandler)value);
          return;
        case LDAPConnection.REFERRALS_HOP_LIMIT:
          cons.setHopLimit(((Integer)value).intValue());
          return;
        case LDAPConnection.BATCHSIZE:
          cons.setBatchSize(((Integer)value).intValue());
          return;
        case LDAPConnection.SERVERCONTROLS:
          if ( value == null )
            cons.setControls( (LDAPControl[]) null );
          else if ( value instanceof LDAPControl )
            cons.setControls( (LDAPControl) value );
          else if ( value instanceof LDAPControl[] )
            cons.setControls( (LDAPControl[])value );
          else
            throw new LDAPException ( "invalid LDAPControl",
                                      LDAPException.PARAM_ERROR );
          return;
        case MAXBACKLOG:
          cons.setMaxBacklog( ((Integer)value).intValue() );
          return;
        default:
          throw new LDAPException ( "invalid option",
                                    LDAPException.PARAM_ERROR );
        }
      } catch ( ClassCastException cc ) {
          throw new LDAPException ( "invalid option value",
                                    LDAPException.PARAM_ERROR );
      }
    }

    /**
     * Send a request to the server
     */
    void sendRequest( JDAPProtocolOp oper, LDAPMessageQueue myListener,
                      LDAPConstraints cons ) throws LDAPException {

        // retry three times if we get a nullPointer exception.
        // Don't remove this. The following situation might happen:
        // Before sendRequest gets invoked, it is possible that the
        // LDAPConnThread encountered a network error and called
        // deregisterConnection which set the thread reference to null.
        boolean requestSent = false;
        for ( int i = 0; !requestSent && (i < 3); i++ ) {
            try {
                _thread.sendRequest( this, oper, myListener, cons );
                requestSent = true;
            } catch( NullPointerException ne ) {
                // do nothing
            } catch (IllegalArgumentException e) {
                throw new LDAPException(e.getMessage(),
                                        LDAPException.PARAM_ERROR);
            }
            
        }
        if ( !isConnected() ) {
            throw new LDAPException( "The connection is not available",
                                     LDAPException.OTHER );
        }
        if ( !requestSent ) {
            throw new LDAPException( "Failed to send request",
                                     LDAPException.OTHER );
        }
    }

    /**
     * Internal routine. Binds to the LDAP server.
     * This method is used by the "smart failover" feature. If a server
     * or network error has occurred, an attempt is made to automatically
     * restore the connection on the next ldap operation request
     * @exception LDAPException failed to bind or the user has disconncted
     */
    private void internalBind( LDAPConstraints cons ) throws LDAPException {
        
        // If the user has invoked disconnect() no attempt is made
        // to restore the connection
        if ( (_connMgr != null) && _connMgr.isUserDisconnected() ) {
            throw new LDAPException( "not connected", LDAPException.OTHER );
        }

        if ( _saslBinder != null ) {
            if ( !isConnected() ) {
                connect();
            }
            _saslBinder.bind( this, false );
            _authMethod = "sasl";
        } else {
            //Rebind using _rebindConstraints
            if ( _rebindConstraints == null ) {
                _rebindConstraints = _defaultConstraints;
            }
            internalBind ( _protocolVersion, false, _rebindConstraints );
        }
    }

    void checkSearchMsg( LDAPSearchResults value, LDAPMessage msg,
                         LDAPSearchConstraints cons, String dn,
                         int scope, String filter,
                         String attrs[], boolean attrsOnly)
        throws LDAPException {

        value.setMsgID( msg.getMessageID() );

        try {
            checkMsg( msg );
            // not the JDAPResult
            if ( msg.getProtocolOp().getType() !=
                 JDAPProtocolOp.SEARCH_RESULT ) {
                value.add( msg );
            }
        } catch ( LDAPReferralException e ) {
            Vector res = new Vector();
            
            try {
                performReferrals( e, cons, JDAPProtocolOp.SEARCH_REQUEST, dn,
                                  scope, filter, attrs, attrsOnly, null, null,
                                  null, res );
            }
            catch ( LDAPException ex ) {
                if ( msg.getProtocolOp() instanceof
                     JDAPSearchResultReference ) {
                   /*
                      Don't want to miss all remaining results, so continue.
                      This is very ugly (using println). We should have a 
                      configurable parameter (probably in LDAPSearchConstraints)
                      whether to ignore failed search references or throw an
                      exception.
                    */
                    System.err.println( "LDAPConnection.checkSearchMsg: " +
                                        "ignoring bad referral" );
                    return;
                }
                throw ex;
            }

            // The size of the vector can be more than 1 because it is possible
            // to visit more than one referral url to retrieve the entries
            for ( int i = 0; i < res.size(); i++ ) {
                value.addReferralEntries( (LDAPSearchResults)res.elementAt(i) );
            }

            res = null;
        } catch ( LDAPException e ) {
            if ( (e.getResultCode() == LDAPException.ADMIN_LIMIT_EXCEEDED) ||
                 (e.getResultCode() == LDAPException.TIME_LIMIT_EXCEEDED) ||
                 (e.getResultCode() == LDAPException.SIZE_LIMIT_EXCEEDED) ) {
                value.add(e);
            } else {
                throw e;
            }
        }
    }

    
    /**
     * Internal routine. Binds to the LDAP server.
     * @param version protocol version to request from server
     * @param rebind true if the bind/authenticate operation is requested,
     * false if the method is invoked by the "smart failover" feature
     * @exception LDAPException failed to bind
     */
    private void internalBind( int version, boolean rebind,
                               LDAPConstraints cons ) throws LDAPException {
        _saslBinder = null;

        if ( !isConnected() ) {
            _bound = false;
            _authMethod = "none";
            connect();
            // special case: the connection currently is not bound, and now
            // there is a bind request. The connection needs to reconnect if
            // the thread has more than one LDAPConnection.
        } else if ( !_bound && rebind && (_thread.getClientCount() > 1) ) {
            disconnect();
            connect();
        }

        // if the connection is still intact and no rebind request
        if ( _bound && !rebind ) {
            return;
        }

        // if the connection was lost and did not have any kind of bind
        // operation and the current one does not request any bind operation (ie,
        // no authenticate has been called)
        if ( !_anonymousBound &&
             ((_boundDN == null) || (_boundPasswd == null)) &&
             !rebind ) {
            return;
        }

        if ( _bound && rebind ) {
            if ( _protocolVersion == version ) {
                if ( _anonymousBound &&
                     ((_boundDN == null) || (_boundPasswd == null)) ) {
                    return;
                }

                if ( !_anonymousBound &&
                     (_boundDN != null) &&
                     (_boundPasswd != null) &&
                     _boundDN.equals(_prevBoundDN) &&
                     equalsBytes(_boundPasswd, _prevBoundPasswd) ) {
                    return;
                }
            }

            // if we got this far, we need to rebind since previous and
            // current credentials are not the same.
            // if the current connection is the only connection of the thread,
            // then reuse this current connection. otherwise, disconnect the
            // current one (ie, detach from the current thread) and reconnect
            // again (ie, get a new thread).
            if ( _thread.getClientCount() > 1 ) {
                disconnect();
                connect();
            }
        }

        _protocolVersion = version;

        LDAPResponseQueue myListener = getResponseListener();
        try {
            if ( (_referralConnection != null) &&
                 _referralConnection.isConnected() ) {
                _referralConnection.disconnect();
            }
            _referralConnection = null;
            _bound = false;
            sendRequest( new JDAPBindRequest( _protocolVersion, _boundDN,
                                              _boundPasswd),
                         myListener, cons );
            checkMsg( myListener.getResponse() );
            markConnAsBound();
            _rebindConstraints = (LDAPConstraints)cons.clone();
            _authMethod = "simple";
        } catch (LDAPReferralException e) {
            _referralConnection = createReferralConnection( e, cons );
        } finally {
            releaseResponseListener( myListener );
        }
    }

    private static boolean equalsBytes( byte[] b1, byte[] b2 ) {
        if ( b1 == null ) {
            return ( b2 == null );
        } else if ( b2 == null ) {
            return true;
        }
        if ( b1.length != b2.length ) {
            return false;
        }
        for( int i = 0; i < b1.length; i++ ) {
            if ( b1[i] != b2[1] ) {
                return false;
            }
        }
        return true;
    }

    /**
     * Mark this connection as bound in the thread connection table
     */
    void markConnAsBound() {
        synchronized (_threadConnTable) {
            if (_threadConnTable.containsKey(_thread)) {
                Vector v = (Vector)_threadConnTable.get(_thread);
                for (int i=0, n=v.size(); i<n; i++) {
                    LDAPConnection conn = (LDAPConnection)v.elementAt(i);
                    conn._bound = true;
                }
            } else {
                printDebug( "Thread table does not contain the thread of " +
                            "this object" );
            }
        }
    }


    /**
     * Evaluate the TRACE_PROPERTY value and create output stream.
     * The value can be of type String, OutputStream or LDAPTraceWriter.
     * The String value represents output file name. If the name is an empty
     * string, the output is sent to System.err. If the file name is
     * prefixed with a '+' character, the file is opened in append mode.
     *
     * @param out Trace output specifier. A file name, an output stream 
     * or an instance of LDAPTraceWriter
     * @return An output stream or an LDAPTraceWriter instance
     */
    Object createTraceOutput( Object out ) throws LDAPException {
                
        if ( out instanceof String ) { // trace file name
            OutputStream os = null;
            String file = (String)out;
            if ( file.length() == 0 ) {
                os = System.err;
            } else {
                try {
                    boolean appendMode = (file.charAt(0) == '+');
                    if ( appendMode ) {
                        file = file.substring(1);
                    }                        
                    FileOutputStream fos =
                        new FileOutputStream( file, appendMode );
                    os = new BufferedOutputStream( fos );
                }
                catch ( IOException e ) {
                    throw new LDAPException(
                        "Can not open output trace file " + file + " " + e );
                }
            }
            return os;
        }        
        else if ( out instanceof OutputStream )  {
            return out;
        }       
        else if ( out instanceof LDAPTraceWriter )  {
            return out;
        } else {
            throw new LDAPException( TRACE_PROPERTY +
                                     " must be an OutputStream, a file " +
                                     "name or an instance of LDAPTraceWriter" );
        }

    }
    
    /**
     * Sets the LDAP protocol version that your client prefers to use when
     * connecting to the LDAP server.
     * <P>
     *
     * @param version the LDAP protocol version that your client uses
     */
    private void setProtocolVersion( int version ) {
        _protocolVersion = version;
    }

    /**
     * Remove this connection from the thread connection table
     */
    private void deleteThreadConnEntry() {
        synchronized ( _threadConnTable ) {
            Vector connVector = (Vector)_threadConnTable.get( _thread );
            if ( connVector == null ) {
                printDebug( "Thread table does not contain the thread of " +
                            "this object" );
                return;
            }
            Enumeration enumv = connVector.elements();
            while ( enumv.hasMoreElements() ) {
                LDAPConnection c = (LDAPConnection)enumv.nextElement();
                if ( c.equals( this ) ) {
                    connVector.removeElement( c );
                    if ( connVector.size() == 0 ) {
                        _threadConnTable.remove( _thread );
                    }
                    return;
                }
            }
        }
    }

    /**
     * Remove the association between this object and the connection thread
     */
    synchronized void deregisterConnection() {
        if ( (_thread != null) && _thread.isRunning() ) {
            _thread.deregister( this );
        }
        _thread = null;
        _bound = false;
    }

    /**
     * Returns the trace output object if set by the user
     */
    Object getTraceOutput() throws LDAPException {
        
        // Check first if trace output has been set using setProperty()
        Object traceOut = _properties.get( TRACE_PROPERTY );
        if ( traceOut != null ) {
            return createTraceOutput( traceOut );
        }
        
        // Check if the property has been set with java
        // -Dcom.org.ietf.ldap.trace
        // If the property does not have a value, send the trace to the
        // System.err, otherwise use the value as the output file name
        try {
            traceOut = System.getProperty( TRACE_PROPERTY );
            if ( traceOut != null ) {
                return createTraceOutput( traceOut );
            }
        }
        catch ( Exception e ) {
            ;// In browser, access to property might not be allowed
        }
        return null;
    }        
        
        
    private synchronized LDAPConnThread getNewThread(LDAPConnSetupMgr connMgr,
                                                     LDAPCache cache)
        throws LDAPException {

        LDAPConnThread newThread = null;
        Vector v = null;

        synchronized( _threadConnTable ) {

            Enumeration keys = _threadConnTable.keys();
            boolean connExists = false;

            // transverse each thread
            while ( keys.hasMoreElements() ) {
                LDAPConnThread connThread = (LDAPConnThread)keys.nextElement();
                Vector connVector = (Vector)_threadConnTable.get( connThread );
                Enumeration enumv = connVector.elements();

                // traverse each LDAPConnection under the same thread
                while ( enumv.hasMoreElements() ) {
                    LDAPConnection conn = (LDAPConnection)enumv.nextElement();

                    // this is not a brand new connection
                    if ( conn.equals( this ) ) {
                        connExists = true;

                        if ( !connThread.isAlive() ) {
                            // need to move all the LDAPConnections from the dead thread
                            // to the new thread
                            try {
                                newThread =
                                    new LDAPConnThread( connMgr, cache,
                                                        getTraceOutput() );
                                v = (Vector)_threadConnTable.remove(
                                    connThread );
                                break;
                            } catch ( Exception e ) {
                                throw new LDAPException(
                                    "unable to establish connection",
                                    LDAPException.UNAVAILABLE );
                            }
                        }
                        break;
                    }
                }

                if ( connExists ) {
                    break;
                }
            }

            // if this connection is new or the corresponding thread for
            // the current connection is dead
            if ( !connExists ) {
                try {
                    newThread = new LDAPConnThread( connMgr, cache,
                                                    getTraceOutput() );
                    v = new Vector();
                    v.addElement( this );
                } catch ( Exception e ) {
                    throw new LDAPException( "unable to establish connection",
                                             LDAPException.UNAVAILABLE );
                }
            }

            // add new thread to the table
            if ( newThread != null ) {
                _threadConnTable.put( newThread, v );
                for ( int i = 0, n = v.size(); i < n; i++ ) {
                    LDAPConnection c = (LDAPConnection)v.elementAt( i );
                    newThread.register( c );
                    c._thread = newThread;
                }
            }
        }

        return newThread;
    }

    /**
     * Performs certificate-based authentication if client authentication was
     * specified at construction time.
     * @exception LDAPException if certificate-based authentication fails.
     */
    private void authenticateSSLConnection() throws LDAPException {

        // if this is SSL
        if ( (_factory != null) &&
             (_factory instanceof LDAPSSLSocketFactoryExt) ) {

            if ( ((LDAPSSLSocketFactoryExt)_factory).isClientAuth() ) {
                bind( null, null, new String[] {EXTERNAL_MECHANISM},
                      EXTERNAL_MECHANISM_PACKAGE, null, null, null );
            }
        }
    }

    /**
     * Gets a new listening agent from the internal buffer of available agents.
     * These objects are used to make the asynchronous LDAP operations
     * synchronous.
     *
     * @return response listener object
     */
    synchronized LDAPResponseQueue getResponseListener() {
        if ( _responseListeners == null ) {
            _responseListeners = new Vector( 5 );
        }

        LDAPResponseQueue l;
        if ( _responseListeners.size() < 1 ) {
            l = new LDAPResponseQueue ( /*asynchOp=*/false );
        }
        else {
            l = (LDAPResponseQueue)_responseListeners.elementAt( 0 );
            _responseListeners.removeElementAt( 0 );
        }
        return l;
    }

    /**
     * Get a new search listening agent from the internal buffer of available
     * agents. These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @return a search response listener object
     */
    private synchronized LDAPSearchQueue getSearchListener (
        LDAPSearchConstraints cons ) {
        if ( _searchListeners == null ) {
            _searchListeners = new Vector( 5 );
        }

        LDAPSearchQueue l;
        if ( _searchListeners.size() < 1 ) {
            l = new LDAPSearchQueue ( /*asynchOp=*/false, cons );
        }
        else {
            l = (LDAPSearchQueue)_searchListeners.elementAt( 0 );
            _searchListeners.removeElementAt( 0 );
            l.setSearchConstraints( cons );
        }
        return l;
    }

    /**
     * Put a listening agent into the internal buffer of available agents.
     * These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @param l listener to buffer
     */
    synchronized void releaseResponseListener( LDAPResponseQueue l ) {
        if ( _responseListeners == null ) {
            _responseListeners = new Vector( 5 );
        }

        l.reset();
        _responseListeners.addElement( l );
    }

    /**
     * Put a search listening agent into the internal buffer of available
     * agents. These objects are used to make the asynchronous LDAP
     * operations synchronous.
     * @param l listener to buffer
     */
    synchronized void releaseSearchListener( LDAPSearchQueue l ) {
        if ( _searchListeners == null ) {
            _searchListeners = new Vector( 5 );
        }

        l.reset();
        _searchListeners.addElement( l );
    }

    /**
     * Checks the message (assumed to be a return value).  If the resultCode
     * is anything other than SUCCESS, it throws an LDAPException describing
     * the server's (error) response.
     * @param m server response to validate
     * @exception LDAPException failed to check message
     */
    void checkMsg( LDAPMessage m ) throws LDAPException {
      if ( m.getProtocolOp() instanceof JDAPResult ) {
          JDAPResult response = (JDAPResult)m.getProtocolOp();
          int resultCode = response.getResultCode();

          if ( resultCode == JDAPResult.SUCCESS ) {
              return;
          }

          if ( resultCode == JDAPResult.REFERRAL ) {
              throw new LDAPReferralException ( "referral", resultCode,
                                                response.getReferrals() );
          }

          if ( resultCode == JDAPResult.LDAP_PARTIAL_RESULTS ) {
              throw new LDAPReferralException ( "referral", resultCode,
                                                response.getErrorMessage() );
          } else {
              throw new LDAPException ( "error result", resultCode,
                                        response.getErrorMessage(),
                                        response.getMatchedDN() );
          }

      } else if ( m.getProtocolOp() instanceof JDAPSearchResultReference ) {
          String[] referrals =
            ((JDAPSearchResultReference)m.getProtocolOp()).getUrls();
          throw new LDAPReferralException ( "referral",
                                            JDAPResult.SUCCESS, referrals );
      } else {
          return;
      }
    }

    /**
     * Set response controls for the current connection for a particular 
     * thread. Get the oldest returned controls and remove them from the 
     * queue. If the connection is executing a persistent search, there may
     * be more than one set of controls in the queue. For any other 
     * operation, there will only ever be at most one set of controls 
     * (controls from any earlier operation are replaced by controls 
     * received on the latest operation on this connection by this thread).
     * @param current the target thread
     * @param con the server response controls
     */
    void setResponseControls( LDAPConnThread current,
                              LDAPResponseControl con ) {
        synchronized(_responseControlTable) {
            Vector v = (Vector)_responseControlTable.get( current );

            // if the current thread already contains response controls from
            // a previous operation
            if ( (v != null) && (v.size() > 0) ) {

                // look at each response control
                for ( int i = v.size() - 1; i >= 0; i-- ) {
                    LDAPResponseControl response = 
                      (LDAPResponseControl)v.elementAt( i );
    
                    // if this response control belongs to this connection
                    if ( response.getConnection().equals( this ) ) {
 
                        // if the given control is null or 
                        // the given control is not null and the current 
                        // control does not correspond to the new LDAPMessage
                        if ( (con == null) || 
                             (con.getMsgID() != response.getMsgID()) ) {
                            v.removeElement( response );
                        }

                        // For the same connection, if the message id from the 
                        // given control is the same as the one in the queue,
                        // those controls in the queue will not get removed
                        // since they come from the persistent search control
                        // which allows more than one control response for
                        // one persistent search operation.
                    }
                }
            } else {
                if ( con != null ) {
                    v = new Vector();
                }
            }          

            if ( con != null ) {
                v.addElement( con );
                _responseControlTable.put( current, v );
            }

            /* Do some garbage collection: check if any attached threads have
               exited */
            /* Now check all threads in the list */
            Enumeration e = _attachedList.elements();
            while( e.hasMoreElements() ) {
                LDAPConnThread aThread = (LDAPConnThread)e.nextElement();
                if ( !aThread.isAlive() ) {
                    _responseControlTable.remove( aThread );
                    _attachedList.removeElement( aThread );
                }
            }
        }
        /* Make sure we're registered */
        if ( _attachedList.indexOf( current ) < 0 ) {
            _attachedList.addElement( current );
        }
    }

    /**
     * Set up connection for referral.
     * @param u referral URL
     * @param cons search constraints
     * @return new LDAPConnection, already connected and authenticated
     */
    private LDAPConnection prepareReferral( String connectList,
                                            LDAPConstraints cons )
        throws LDAPException {
        LDAPConnection connection =
            new LDAPConnection( this.getSocketFactory() );
        
        // Set the same connection setup failover policy as for this connection
        connection.setConnSetupDelay( getConnSetupDelay() );
        
        connection.setOption( REFERRALS, new Boolean(true) );
        connection.setOption( REFERRALS_REBIND_PROC,
                              cons.getReferralHandler() );
  
        // need to set the protocol version which gets passed to connection
        connection.setOption( PROTOCOL_VERSION,
                              new Integer(_protocolVersion) );

        connection.setOption( REFERRALS_HOP_LIMIT,
                              new Integer(cons.getHopLimit()-1) );

        try { 
            connection.connect( connectList, connection.DEFAULT_PORT );
        }
        catch ( LDAPException e ) {
            throw new LDAPException( "Referral connect failed: " +
                                     e.getMessage(),
                                     e.getResultCode() );
        }
        return connection;
    }

    private void referralRebind( LDAPConnection ldc, LDAPConstraints cons )
        throws LDAPException{
        try {
            LDAPReferralHandler handler = cons.getReferralHandler();
            if ( handler == null ) {
                ldc.bind ( _protocolVersion, null, null );
            } else if ( handler instanceof LDAPAuthHandler ) {
                LDAPAuthProvider auth =
                  ((LDAPAuthHandler)handler).getAuthProvider(
                      ldc.getHost(),
                      ldc.getPort());
                ldc.bind( _protocolVersion,
                          auth.getDN(),
                          auth.getPassword() );
            } else if ( handler instanceof LDAPBindHandler ) {
                String[] urls = { "ldap://" + ldc.getHost() + ':' +
                                  ldc.getPort() + '/' };
                ((LDAPBindHandler)handler).bind( urls, ldc);
            }
        }
        catch ( LDAPException e ) {
            throw new LDAPException( "Referral bind failed: " + e.getMessage(),
                                     e.getResultCode() );
        }            
    }
    
    
    private String createReferralConnectList( String[] urls ) {
        String connectList = "";
        String host = null;
        int port = 0;
        
        for ( int i=0; urls != null && i < urls.length; i++ ) {
            try {
                LDAPUrl url = new LDAPUrl( urls[i] );
                host = url.getHost();
                port = url.getPort();
                if ( (host == null) ||
                     (host.length() < 1) ) {
                    // If no host:port was specified, use the latest
                    // (hop-wise) parameters
                    host = getHost();
                    port = getPort();
                }
            } catch ( Exception e ) {
                host = getHost();
                port = getPort();
            }
            connectList += (i==0 ? "" : " ") + host+":"+port;
        }
        
        return (connectList.length() == 0) ? null : connectList;
    }

    private LDAPUrl findReferralURL( LDAPConnection ldc, String[] urls ) {
        String connHost = ldc.getHost();
        int    connPort = ldc.getPort();
        for ( int i = 0; i < urls.length; i++ ) {
            try {
                LDAPUrl url = new LDAPUrl( urls[i] );
                String host = url.getHost();
                int port = url.getPort();
                if ( host == null ||
                     host.length() < 1 ) {
                    // No host:port specified, compare with the latest
                    // (hop-wise) parameters 
                    if ( connHost.equals( getHost() ) &&
                         connPort == getPort() ) {
                        return url;
                    }
                } else if ( connHost.equals( host ) &&
                            connPort == port ) {
                    return url;
                }
            } catch ( Exception e ) {
            }
        }
        return null;
    }
    
    /**
     * Establish the LDAPConnection to the referred server. This one is used
     * for bind operation only since we need to keep this new connection for
     * the subsequent operations. This new connection will be destroyed in
     * two circumstances: disconnect gets called or the client binds as
     * someone else.
     * @return the new LDAPConnection to the referred server
     */
    LDAPConnection createReferralConnection( LDAPReferralException e,
                                             LDAPConstraints cons )
        throws LDAPException {
        if ( cons.getHopLimit() <= 0 ) {
            throw new LDAPException( "exceed hop limit",
                                     e.getResultCode(),
                                     e.getLDAPErrorMessage() );
        }
        if ( !cons.getReferralFollowing() ) {
            throw e;
        }

        String connectList = 
            createReferralConnectList( e.getReferrals() );
        // If there are no referrals (because the server isn't set up for
        // them), give up here
        if ( connectList == null ) {
            throw new LDAPException( "No target URL in referral",
                                     LDAPException.NO_RESULTS_RETURNED );
        }

        LDAPConnection connection = null;
        connection = prepareReferral( connectList, cons );
        try {
            connection.bind( _protocolVersion, _boundDN, _boundPasswd );
        } catch ( LDAPException authEx ) {
            // Disconnect needed to terminate the LDAPConnThread
            try  {                
                connection.disconnect();
            } catch ( LDAPException ignore ) {
            }
            throw authEx;
        }
        return connection;
    }

    /**
     * Follows a referral
     *
     * @param e referral exception
     * @param cons search constraints
     */
    void performReferrals( LDAPReferralException e,
                           LDAPConstraints cons, int ops,
                           /* unions of different operation parameters */
                           String dn, int scope, String filter, String types[],
                           boolean attrsOnly, LDAPModification mods[],
                           LDAPEntry entry,
                           LDAPAttribute attr,
                           /* result */
                           Vector results )
        throws LDAPException {

        if ( cons.getHopLimit() <= 0 ) {
            throw new LDAPException( "exceed hop limit",
                                     e.getResultCode(),
                                     e.getLDAPErrorMessage() );
        }
        if ( !cons.getReferralFollowing() ) {
            if ( ops == JDAPProtocolOp.SEARCH_REQUEST ) {
                LDAPSearchResults res = new LDAPSearchResults();
                res.add( e );
                results.addElement( res );
                return;
            } else {
                throw e;
            }
        }

        String[] urls = e.getReferrals();
        // If there are no referrals (because the server isn't configured to
        // return one), give up here
        if ( (urls == null) || (urls.length == 0) ) {
            return;
        }

        LDAPUrl referralURL = null;
        LDAPConnection connection = null;

        if ( (_referralConnection != null) &&
             _referralConnection.isConnected() ) {
            referralURL = findReferralURL( _referralConnection, urls );
        }
        if ( referralURL != null ) {
            connection = _referralConnection;
        }
        else {
            String connectList = createReferralConnectList( urls );
            connection = prepareReferral( connectList, cons );
                
            // which one did we connect to...
            referralURL = findReferralURL( connection, urls );
                
            // Authenticate
            referralRebind( connection, cons );
        }

        String newDN = referralURL.getDN();
        String DN = null;
        if ( (newDN == null) || newDN.equals("") ) {
            DN = dn;
        } else {
            DN = newDN;
        }
        // If this was a one-level search, and a direct subordinate
        // has a referral, there will be a "?base" in the URL, and
        // we have to rewrite the scope from one to base
        if ( referralURL.toString().indexOf("?base") > -1 ) {
            scope = SCOPE_BASE;
        }

        LDAPSearchConstraints newcons = (LDAPSearchConstraints)cons.clone();
        newcons.setHopLimit( cons.getHopLimit()-1 );

        performReferrals( connection, newcons, ops, DN, scope, filter,
                          types, attrsOnly, mods, entry, attr, results );
    }

    void performReferrals( LDAPConnection connection, 
                           LDAPConstraints cons, int ops, String dn, int scope,
                           String filter, String types[], boolean attrsOnly,
                           LDAPModification mods[], LDAPEntry entry,
                           LDAPAttribute attr,
                           Vector results ) throws LDAPException {
 
        LDAPSearchResults res = null;
        try {
            switch ( ops ) {
                case JDAPProtocolOp.SEARCH_REQUEST:

                    res = connection.search( dn, scope, filter,
                                             types, attrsOnly, 
                                             (LDAPSearchConstraints)cons );
                    if ( res != null ) {
                        res.closeOnCompletion( connection );
                        results.addElement( res );
                    } else {
                        if ( (_referralConnection == null) ||
                             !connection.equals(_referralConnection) ) {
                            connection.disconnect();
                        }
                    }
                    break;
                case JDAPProtocolOp.MODIFY_REQUEST:
                    connection.modify( dn, mods, cons );
                    break;
                case JDAPProtocolOp.ADD_REQUEST:
                    if ( (dn != null) && !dn.equals("") )
                         entry.setDN( dn );
                    connection.add( entry, cons );
                    break;
                case JDAPProtocolOp.DEL_REQUEST:
                    connection.delete( dn, cons );
                    break;
                case JDAPProtocolOp.MODIFY_RDN_REQUEST:
                    connection.rename( dn, filter /* newRDN */, 
                                       attrsOnly /* deleteOld */, cons );
                    break;
                case JDAPProtocolOp.COMPARE_REQUEST:
                    boolean bool = connection.compare( dn, attr, cons );
                    results.addElement( new Boolean(bool) );
                    break;
                default:
                    /* impossible */
                    break;
            }
        } catch ( LDAPException ee ) {
            throw ee;
        } finally {
            if ( (connection != null) && 
                 ((ops != JDAPProtocolOp.SEARCH_REQUEST) || (res == null)) &&
                 ((_referralConnection == null) || 
                  !connection.equals(_referralConnection)) ) {
                connection.disconnect();
            }
        }
    }

    /**
     * Follows a referral for an extended operation
     *
     * @param e referral exception
     * @param cons search constraints
     */
    private LDAPExtendedOperation performExtendedReferrals(
        LDAPReferralException e,
        LDAPConstraints cons, LDAPExtendedOperation op )
        throws LDAPException {

        if ( cons.getHopLimit() <= 0 ) {
            throw new LDAPException( "exceed hop limit",
                                     e.getResultCode(),
                                     e.getLDAPErrorMessage() );
        }
        if ( !cons.getReferralFollowing() ) {
            throw e;
        }

        String[] u = e.getReferrals();
        // If there are no referrals (because the server isn't configured to
        // return one), give up here
        if ( (u == null) || (u.length == 0) ) {
            return null;
        }

        String connectList = createReferralConnectList( u );
        LDAPConnection connection = prepareReferral( connectList, cons );
        referralRebind( connection, cons );
        LDAPExtendedOperation results =
            connection.extendedOperation( op );
        connection.disconnect();
        return results; /* return right away if operation is successful */
        
    }

    /**
     * Sets up basic connection privileges for Communicator.
     * @return true if in Communicator and connections okay.
     */
    private static boolean checkCommunicator() {
        try {
            java.lang.reflect.Method m = LDAPCheckComm.getMethod(
                "netscape.security.PrivilegeManager", "enablePrivilege" );
            if ( m == null ) {
                printDebug( "Method is null" );
                return false;
            }

            Object[] args = new Object[1];
            args[0] = new String( "UniversalConnect" );
            m.invoke( null, args);
            printDebug( "UniversalConnect enabled" );
            args[0] = new String( "UniversalPropertyRead" );
            m.invoke( null, args);
            printDebug( "UniversalPropertyRead enabled" );
            return true;
        } catch ( LDAPException e ) {
            printDebug( "Exception: " + e.toString() );
        } catch ( Exception ie ) {
            printDebug( "Exception on invoking " + "enablePrivilege: " +
                        ie.toString() );
        }
        return false;
    }

    /**
     * Reports if the class is running in a Netscape browser.
     * @return <CODE>true</CODE> if the class is running in a Netscape browser.
     */
    public static boolean isNetscape() {
        return isCommunicator;
    }

    static void printDebug( String msg ) {
        if ( debug ) {
            System.out.println( msg );
        }
    }

    /**
     * Returns the string representation for this <CODE>LDAPConnection</CODE>.
     * <P>
     * For example:
     *
     * <PRE>LDAPConnection {ldap://dilly:389 (2) ldapVersion:3 bindDN:
     * "uid=admin,o=iplanet.com"}</PRE>
     * 
     * For cloned connections, the number of LDAPConnection instances sharing
     * the same physical connection is shown in parenthesis following the ldap
     * url. 
     * If an LDAPConnection objectis not cloned, this number is omitted from
     * the string representation.
     *
     * @return string representation of the connection
     * @see org.ietf.ldap.LDAPConnection#clone
     */    
    public String toString() {
        int cloneCnt = (_thread == null) ? 0 : _thread.getClientCount();
        StringBuffer sb = new StringBuffer( "LDAPConnection {" );
        //url
//        sb.append( (isSecure() ? "ldaps" : "ldap") );
        sb.append( "ldap" );
        sb.append( "://" );
        sb.append( getHost() );
        sb.append( ":" );
        sb.append( getPort() );
        // clone count
        if ( cloneCnt > 1 ) {
            sb.append( " (" );
            sb.append( cloneCnt );
            sb.append( ")" );
        }
        // ldap version
        sb.append( " ldapVersion:" );
        sb.append( _protocolVersion );
        // bind DN
        sb.append( " bindDN:\"" );
        if ( getAuthenticationDN() != null ) {
            sb.append( getAuthenticationDN() );
        }
        sb.append( "\"}" );
        
        return sb.toString();
    }
                                                                     
    /**
     * Prints out the LDAP Java SDK version and the highest LDAP
     * protocol version supported by the SDK. To view this
     * information, open a terminal window, and enter:
     * <PRE>java org.ietf.ldap.LDAPConnection
     * </PRE>
     * @param args not currently used
     */
    public static void main(String[] args) {
        System.out.println( "LDAP SDK Version is " + SdkVersion );
        System.out.println( "LDAP Protocol Version is " + ProtocolVersion );
    }

    /**
     * Option specifying the maximum number of unread results to be cached in
     * any LDAPSearchResults without suspending reading from the server
     * @see org.ietf.ldap.LDAPConnection#getOption
     * @see org.ietf.ldap.LDAPConnection#setOption
     */
    public static final int MAXBACKLOG = 30;

    private static boolean isCommunicator = checkCommunicator();
    private static boolean debug = false;
}
