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
import javax.security.auth.callback.CallbackHandler;

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
 * the search method when the results are not all returned at
 * the same time).
 * <P>
 *
 * This class also specifies a default set of constraints
 * (such as the maximum amount of time an operation is allowed before timing out)
 * which apply to all operations. To get and set these constraints,
 * use the <CODE>getOption</CODE> and <CODE>setOption</CODE> methods.
 * To override these constraints for an individual operation,
 * define a new set of constraints by creating a <CODE>LDAPConstraints</CODE>
 * object and pass the object to the method for the operation. For search 
 * operations, additional constraints are defined in <CODE>LDAPSearchConstraints</CODE>
 * (a subclass of <CODE>LDAPConstraints</CODE>). To override the default search
 * constraints, create a <CODE>LDAPSearchConstraints</CODE> object and pass it
 * to the <CODE>search</CODE> method.
 * <P>
 *
 * If you set up your client to follow referrals automatically,
 * an operation that results in a referral will create a new connection
 * to the LDAP server identified in the referral.  In order to have
 * your client authenticate to that LDAP server automatically, you need
 * to define a class that implements the <CODE>LDAPRebind</CODE> interface.
 * In your definition of the class, you need to define a
 * <CODE>getRebindAuthentication</CODE> method that creates an <CODE>LDAPRebindAuth</CODE>
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
 * @see netscape.ldap.LDAPConstraints
 * @see netscape.ldap.LDAPSearchConstraints
 * @see netscape.ldap.LDAPRebind
 * @see netscape.ldap.LDAPRebindAuth
 * @see netscape.ldap.LDAPException
 */
public class LDAPConnection
       implements LDAPv3, LDAPAsynchronousConnection, Cloneable {

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
     * use the <A HREF="#authenticate(int, java.lang.String, java.lang.String)"><CODE>authenticate(int version, String dn, String passwd)</CODE></A> method.
     * For example:
     * <P>
     *
     * <PRE>
     * ld.authenticate( 3, myDN, myPW );
     * </PRE>
     *
     * @see netscape.ldap.LDAPConnection#authenticate(int, java.lang.String, java.lang.String)
     */
    public final static int LDAP_VERSION              = 2;
    /**
     * Name of the property specifying the version of the SDK. <P>
     *
     * To get the version number, pass this name to the
     * <CODE>getProperty</CODE> method.  The SDK version number
     * is a <CODE>Float</CODE> type. For example:<P>
     * <PRE>
     *      ...
     *      Float sdkVersion = ( Float )myConn.getProperty( myConn.LDAP_PROPERTY_SDK );
     *      System.out.println( "SDK version: " + sdkVersion );
     *      ... </PRE>
     * @see netscape.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_SDK      = "version.sdk";
    /**
     * Name of the property specifying the highest supported version of
     * the LDAP protocol. <P>
     *
     * To get the version number, pass this name to the
     * <CODE>getProperty</CODE> method.  The LDAP protocol version number
     * is a <CODE>Float</CODE> type. For example:<P>
     * <PRE>
     *      ...
     *      Float LDAPVersion = ( Float )myConn.getProperty( myConn.LDAP_PROPERTY_PROTOCOL );
     *      System.out.println( "Highest supported LDAP protocol version: " + LDAPVersion );
     *      ... </PRE>
     * @see netscape.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_PROTOCOL = "version.protocol";
    /**
     * Name of the property specifying the types of authentication allowed by this
     * API (for example, anonymous authentication and simple authentication). <P>
     *
     * To get the supported types, pass this name to the
     * <CODE>getProperty</CODE> method.  The value of this property is
     * a <CODE>String</CODE> type. For example:<P>
     * <PRE>
     *      ...
     *      String authTypes = ( String )myConn.getProperty( myConn.LDAP_PROPERTY_SECURITY );
     *      System.out.println( "Supported authentication types: " + authTypes );
     *      ... </PRE>
     * @see netscape.ldap.LDAPConnection#getProperty(java.lang.String)
     */
    public final static String LDAP_PROPERTY_SECURITY = "version.security";

    /**
     * Specifies the serial connection setup policy when a list of hosts is
     * passed to  the <CODE>connect</CODE> method.
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay(int)
     */    
    public final static int NODELAY_SERIAL = -1;
    /**
     * Specifies the parallel connection setup policy with no delay when a
     * list of hosts is passed to the <CODE>connect</CODE> method.
     * For each host in the list, a separate thread is created to attempt
     * to connect to the host. All threads are started simultaneously.
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay(int)
     */    
    public final static int NODELAY_PARALLEL = 0;

    /**
     * Constants
     */
    private final static String defaultFilter = "(objectClass=*)";
    private final static LDAPConstraints readConstraints = new
      LDAPSearchConstraints();

    /**
     * Internal variables
     */
    private LDAPSearchConstraints m_defaultConstraints =
        new LDAPSearchConstraints();
    private Vector m_responseListeners;
    private Vector m_searchListeners;
    private boolean m_bound;
    private String m_prevBoundDN;
    private String m_prevBoundPasswd;
    private String m_boundDN;
    private String m_boundPasswd;
    private int m_protocolVersion = LDAP_VERSION;
    private LDAPConnSetupMgr m_connMgr;
    private int m_connSetupDelay = -1;
    private LDAPSocketFactory m_factory;
    /* m_thread does all socket i/o for the object and any clones */
    private LDAPConnThread m_thread = null;
    /* To manage received server controls on a per-thread basis,
       we keep a table of active threads and a table of controls,
       indexed by thread */
    private Vector m_attachedList = new Vector();
    private Hashtable m_responseControlTable = new Hashtable();
    private LDAPCache m_cache = null;
    static Hashtable m_threadConnTable = new Hashtable();

    // this handles the case when the client lost the connection with the
    // server. After the client reconnects with the server, the bound resets
    // to false. If the client used to have anonymous bind, then this boolean
    // will take care of the case whether the client should send anonymous bind
    // request to the server.
    private boolean m_anonymousBound = false;

    private Object m_security = null;
    private LDAPSaslBind m_saslBinder = null;
    private Properties m_securityProperties;
    private Hashtable m_properties = new Hashtable();
    private LDAPConnection m_referralConnection;

    /**
     * Properties
     */
    private final static Float SdkVersion = new Float(4.02f);
    private final static Float ProtocolVersion = new Float(3.0f);
    private final static String SecurityVersion = new String("none,simple,sasl");
    private final static Float MajorVersion = new Float(4.0f);
    private final static Float MinorVersion = new Float(0.2f);
    private final static String DELIM = "#";
    private final static String PersistSearchPackageName =
      "netscape.ldap.controls.LDAPPersistSearchControl";
    final static String EXTERNAL_MECHANISM = "external";
    private final static String EXTERNAL_MECHANISM_PACKAGE =
      "com.netscape.sasl";
    final static String DEFAULT_SASL_PACKAGE =
      "com.netscape.sasl";
    final static String SCHEMA_BUG_PROPERTY =
      "com.netscape.ldap.schema.quoting";
    final static String SASL_PACKAGE_PROPERTY =
      "com.netscape.ldap.saslpackage";

   /**
    * Constructs a new <CODE>LDAPConnection</CODE> object,
    * which represents a connection to an LDAP server. <P>
    *
    * Calling the constructor does not actually establish
    * the connection.  To connect to the LDAP server, use the
    * <CODE>connect</CODE> method.
    *
    * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int)
    * @see netscape.ldap.LDAPConnection#authenticate(java.lang.String, java.lang.String)
    */
    public LDAPConnection () {
        super();
        m_factory = null;

        m_properties.put(LDAP_PROPERTY_SDK, SdkVersion); 
        m_properties.put(LDAP_PROPERTY_PROTOCOL, ProtocolVersion); 
        m_properties.put(LDAP_PROPERTY_SECURITY, SecurityVersion); 
        m_properties.put("version.major", MajorVersion); 
        m_properties.put("version.minor", MinorVersion);
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
     * @see netscape.ldap.LDAPSocketFactory
     * @see netscape.ldap.LDAPSSLSocketFactory
     * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int)
     * @see netscape.ldap.LDAPConnection#authenticate(java.lang.String, java.lang.String)
     * @see netscape.ldap.LDAPConnection#getSocketFactory
     * @see netscape.ldap.LDAPConnection#setSocketFactory(netscape.ldap.LDAPSocketFactory)
     */
    public LDAPConnection ( LDAPSocketFactory factory ) {
        this();
        m_factory = factory;
    }

    /**
     * Finalize method, which disconnects from the LDAP server.
     * @exception LDAPException Thrown when the connection cannot be disconnected.
     */
    public void finalize() throws LDAPException {
        if (isConnected()) {
            disconnect();
        }
    }

    /**
     *  Sets the specified <CODE>LDAPCache</CODE> object as the
     *  cache for the <CODE>LDAPConnection</CODE> object.
     *  <P>
     *
     *  @param cache The <CODE>LDAPCache</CODE> object representing
     *   the cache you want used by the current connection.
     *  @see netscape.ldap.LDAPCache
     *  @see netscape.ldap.LDAPConnection#getCache
     */
    public void setCache(LDAPCache cache) {
        m_cache = cache;
        if ( m_thread != null ) {
            m_thread.setCache( cache );
        }
    }

    /**
     * Gets the <CODE>LDAPCache</CODE> object associated with
     * the current <CODE>LDAPConnection</CODE> object.
     * <P>
     *
     * @return The <CODE>LDAPCache</CODE> object representing
     *   the cache used by the current connection.
     * @see netscape.ldap.LDAPCache
     * @see netscape.ldap.LDAPConnection#setCache(netscape.ldap.LDAPCache)
     */
    public LDAPCache getCache() {
        return m_cache;
    }

    /**
     * Gets a property of a connection. <P>
     *
     * You can get the following properties for a given connection: <P>
     * <UL TYPE="DISC">
     * <LI><CODE>LDAP_PROPERTY_SDK</CODE> <P>
     * To get the version of this SDK, get this property.  The value of
     * this property is a <CODE>Float</CODE> data type. <P>
     * <LI><CODE>LDAP_PROPERTY_PROTOCOL</CODE> <P>
     * To get the highest supported version of the LDAP protocol, get
     * this property.
     * The value of this property is a <CODE>Float</CODE> data type. <P>
     * <LI><CODE>LDAP_PROPERTY_SECURITY</CODE> <P>
     * To get a comma-separated list of the types of authentication
     * supported, get this property.  The value of this property is a
     * <CODE>String</CODE>. <P>
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
     * @param name Name of the property (for example,
     * <CODE>LDAP_PROPERTY_SDK</CODE>). <P>
     *
     * @return The value of the property. <P>
     *
     * Since the return value is an object, you
     * should recast it as the appropriate type.
     * (For example, when getting the <CODE>LDAP_PROPERTY_SDK</CODE> property,
     * recast the return value as a <CODE>Float</CODE>.) <P>
     *
     * If you pass this method an unknown property name, the method
     * returns null. <P>
     *
     * @exception LDAPException Unable to get the value of the
     * specified property. <P>
     *
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_SDK
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_PROTOCOL
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_SECURITY
     */
    public Object getProperty(String name) throws LDAPException {
        return m_properties.get( name );
    }

    /**
     * The following properties are defined:<BR> 
     * com.netscape.ldap.schema.quoting - "standard" or "NetscapeBug"<BR> 
     * Note: if this property is not set, the SDK will query the server 
     * to determine if attribute syntax values and objectclass superior 
     * values must be quoted when adding schema.<BR>
     * com.netscape.ldap.saslpackage - the default is "com.netscape.sasl"<BR> 
     * <P>
     *
     * @param name Name of the property that you want to set.
     * @param val Value that you want to set.
     * @exception LDAPException Unable to set the value of the specified
     * property.
     */
    public void setProperty(String name, Object val) throws LDAPException {
        if ( name.equalsIgnoreCase( SCHEMA_BUG_PROPERTY ) ) { 
            m_properties.put( SCHEMA_BUG_PROPERTY, val ); 
        } else if ( name.equalsIgnoreCase( SASL_PACKAGE_PROPERTY ) ) {
            m_properties.put( SASL_PACKAGE_PROPERTY, val ); 
        } else if ( name.equalsIgnoreCase( "debug" ) ) {
            debug = ((String)val).equalsIgnoreCase( "true" ); 
        } else {
            throw new LDAPException("Unknown property: " + name);
        }
    }

    /**
     * Sets the LDAP protocol version that your client prefers to use when
     * connecting to the LDAP server.
     * <P>
     *
     * @param version The LDAP protocol version that your client uses.
     */
    private void setProtocolVersion(int version) {
        m_protocolVersion = version;
    }

    /**
     * Returns the host name of the LDAP server to which you are connected.
     * @return Host name of the LDAP server.
     */
    public String getHost () {
        if (m_connMgr != null) {
            return m_connMgr.getHost();
        }
        return null;
    }

    /**
     * Returns the port number of the LDAP server to which you are connected.
     * @return Port number of the LDAP server.
     */
    public int getPort () {
        if (m_connMgr != null) {
            return m_connMgr.getPort();
        }
        return -1;
    }

    /**
     * Returns the distinguished name (DN) used for authentication over
     * this connection.
     * @return Distinguished name used for authentication over this connection.
     */
    public String getAuthenticationDN () {
        return m_boundDN;
    }

    /**
     * Returns the password used for authentication over this connection.
     * @return Password used for authentication over this connection.
     */
    public String getAuthenticationPassword () {
        return m_boundPasswd;
    }
    
    /**
     * Returns the delay in seconds when making concurrent connection attempts to
     * multiple servers.
     * @return The delay in seconds between connection attempts:<br>
     * <CODE>NODELAY_SERIAL</CODE> The serial connection setup policy is enabled
     * (no concurrency).<br>
     * <CODE>NODELAY_PARALLEL</CODE> The parallel connection setup policy with no delay
     *  is enabled.<br>
     * <CODE>delay > 0</CODE> The parallel connection setup policy with the delay of 
     * <CODE>delay</CODE> seconds is enabled.
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     */
    public int getConnSetupDelay () {
        return m_connSetupDelay;
    }

    /**
     * Specifies the delay in seconds when making concurrent connection attempts to
     * multiple servers.
     * <P>Effectively, selects the connection setup policy when a list of hosts is passed
     * to the <CODE>connect</CODE>method.
     * 
     * <br>If the serial policy, the default one, is selected, an attempt is made to
     * connect to the first host in the list in the specified order. Next entry in
     * the list is tried only after the connection attempt to the current one fails.
     * This might cause your application to block for unacceptably long time if a host is down.
     *
     * <br>If the parallel policy is selected, multiple connection attempts may run
     * concurrently on a separate thread. A new connection attempt to the next entry
     * in the list can be started with or without delay.
     * <P> The <CODE>ConnSetupDelay</CODE> must be set before the call to the <CODE>connect</CODE>
     *  method. 
     * 
     * @param delay The delay in seconds between connection attempts. Possible values are:<br>
     * <CODE>NODELAY_SERIAL</CODE> Use the serial connection setup policy.<br>
     * <CODE>NODELAY_PARALLEL</CODE> Use the parallel connection setup policy with no delay.
     * Start all connection setup threads immediately.<br>
     * <CODE>delay > 0</CODE> Use the parallel connection setup policy with delay.
     * Start another connection setup thread after <CODE>delay</CODE> seconds.<br>
     * @see netscape.ldap.LDAPConnection#NODELAY_SERIAL
     * @see netscape.ldap.LDAPConnection#NODELAY_PARALLEL
     * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int)
     */
    public void setConnSetupDelay (int delay) {
        m_connSetupDelay = delay;
    }

    /**
     * Gets the object representing the socket factory used to establish
     * a connection to the LDAP server.
     * <P>
     *
     * @return The object representing the socket factory used to
     *  establish a connection to a server.
     * @see netscape.ldap.LDAPSocketFactory
     * @see netscape.ldap.LDAPSSLSocketFactory
     * @see netscape.ldap.LDAPConnection#setSocketFactory(netscape.ldap.LDAPSocketFactory)
     */
    public LDAPSocketFactory getSocketFactory () {
        return m_factory;
    }

    /**
     * Specifies the object representing the socket factory that you
     * want to use to establish a connection to a server.
     * <P>
     *
     * @param factory The object representing the socket factory that
     *  you want to use to establish a connection to a server.
     * @see netscape.ldap.LDAPSocketFactory
     * @see netscape.ldap.LDAPSSLSocketFactory
     * @see netscape.ldap.LDAPConnection#getSocketFactory
     */
    public void setSocketFactory (LDAPSocketFactory factory) {
        m_factory = factory;
    }

    /**
     * Indicates whether the connection represented by this object
     * is open at this time.
     * @return If connected to an LDAP server over this connection,
     * returns <CODE>true</CODE>.  If not connected to an LDAP server,
     * returns <CODE>false</CODE>.
     */
    public boolean isConnected() {
        // This is the hack: If the user program calls isConnected() when
        // the thread is about to shut down, the isConnected might get called
        // before the deregisterConnection(). We add the yield() so that 
        // the deregisterConnection() will get called first. 
        // This problem only exists on Solaris.
        Thread.yield();
        return (m_thread != null);
    }

    /**
     * Indicates whether this client has authenticated to the LDAP server
     * @return If authenticated, returns <CODE>true</CODE>. If not
     * authenticated, or if authenticated as an anonymous user (with
     * either a blank name or password), returns <CODE>false</CODE>.
     */
    public boolean isAuthenticated () {
        if (m_bound) {
            if ((m_boundDN == null) || m_boundDN.equals("") ||
                (m_boundPasswd == null) || m_boundPasswd.equals("")) {
                return false;
            }
        }
        return m_bound;
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
     *
     * @param host Host name of the LDAP server that you want to connect to.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). The connection setup policy specified with
     * the <CODE>ConnSetupDelay</CODE> property controls whether connection
     * attempts are made serially or concurrently. For example, you can specify
     *  the following values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * @param port Port number of the LDAP server that you want to connect to.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @exception LDAPException The connection failed.
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     */
    public void connect(String host, int port) throws LDAPException {
        connect( host, port, null, null, m_defaultConstraints, false );
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
     *     switch( e.getLDAPResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified user does not exist." );
     *             break;
     *         case e.INVALID_CREDENTIALS:
     *             System.out.println( "Invalid password." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Failed to connect to " + ldapHost + " at port " + ldapPort );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Connected to " + ldapHost + " at port " + ldapPort );
     * </PRE>
     *
     * @param host Host name of the LDAP server that you want to connect to.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). The connection setup policy specified with
     * the <CODE>ConnSetupDelay</CODE> property controls whether connection
     * attempts are made serially or concurrently. For example, you can specify
     *  the following values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * @param port Port number of the LDAP server that you want to connect to.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @param dn Distinguished name used for authentication
     * @param passwd Password used for authentication
     * @exception LDAPException The connection or authentication failed.
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     */
    public void connect(String host, int port, String dn, String passwd)
        throws LDAPException {
        connect(host, port, dn, passwd, m_defaultConstraints, true);
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server.  If this LDAPConnection object
     * represents an open connection, the connection is closed first
     * before the new connection is opened. This method allows the user to 
     * specify the preferences for the bind operation.
     * 
     * @param host Host name of the LDAP server that you want to connect to.
     * This value can also be a space-delimited list of hostnames or
     * hostnames and port numbers (using the syntax
     * <I>hostname:portnumber</I>). The connection setup policy specified with
     * the <CODE>ConnSetupDelay</CODE> property controls whether connection
     * attempts are made serially or concurrently. For example, you can specify
     *  the following values for the <CODE>host</CODE> argument:<BR>
     *<PRE>
     *   myhost
     *   myhost hishost:389 herhost:5000 whathost
     *   myhost:686 myhost:389 hishost:5000 whathost:1024
     *</PRE>
     * @param port Port number of the LDAP server that you want to connect to.
     * This parameter is ignored for any host in the <CODE>host</CODE>
     * parameter which includes a colon and port number.
     * @param dn Distinguished name used for authentication
     * @param passwd Password used for authentication
     * @param cons Preferences for the bind operation.
     * @exception LDAPException The connection or authentication failed.
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     */
    public void connect(String host, int port, String dn, String passwd,
        LDAPConstraints cons) throws LDAPException {
        connect(host, port, dn, passwd, cons, true);
    }

    private void connect(String host, int port, String dn, String passwd,
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
        m_connMgr = new LDAPConnSetupMgr(hostList, portList, m_factory,
                                         m_connSetupDelay);
    
        connect();

        if (doAuthenticate) {
            authenticate(dn, passwd, cons);
        }
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server, with the specified LDAP
     * protocol version. If the server does not support the requested
     * protocol version, an exception is thrown. If this LDAPConnection
     * object represents an open connection, the connection is closed first
     * before the new connection is opened. This is equivalent to
     * <CODE>connect(host, port)</CODE> followed by <CODE>authenticate(version, dn, passwd)</CODE>.<P>
     *
     * @param version LDAP protocol version requested: currently 2 or 3
     * @param host Contains a hostname or dotted string representing
     *   the IP address of a host running an LDAP server to connect to.
     *   Alternatively, it may contain a list of host names, space-delimited.
     *   Each host name may include a trailing colon and port number. In the
     *   case where more than one host name is specified, the connection setup
     *  policy specified with the <CODE>ConnSetupDelay</CODE> property controls
     *  whether connection attempts are made serially or concurrently.<P>
     *
     * <PRE>
     *   Examples:
     *      "directory.knowledge.com"
     *      "199.254.1.2"
     *      "directory.knowledge.com:1050 people.catalog.com 199.254.1.2"
     * </PRE>
     * @param port Contains the TCP or UDP port number to connect to or contact.
     *   The default LDAP port is 389. "port" is ignored for any host name which
     *   includes a colon and port number.
     * @param dn If non-null and non-empty, specifies that the connection and
     *   all operations through it should be authenticated with dn as the
     *   distinguished name.
     * @param passwd If non-null and non-empty, specifies that the connection and
     *   all operations through it should be authenticated with dn as the
     *   distinguished name and passwd as password.
     * @exception LDAPException The connection or authentication failed.
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     */
    public void connect(int version, String host, int port, String dn,
        String passwd) throws LDAPException {

        connect(version, host, port, dn, passwd, m_defaultConstraints);
    }

    /**
     * Connects to the specified host and port and uses the specified DN and
     * password to authenticate to the server, with the specified LDAP
     * protocol version. If the server does not support the requested
     * protocol version, an exception is thrown. This method allows the user
     * to specify preferences for the bind operation. If this LDAPConnection
     * object represents an open connection, the connection is closed first
     * before the new connection is opened. This is equivalent to
     * <CODE>connect(host, port)</CODE> followed by <CODE>authenticate(version, dn, passwd)</CODE>.<P>
     *
     * @param version LDAP protocol version requested: currently 2 or 3
     * @param host Contains a hostname or dotted string representing
     *   the IP address of a host running an LDAP server to connect to.
     *   Alternatively, it may contain a list of host names, space-delimited.
     *   Each host name may include a trailing colon and port number. In the
     *   case where more than one host name is specified, the connection setup
     *  policy specified with the <CODE>ConnSetupDelay</CODE> property controls
     *  whether connection attempts are made serially or concurrently.<P>
     *
     * <PRE>
     *   Examples:
     *      "directory.knowledge.com"
     *      "199.254.1.2"
     *      "directory.knowledge.com:1050 people.catalog.com 199.254.1.2"
     * </PRE>
     * @param port Contains the TCP or UDP port number to connect to or contact.
     *   The default LDAP port is 389. "port" is ignored for any host name which
     *   includes a colon and port number.
     * @param dn If non-null and non-empty, specifies that the connection and
     *   all operations through it should be authenticated with dn as the
     *   distinguished name.
     * @param passwd If non-null and non-empty, specifies that the connection and
     *   all operations through it should be authenticated with dn as the
     *   distinguished name and passwd as password.
     * @param cons Preferences for the bind operation.
     * @exception LDAPException The connection or authentication failed.
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     */
    public void connect(int version, String host, int port, String dn,
        String passwd, LDAPConstraints cons) throws LDAPException {

        setProtocolVersion(version);
        connect(host, port, dn, passwd, cons);
    }

    /**
     * Internal routine to connect with internal params
     * @exception LDAPException failed to connect
     */
    private synchronized void connect () throws LDAPException {
        if ( isConnected() ) {
            return;
        }

        if (m_connMgr == null) {
            throw new LDAPException ( "no connection parameters",
                                      LDAPException.PARAM_ERROR );
        }        

        m_connMgr.openConnection();              
        m_thread = getNewThread(m_connMgr, m_cache);
        authenticateSSLConnection();
    }

    private synchronized LDAPConnThread getNewThread(LDAPConnSetupMgr connMgr,
                                                     LDAPCache cache)
        throws LDAPException {

        LDAPConnThread newThread = null;
        Vector v = null;

        synchronized(m_threadConnTable) {

            Enumeration keys = m_threadConnTable.keys();
            boolean connExists = false;

            // transverse each thread
            while (keys.hasMoreElements()) {
                LDAPConnThread connThread = (LDAPConnThread)keys.nextElement();
                Vector connVector = (Vector)m_threadConnTable.get(connThread);
                Enumeration enumv = connVector.elements();

                // traverse each LDAPConnection under the same thread
                while (enumv.hasMoreElements()) {
                    LDAPConnection conn = (LDAPConnection)enumv.nextElement();

                    // this is not a brand new connection
                    if (conn.equals(this)) {
                        connExists = true;

                        if (!connThread.isAlive()) {
                            // need to move all the LDAPConnections from the dead thread
                            // to the new thread
                            try {
                                newThread = new LDAPConnThread(connMgr, cache);
                                v = (Vector)m_threadConnTable.remove(connThread);
                                break;
                            } catch (Exception e) {
                                throw new LDAPException ("unable to establish connection",
                                                         LDAPException.UNAVAILABLE);
                            }
                        }
                        break;
                    }
                }

                if (connExists) {
                    break;
                }
            }

            // if this connection is new or the corresponding thread for the current
            // connection is dead
            if (!connExists) {
                try {
                    newThread = new LDAPConnThread(connMgr, cache);
                    v = new Vector();
                    v.addElement(this);
                } catch (Exception e) {
                    throw new LDAPException ("unable to establish connection",
                        LDAPException.UNAVAILABLE);
                }
            }

            // add new thread to the table
            if (newThread != null) {
                m_threadConnTable.put(newThread, v);
                for (int i=0, n=v.size(); i<n; i++) {
                    LDAPConnection c = (LDAPConnection)v.elementAt(i);
                    newThread.register(c);
                    c.m_thread = newThread;
                }
            }
        }

        return newThread;
    }

    /**
     * Do certificate-based authentication if client authentication was
     * specified at construction time.
     * @exception LDAPException if certificate-based authentication fails.
     */
    private void authenticateSSLConnection() throws LDAPException {

        // if this is SSL
        if ((m_factory != null) &&
            (m_factory instanceof LDAPSSLSocketFactoryExt)) {

            if (((LDAPSSLSocketFactoryExt)m_factory).isClientAuth()) {
                authenticate(null, EXTERNAL_MECHANISM,
                             EXTERNAL_MECHANISM_PACKAGE, null, null);
            }
        }
    }

    /**
     * Abandons a current search operation, notifying the server not
     * to send additional search results.
     *
     * @param searchResults The search results returned when the search
     * was started.
     * @exception LDAPException Failed to notify the LDAP server.
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     * @see netscape.ldap.LDAPSearchResults
     */
    public void abandon( LDAPSearchResults searchResults )
                             throws LDAPException {
        if ( (!isConnected()) || (searchResults == null) ) {
            return;
        }

        searchResults.abandon();

        int id = searchResults.getID();
        abandon(id);        
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password.
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
     *     myConn.authenticate( myDN, myPW );
     * } catch ( LDAPException e ) {
     *     switch( e.getLDAPResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified user does not exist." );
     *             break;
     *         case e.INVALID_CREDENTIALS:
     *             System.out.println( "Invalid password." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Failed to authentice as " + myDN );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Authenticated as " + myDN );
     * </PRE>
     *
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, String passwd) throws LDAPException {
        authenticate(m_protocolVersion, dn, passwd, m_defaultConstraints);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password. The
     * default protocol version (version 2) is used. If the server
     * doesn't support the default version, an LDAPException is thrown
     * with the error code PROTOCOL_ERROR. This method allows the
     * user to specify the preferences for the bind operation.
     *
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @param cons Preferences for the bind operation.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, String passwd,
      LDAPConstraints cons) throws LDAPException {
        authenticate(m_protocolVersion, dn, passwd, cons);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password, and
     * requesting that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR.
     *
     * @param version Required LDAP protocol version.
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(int version, String dn, String passwd)
      throws LDAPException {
        authenticate(version, dn, passwd, m_defaultConstraints);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password, and
     * requesting that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR. This method allows the user to specify the
     * preferences for the bind operation.
     *
     * @param version Required LDAP protocol version.
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @param cons Preferences for the bind operation.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(int version, String dn, String passwd,
      LDAPConstraints cons) throws LDAPException {
        m_prevBoundDN = m_boundDN;
        m_prevBoundPasswd = m_boundPasswd;
        m_boundDN = dn;
        m_boundPasswd = passwd;

        m_anonymousBound =
            ((m_prevBoundDN == null) || (m_prevBoundPasswd == null));

        internalBind (version, true, cons);
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and whatever SASL mechanisms
     * are supported by the server. Each supported mechanism in turn
     * is tried until authentication succeeds or an exception is thrown.
     * If the object has been disconnected from an LDAP server, this
     * method attempts to reconnect to the server. If the object had
     * already authenticated, the old authentication is discarded.
     *
     * @param dn If non-null and non-empty, specifies that the connection and
     * all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param cbh A class which may be called by the SASL framework to
     * obtain additional information.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, Hashtable props,
                             CallbackHandler cbh)
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
        authenticate( dn, attr.getStringValueArray(), props, cbh );
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn If non-null and non-empty, specifies that the connection and
     * all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param mechanisms A list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param cbh A class which may be called by the SASL framework to
     * obtain additional information required.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @see netscape.ldap.LDAPConnection#authenticate(java.lang.String,
     * java.util.Hashtable, javax.security.auth.callback.CallbackHandler)
     */
    public void authenticate(String dn, String[] mechanisms,
                             Hashtable props, CallbackHandler cbh)
        throws LDAPException {

        authenticate( dn, mechanisms, DEFAULT_SASL_PACKAGE, props, cbh );
    }
    
    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn If non-null and non-empty, specifies that the connection and
     * all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param mechanism A single mechanism name, e.g. "GSSAPI".
     * @param packageName A package which contains a SASL ClientFactory,
     * e.g. "myclasses.SASL". If null, a system default is used.
     * @param cbh A class which may be called by the SASL framework to
     * obtain additional information required.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @deprecated Please use authenticate without packageName
     * instead.
     */
    public void authenticate(String dn, String mechanism, String packageName,
                             Hashtable props, CallbackHandler cbh)
        throws LDAPException {

        authenticate( dn, new String[] {mechanism}, packageName, props, cbh );
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn If non-null and non-empty, specifies that the connection and
     * all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param mechanisms A list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param packageName A package which contains a SASL ClientFactory,
     * e.g. "myclasses.SASL". If null, a system default is used.
     * @param cbh A class which may be called by the SASL framework to
     * obtain additional information required.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @deprecated Please use authenticate without packageName
     * instead.
     */
    public void authenticate(String dn, String[] mechanisms,
                             String packageName,
                             Hashtable props, CallbackHandler cbh)
        throws LDAPException {

        if ( !isConnected() ) {
            connect();
        } else {
            // If the thread has more than one LDAPConnection, then
            // we should disconnect first. Otherwise,
            // we can reuse the same thread for the rebind.
            if (m_thread.getClientCount() > 1) {
                disconnect();
                connect();
            }
        }
        m_boundDN = null;
        m_protocolVersion = 3; // Must be 3 for SASL
        if ( props == null ) {
            props = new Hashtable();
        }
        m_saslBinder = new LDAPSaslBind( dn, mechanisms, packageName,
                                         props, cbh );
        m_saslBinder.bind( this );
        m_boundDN = dn;
    }
    
    /** 
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password.
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
     *     myConn.bind( myDN, myPW );
     * } catch ( LDAPException e ) {
     *     switch( e.getLDAPResultCode() ) {
     *         case e.NO_SUCH_OBJECT:
     *             System.out.println( "The specified user does not exist." );
     *             break;
     *         case e.INVALID_CREDENTIALS:
     *             System.out.println( "Invalid password." );
     *             break;
     *         default:
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Failed to authentice as " + myDN );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Authenticated as " + myDN );
     * </PRE>
     *
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind(String dn, String passwd) throws LDAPException {
        authenticate(m_protocolVersion, dn, passwd, m_defaultConstraints);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password. The
     * default protocol version (version 2) is used. If the server
     * doesn't support the default version, an LDAPException is thrown
     * with the error code PROTOCOL_ERROR. This method allows the
     * user to specify the preferences for the bind operation.
     *
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @param cons Preferences for the bind operation.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind(String dn, String passwd,
                     LDAPConstraints cons) throws LDAPException {
        authenticate(m_protocolVersion, dn, passwd, cons);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password, and
     * requesting that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR.
     *
     * @param version Required LDAP protocol version.
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind(int version, String dn, String passwd)
        throws LDAPException {
        authenticate(version, dn, passwd, m_defaultConstraints);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password, and
     * requesting that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR. This method allows the user to specify the
     * preferences for the bind operation.
     *
     * @param version Required LDAP protocol version.
     * @param dn Distinguished name used for authentication.
     * @param passwd Password used for authentication.
     * @param cons Preferences for the bind operation.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind(int version, String dn, String passwd,
      LDAPConstraints cons) throws LDAPException {
        authenticate(version, dn, passwd, cons);
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and whatever SASL mechanisms
     * are supported by the server. Each supported mechanism in turn
     * is tried until authentication succeeds or an exception is thrown.
     * If the object has been disconnected from an LDAP server, this
     * method attempts to reconnect to the server. If the object had
     * already authenticated, the old authentication is discarded.
     *
     * @param dn If non-null and non-empty, specifies that the connection and
     * all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param cbh A class which may be called by the SASL framework to
     * obtain additional information.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind(String dn, Hashtable props,
                     CallbackHandler cbh)
        throws LDAPException {
        authenticate(dn, props, cbh);
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param dn If non-null and non-empty, specifies that the connection and
     * all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param mechanisms A list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param cbh A class which may be called by the SASL framework to
     * obtain additional information.
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @see netscape.ldap.LDAPConnection#bind(java.lang.String,
     * java.util.Hashtable, javax.security.auth.callback.CallbackHandler)
     */
    public void bind(String dn, String[] mechanisms,
                     Hashtable props, CallbackHandler cbh)
        throws LDAPException {
        authenticate(dn, mechanisms, props, cbh);
    }

    /**
     * Internal routine. Binds to the LDAP server.
     * @param version protocol version to request from server
     * @param rebind True if the bind/authenticate operation is requested,
     * false if the method is invoked by the "smart failover" feature
     * @exception LDAPException failed to bind
     */
    private void internalBind (int version, boolean rebind,
      LDAPConstraints cons) throws LDAPException {
        m_saslBinder = null;

        if (!isConnected()) {
            m_bound = false;
            connect ();
            // special case: the connection currently is not bound, and now there is
            // a bind request. The connection needs to reconnect if the thread has
            // more than one LDAPConnection.
        } else if (!m_bound && rebind && m_thread.getClientCount() > 1) {
            disconnect();
            connect();
        }

        // if the connection is still intact and no rebind request
        if (m_bound && !rebind) {
            return;
        }

        // if the connection was lost and did not have any kind of bind
        // operation and the current one does not request any bind operation (ie,
        // no authenticate has been called)
        if (!m_anonymousBound &&
            ((m_boundDN == null) || (m_boundPasswd == null)) &&
            !rebind) {
            return;
        }

        if (m_bound && rebind) {
            if (m_protocolVersion == version) {
                if (m_anonymousBound && ((m_boundDN == null) || (m_boundPasswd == null)))
                    return;

                if (!m_anonymousBound && (m_boundDN != null) && (m_boundPasswd != null) &&
                    m_boundDN.equals(m_prevBoundDN) &&
                    m_boundPasswd.equals(m_prevBoundPasswd)) {
                    return;
                }
            }

            // if we got this far, we need to rebind since previous and
            // current credentials are not the same.
            // if the current connection is the only connection of the thread,
            // then reuse this current connection. otherwise, disconnect the current
            // one (ie, detach from the current thread) and reconnect again (ie, get
            // a new thread).
            if (m_thread.getClientCount() > 1) {
                disconnect();
                connect();
            }
        }

        m_protocolVersion = version;

        LDAPResponseListener myListener = getResponseListener ();
        try {
            if (m_referralConnection != null) {
                m_referralConnection.disconnect();
                m_referralConnection = null;
            }
            sendRequest(new JDAPBindRequest(m_protocolVersion, m_boundDN,
                                            m_boundPasswd),
                        myListener, cons);
            checkMsg( myListener.getResponse() );
        } catch (LDAPReferralException e) {
            m_referralConnection = createReferralConnection(e, cons);
        } finally {
            releaseResponseListener(myListener);
        }

        updateThreadConnTable();
    }

    /**
     * Mark this connection as bound in the thread connection table
     */
    void updateThreadConnTable() {
        if (m_threadConnTable.containsKey(m_thread)) {
            Vector v = (Vector)m_threadConnTable.get(m_thread);
            for (int i=0, n=v.size(); i<n; i++) {
                LDAPConnection conn = (LDAPConnection)v.elementAt(i);
                conn.m_bound = true;
            }
        } else {
            printDebug("Thread table does not contain the thread of this object");
        }
    }

    /**
     * Send a request to the server
     */
    void sendRequest(JDAPProtocolOp oper, LDAPMessageQueue myListener,
                     LDAPConstraints cons) throws LDAPException {

        // retry three times if we get a nullPointer exception.
        // Don't remove this. The following situation might happen:
        // Before sendRequest gets invoked, it is possible that the LDAPConnThread
        // encountered a network error and called deregisterConnection which
        // set the thread reference to null.
        for (int i=0; i<3; i++) {
            try {
                m_thread.sendRequest(this, oper, myListener, cons);
                break;
            } catch(NullPointerException ne) {
                // do nothing
            }
        }
        if (!isConnected()) {
            throw new LDAPException("The connection is not available",
                LDAPException.OTHER);
        }
    }

    /**
     * Internal routine. Binds to the LDAP server.
     * This method is used by the "smart failover" feature. If a server
     * or network error has occurred, an attempt is made to automatically
     * restore the connection on the next ldap operation request
     * @exception LDAPException failed to bind or the user has disconncted
     */
    private void internalBind (LDAPConstraints cons) throws LDAPException {
        
        // If the user has invoked disconnect() no attempt is made
        // to restore the connection
        if (m_connMgr != null && m_connMgr.isUserDisconnected()) {
            throw new LDAPException("not connected", LDAPException.OTHER);
        }

        if (m_saslBinder != null) {
            if ( !isConnected() ) {
                connect();
            }
            m_saslBinder.bind(this, false);
        } else {
            internalBind (m_protocolVersion, false, cons);
        }
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
        
        if (m_saslBinder != null) {
            m_saslBinder.bind(this, true);
        } else {
            internalBind (m_protocolVersion, true, m_defaultConstraints);
        }
    }

    /**
     * Disconnects from the LDAP server. Before you can perform LDAP operations
     * again, you need to reconnect to the server by calling
     * <CODE>connect</CODE>.
     * @exception LDAPException Failed to disconnect from the LDAP server.
     * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int)
     * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int, java.lang.String, java.lang.String)
     */
    public synchronized void disconnect() throws LDAPException {
        if (m_referralConnection != null) {
            m_referralConnection.disconnect();
            m_referralConnection = null;
        }

        if (!isConnected())
            throw new LDAPException ( "unable to disconnect() without connecting",
                                      LDAPException.OTHER );
        
        // Clone the Connection Setup Manager if the connection is shared
        if (m_thread.getClientCount() > 1) {
            m_connMgr = (LDAPConnSetupMgr) m_connMgr.clone();
            m_connMgr.disconnect();
        }
    
        if (m_cache != null) {
            m_cache.cleanup();
            m_cache = null;
        }
        deleteThreadConnEntry();
        deregisterConnection();
    }

    /**
     * Remove this connection from the thread connection table
     */
    private void deleteThreadConnEntry() {
        Enumeration keys = m_threadConnTable.keys();
        while (keys.hasMoreElements()) {
            LDAPConnThread connThread = (LDAPConnThread)keys.nextElement();
            Vector connVector = (Vector)m_threadConnTable.get(connThread);
            Enumeration enumv = connVector.elements();
            while (enumv.hasMoreElements()) {
                LDAPConnection c = (LDAPConnection)enumv.nextElement();
                if (c.equals(this)) {
                    connVector.removeElement(c);
                    if (connVector.size() == 0) {
                        m_threadConnTable.remove(connThread);
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
        m_thread.deregister(this);
        m_thread = null;
        m_bound = false;
    }

    /**
     * Reads the entry for the specified distiguished name (DN) and retrieves all
     * attributes for the entry.
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
     *     switch( e.getLDAPResultCode() ) {
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
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Could not read the specified entry." );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Found the specified entry." );
     * </PRE>
     *
     * @param DN Distinguished name of the entry that you want to retrieve.
     * @exception LDAPException Failed to find or read the specified entry
     * from the directory.
     * @return LDAPEntry Returns the specified entry or raises an exception
     * if the entry is not found.
     */
    public LDAPEntry read (String DN) throws LDAPException {
        return read (DN, null, m_defaultConstraints);
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
     *     switch( e.getLDAPResultCode() ) {
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
     *             System.out.println( "Error number: " + e.getLDAPResultCode() );
     *             System.out.println( "Could not read the specified entry." );
     *             break;
     *     }
     *     return;
     * }
     * System.out.println( "Found the specified entry." );
     * </PRE>
     *
     * @param DN Distinguished name of the entry that you want to retrieve.
     * @param cons Preferences for the read operation.
     * @exception LDAPException Failed to find or read the specified entry
     * from the directory.
     * @return LDAPEntry Returns the specified entry or raises an exception
     * if the entry is not found.
     */
    public LDAPEntry read (String DN, LDAPSearchConstraints cons)
        throws LDAPException {
        return read (DN, null, cons);
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
     *      switch( e.getLDAPResultCode() ) {
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
     *               System.out.println( "Error number: " + e.getLDAPResultCode() );
     *               System.out.println( "Could not read the specified entry." );
     *               break;
     *      }
     *      return;
     * }
     *
     * LDAPAttributeSet foundAttrs = foundEntry.getAttributeSet();
     * int size = foundAttrs.size();
     * Enumeration enumAttrs = foundAttrs.getAttributes();
     * System.out.println( "Attributes: " );
     *
     * while ( enumAttrs.hasMoreElements() ) {
     *      LDAPAttribute anAttr = ( LDAPAttribute )enumAttrs.nextElement();
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
     * @param DN Distinguished name of the entry that you want to retrieve.
     * @param attrs Names of attributes that you want to retrieve.
     * @return LDAPEntry Returns the specified entry (or raises an
     * exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     */
    public LDAPEntry read (String DN, String attrs[]) throws LDAPException {
        return read(DN, attrs, m_defaultConstraints);
    }

    public LDAPEntry read (String DN, String attrs[],
        LDAPSearchConstraints cons) throws LDAPException {
        LDAPSearchResults results = search (DN, SCOPE_BASE,
                                            defaultFilter, attrs, false, cons);
        if (results == null) {
            return null;
        }
        return results.next ();
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
     *    int errCode = e.getLDAPResultCode();
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
     * @param toGet LDAP URL specifying the entry that you want to read.
     * @return LDAPEntry Returns the entry specified by the URL (or raises
     * an exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     * @see netscape.ldap.LDAPUrl
     * @see netscape.ldap.LDAPConnection#search(netscape.ldap.LDAPUrl)
     */
    public static LDAPEntry read (LDAPUrl toGet) throws LDAPException {
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
        connection.connect (host, port);

        returnValue = connection.read (DN, attributes);
        connection.disconnect ();

        return returnValue;
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
     *    int errCode = e.getLDAPResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     *
     * while ( myResults.hasMoreElements() ) {
     *    LDAPEntry myEntry = myResults.next();
     *    String nextDN = myEntry.getDN();
     *    System.out.println( nextDN );
     *    LDAPAttributeSet entryAttrs = myEntry.getAttributeSet();
     *    Enumeration attrsInSet = entryAttrs.getAttributes();
     *    while ( attrsInSet.hasMoreElements() ) {
     *        LDAPAttribute nextAttr = (LDAPAttribute)attrsInSet.nextElement();
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
     * @param toGet LDAP URL representing the search that you want to perform.
     * @return LDAPSearchResults The results of the search as an enumeration.
     * @exception LDAPException Failed to complete the search specified by
     * the LDAP URL.
     * @see netscape.ldap.LDAPUrl
     * @see netscape.ldap.LDAPSearchResults
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
     */
    public static LDAPSearchResults search (LDAPUrl toGet) throws LDAPException {
        return search (toGet, null);
    }

    /**
     * Performs the search specified by the LDAP URL. This method also
     * allows you to specify constraints for the search (such as the
     * maximum number of entries to find or the
     * maximum time to wait for search results). <P>
     *
     * As part of the search constraints, you can specify whether or not you
     * want the results delivered all at once or in smaller batches.
     * If you specify that you want the results delivered in smaller
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
     *    int errCode = e.getLDAPResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param toGet LDAP URL representing the search that you want to run.
     * @param cons Constraints specific to the search.
     * @return LDAPSearchResults The results of the search as an enumeration.
     * @exception LDAPException Failed to complete the search specified
     * by the LDAP URL.
     * @see netscape.ldap.LDAPUrl
     * @see netscape.ldap.LDAPSearchResults
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
     */
    public static LDAPSearchResults search (LDAPUrl toGet,
        LDAPSearchConstraints cons) throws LDAPException {
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
     *    myResults = myConn.search( myBaseDN, LDAPv2.SCOPE_SUB, myFilter, myAttrs, false );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getLDAPResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     *
     * while ( myResults.hasMoreElements() ) {
     *    LDAPEntry myEntry = myResults.next();
     *    String nextDN = myEntry.getDN();
     *    System.out.println( nextDN );
     *    LDAPAttributeSet entryAttrs = myEntry.getAttributeSet();
     *    Enumeration attrsInSet = entryAttrs.getAttributes();
     *    while ( attrsInSet.hasMoreElements() ) {
     *        LDAPAttribute nextAttr = (LDAPAttribute)attrsInSet.nextElement();
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
     * @param base The base distinguished name to search from
     * @param scope The scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter Search filter specifying the search criteria.
     * @param attrs List of attributes that you want returned in the
     * search results.
     * @param attrsOnly If true, returns the names but not the values of the
     * attributes found.  If false, returns the names and values for
     * attributes found
     * @return LDAPSearchResults The results of the search as an enumeration.
     * @exception LDAPException Failed to complete the specified search.
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
     */
    public LDAPSearchResults search( String base, int scope, String filter,
        String[] attrs, boolean attrsOnly ) throws LDAPException {
        return search( base, scope, filter, attrs, attrsOnly, m_defaultConstraints);
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
     *    myResults = myConn.search( myBaseDN, LDAPv2.SCOPE_SUB, myFilter, myAttrs, false, mySearchConstraints );
     * } catch ( LDAPException e ) {
     *    int errCode = e.getLDAPResultCode();
     *    System.out.println( "LDAPException: return code:" + errCode );
     *    return;
     * }
     * </PRE>
     * <P>
     *
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base The base distinguished name to search from
     * @param scope The scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter Search filter specifying the search criteria.
     * @param attrs List of attributes that you want returned in the search
     * results.
     * @param cons Constraints specific to this search (for example, the
     * maximum number
     * of entries to return).
     * @param attrsOnly If true, returns the names but not the values of the
     * attributes found.  If false, returns the names and  values for
     * attributes found
     * @return LDAPSearchResults The results of the search as an enumeration.
     * @exception LDAPException Failed to complete the specified search.
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
     */
    public LDAPSearchResults search( String base, int scope, String filter,
        String[] attrs, boolean attrsOnly, LDAPSearchConstraints cons )
        throws LDAPException {

        if (cons == null) {
            cons = m_defaultConstraints;
        }

        LDAPSearchResults returnValue =
            new LDAPSearchResults(this, cons, base, scope, filter,
                                  attrs, attrsOnly);
        Vector cacheValue = null;
        Long key = null;
        boolean isKeyValid = true;

        try {
            // get entry from cache which is a vector of JDAPMessages
            if (m_cache != null) {
                // create key for cache entry using search arguments
                key = m_cache.createKey(getHost(), getPort(),base, filter,
                                        scope, attrs, m_boundDN, cons);

                cacheValue = (Vector)m_cache.getEntry(key);

                if (cacheValue != null) {
                    return (new LDAPSearchResults(cacheValue, this, cons, base, scope,
                                                  filter, attrs, attrsOnly));
                }
            }
        } catch (LDAPException e) {
            isKeyValid = false;
            printDebug("Exception: "+e);
        }

        internalBind(cons);

        LDAPSearchListener myListener = getSearchListener ( cons );
        int deref = cons.getDereference();

        JDAPSearchRequest request = null;        
        try {
            request = new JDAPSearchRequest (base, scope, deref,
                cons.getMaxResults(), cons.getServerTimeLimit(),
                attrsOnly, filter, attrs);
        } catch (IllegalArgumentException e) {
            throw new LDAPException(e.getMessage(), LDAPException.PARAM_ERROR);
        }

        synchronized(myListener) {
            boolean success = false;
            try {
                sendRequest (request, myListener, cons);
                success = true;
            } finally {
                if (!success) {
                    releaseSearchListener (myListener);
                }
            }

            // if using cache, then need to add the key to the search listener.
            // The search listener retrieves the key and then add the key and
            // a vector of results to the hashtable.
            if ((m_cache != null) && (isKeyValid))
                myListener.setKey(key);
        }


        /* Synchronous search if all requested at once */
        if ( cons.getBatchSize() == 0 ) {
            try {
                /* Block until all results are in */
                LDAPMessage response = myListener.completeSearchOperation();
                Enumeration results = myListener.getAllMessages().elements();

                try {
                    checkSearchMsg(returnValue, response, cons, base, scope,
                                   filter, attrs, attrsOnly);
                } catch ( LDAPException ex ) {
                    /* Was the exception caused by a bad referral? */
                    JDAPProtocolOp op = response.getProtocolOp();
                    if ( (op instanceof JDAPSearchResultReference) ||
                         (op instanceof JDAPSearchResult) ){
                        System.err.println( "LDAPConnection.checkSearchMsg: " +
                                            "ignoring bad referral" );
                    } else {
                        throw ex;
                    }
                }

                while (results.hasMoreElements ()) {
                    LDAPMessage msg = (LDAPMessage)results.nextElement();

                    checkSearchMsg(returnValue, msg, cons, base, scope, filter, attrs,
                        attrsOnly);
                }
            } catch (LDAPException ee) {
                throw ee;
            } finally {
                releaseSearchListener (myListener);
            }
        } else {
            /*
            * Asynchronous to retrieve one at a time, check to make sure
            * the search didn't fail
            */
            LDAPMessage firstResult = myListener.nextMessage ();
            if (firstResult instanceof LDAPResponse) {
                try {
                    checkSearchMsg(returnValue, firstResult, cons, base, scope,
                      filter, attrs, attrsOnly);
                } finally {
                    releaseSearchListener (myListener);
                }
            } else {
                /* First result could be a bad referral, ultimately causing
                   a NO_SUCH_OBJECT exception. Don't want to miss all
                   following results, so skip it. */
                try {
                    checkSearchMsg(returnValue, firstResult, cons, base,
                                   scope, filter, attrs, attrsOnly);
                } catch ( LDAPException ex ) {
                    /* Was the exception caused by a bad referral? */
                    if (firstResult.getProtocolOp() instanceof
                        JDAPSearchResultReference) {
                        System.err.println( "LDAPConnection.checkSearchMsg: " +
                                            "ignoring bad referral" );
                    } else {
                        throw ex;
                    }
                }

                LDAPControl[] controls = (LDAPControl[])getOption(LDAPv3.SERVERCONTROLS, cons);

                for (int i=0; (controls != null) && (i<controls.length); i++) {
                    if ((controls[i].getClass().getName()).equals(PersistSearchPackageName))
                    {
                        returnValue.associatePersistentSearch (myListener);
                        return returnValue;
                    }
                }
                /* we let this listener get garbage collected.. */
                returnValue.associate (myListener);
            }
        }
        return returnValue;
    }

    void checkSearchMsg(LDAPSearchResults value, LDAPMessage msg,
        LDAPSearchConstraints cons, String dn, int scope, String filter,
        String attrs[], boolean attrsOnly) throws LDAPException {

        value.setResponseControls(msg.getControls());

        try {
            checkMsg (msg);
            // not the JDAPResult
            if (msg.getProtocolOp().getType() != JDAPProtocolOp.SEARCH_RESULT) {
                value.add (msg);
            }
        } catch (LDAPReferralException e) {
            Vector res = new Vector();
            performReferrals(e, cons, JDAPProtocolOp.SEARCH_REQUEST, dn,
                scope, filter, attrs, attrsOnly, null, null, null, res);

            // the size of the vector can be more than 1 because it is possible
            // to visit more than one referral url to retrieve the entries
            for (int i=0; i<res.size(); i++) {
                value.addReferralEntries((LDAPSearchResults)res.elementAt(i));
            }

            res = null;
        } catch (LDAPException e) {
            if ((e.getLDAPResultCode() == LDAPException.ADMIN_LIMIT_EXCEEDED) ||
                (e.getLDAPResultCode() == LDAPException.TIME_LIMIT_EXCEEDED) ||
                (e.getLDAPResultCode() == LDAPException.SIZE_LIMIT_EXCEEDED)) {
                value.add(e);
            } else {
                throw e;
            }
        }
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
     * @param DN The distinguished name of the entry that you want to use in
     * the comparison.
     * @param attr The attribute that you want to compare against the entry.
     * (The method checks to see if the entry has an attribute with the same name
     * and value as this attribute.)
     * @return true if the entry contains the specified attribute and value.
     * @exception LDAPException Failed to perform the comparison.
     * @see netscape.ldap.LDAPAttribute
     */
    public boolean compare( String DN, LDAPAttribute attr )
        throws LDAPException {
        return compare(DN, attr, m_defaultConstraints);
    }

    public boolean compare( String DN, LDAPAttribute attr,
        LDAPConstraints cons) throws LDAPException {
        internalBind(cons);

        LDAPResponseListener myListener = getResponseListener ();
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
     * attributes of the new entry.
     * @exception LDAPException Failed to add the specified entry to the
     * directory.
     * @see netscape.ldap.LDAPEntry
     */
    public void add( LDAPEntry entry ) throws LDAPException {
        add(entry, m_defaultConstraints);
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
     * attributes of the new entry.
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to add the specified entry to the
     * directory.
     * @see netscape.ldap.LDAPEntry
     * @see netscape.ldap.LDAPConstraints
     */
    public void add( LDAPEntry entry, LDAPConstraints cons )
        throws LDAPException {
        internalBind (cons);

        LDAPResponseListener myListener = getResponseListener ();
        LDAPAttributeSet attrs = entry.getAttributeSet ();
        LDAPAttribute[] attrList = new LDAPAttribute[attrs.size()];
        for( int i = 0; i < attrs.size(); i++ ) {
            attrList[i] = (LDAPAttribute)attrs.elementAt( i );
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
     * Performs an extended operation on the directory.  Extended operations
     * are part of version 3 of the LDAP protocol.<P>
     *
     * Note that in order for the extended operation to work, the server
     * that you are connecting to must support LDAP v3 and must be configured
     * to process the specified extended operation.
     *
     * @param op LDAPExtendedOperation object specifying the OID of the
     * extended operation and the data to be used in the operation.
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see netscape.ldap.LDAPExtendedOperation
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op )
        throws LDAPException {

        return extendedOperation(op, m_defaultConstraints);
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
     * extended operation and the data to be used in the operation.
     * @param cons Preferences for the extended operation.
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see netscape.ldap.LDAPExtendedOperation
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op,
                                                    LDAPConstraints cons)
        throws LDAPException {
        internalBind (cons);

        LDAPResponseListener myListener = getResponseListener ();
        LDAPMessage response = null;
        byte[] results = null;
        String resultID;

        try {
            sendRequest ( new JDAPExtendedRequest( op.getID(), op.getValue() ),
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
     * Makes a single change to an existing entry in the directory
     * (for example, changes the value of an attribute, adds a new
     * attribute value, or removes an existing attribute value). <P>
     *
     * Use the <CODE>LDAPModification</CODE> object to specify the change
     * that needs to be made and the <CODE>LDAPAttribute</CODE> object
     * to specify the attribute value that needs to be changed. The
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
     * @param DN The distinguished name of the entry that you want to modify
     * @param mod A single change to be made to the entry
     * @exception LDAPException Failed to make the specified change to the
     * directory entry.
     * @see netscape.ldap.LDAPModification
     */
    public void modify( String DN, LDAPModification mod ) throws LDAPException {
        modify(DN, mod, m_defaultConstraints);
    }

   /**
    * Makes a single change to an existing entry in the directory and
    * allows you to specify preferences for this LDAP modify operation
    * by using an <CODE>LDAPConstraints</CODE> object. For
    * example, you can specify whether or not to follow referrals.
    * You can also apply LDAP v3 controls to the operation.
    * <P>
    *
    * @param DN The distinguished name of the entry that you want to modify
    * @param mod A single change to be made to the entry
    * @param cons The set of preferences that you want applied to this operation.
    * @exception LDAPException Failed to make the specified change to the
    * directory entry.
    * @see netscape.ldap.LDAPModification
    * @see netscape.ldap.LDAPConstraints
    */
    public void modify( String DN, LDAPModification mod,
        LDAPConstraints cons ) throws LDAPException {
        LDAPModification[] mods = { mod };
        modify (DN, mods, cons);
    }

    /**
     * Makes a set of changes to an existing entry in the directory
     * (for example, changes attribute values, adds new attribute values,
     * or removes existing attribute values). <P>
     *
     * Use the <CODE>LDAPModificationSet</CODE> object to specify the set
     * of changes that needs to be made.  Changes are specified in terms
     * of attribute values.  Each attribute value to be modified, added,
     * or removed must be specified by an <CODE>LDAPAttribute</CODE> object.
     * <P>
     *
     * For example, the following section of code changes Barbara Jensen's
     * title, adds a telephone number to the entry, and removes the room
     * number from the entry. <P>
     *
     * <PRE>
     * ...
     * String myEntryDN = "cn=Barbara Jensen,ou=Product Development,o=Ace Industry,c=US";
     *
     * LDAPModificationSet manyChanges = new LDAPModificationSet();
     * LDAPAttribute attrTelephoneNumber = new LDAPAttribute( "telephoneNumber",
     *                                                        "555-1212" );
     * manyChanges.add( LDAPModification.ADD, attrTelephoneNumber );
     * LDAPAttribute attrRoomNumber = new LDAPAttribute( "roomnumber", "222" );
     * manyChanges.add( LDAPModification.DELETE, attrRoomNumber );
     * LDAPAttribute attrTitle = new LDAPAttribute( "title",
     *                                       "Manager of Product Development" );
     * manyChanges.add( LDAPModification.REPLACE, attrTitle );
     *
     * myConn.modify( myEntryDN, manyChanges );
     * ... </PRE>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mods A set of changes to be made to the entry
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModificationSet
     */
    public void modify (String DN, LDAPModificationSet mods)
        throws LDAPException {
        modify(DN, mods, m_defaultConstraints);
    }

    /**
     * Makes a set of changes to an existing entry in the directory and
     * allows you to specify preferences for this LDAP modify operation
     * by using an <CODE>LDAPConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mods A set of changes to be made to the entry
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModificationSet
     * @see netscape.ldap.LDAPConstraints
     */
     public void modify (String DN, LDAPModificationSet mods,
         LDAPConstraints cons) throws LDAPException {
         LDAPModification[] modList = new LDAPModification[mods.size()];
         for( int i = 0; i < mods.size(); i++ ) {
             modList[i] = mods.elementAt( i );
         }
         modify (DN, modList, cons);
     }

    /**
     * Makes a set of changes to an existing entry in the directory
     * (for example, changes attribute values, adds new attribute values,
     * or removes existing attribute values). <P>
     *
     * Use an array of <CODE>LDAPModification</CODE> objects to specify the
     * changes that need to be made.  Each change must be specified by
     * an <CODE>LDAPModification</CODE> object, and each attribute value
     * to be modified, added, or removed must be specified by an
     * <CODE>LDAPAttribute</CODE> object. <P>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mods An array of objects representing the changes to be made
     * to the entry
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModification
     */
    public void modify (String DN, LDAPModification[] mods)
        throws LDAPException {
        modify(DN, mods, m_defaultConstraints);
    }

    /**
     * Makes a set of changes to an existing entry in the directory and
     * allows you to specify preferences for this LDAP modify operation
     * by using an <CODE>LDAPConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN The distinguished name of the entry that you want to modify
     * @param mods An array of objects representing the changes to be made
     * to the entry
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModification
     * @see netscape.ldap.LDAPConstraints
     */
    public void modify (String DN, LDAPModification[] mods,
         LDAPConstraints cons) throws LDAPException {
         internalBind (cons);

         LDAPResponseListener myListener = getResponseListener ();
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
     * @param DN Distinguished name identifying the entry that you want
     * to remove from the directory.
     * @exception LDAPException Failed to delete the specified entry from
     * the directory.
     */
    public void delete( String DN ) throws LDAPException {
        delete(DN, m_defaultConstraints);
    }

    /**
     * Deletes the entry for the specified DN from the directory and
     * allows you to specify preferences for this LDAP delete operation
     * by using an <CODE>LDAPConstraints</CODE> object. For
     * example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN Distinguished name identifying the entry that you want
     * to remove from the directory.
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to delete the specified entry from
     * the directory.
     * @see netscape.ldap.LDAPConstraints
     */
    public void delete( String DN, LDAPConstraints cons )
        throws LDAPException {
        internalBind (cons);

        LDAPResponseListener myListener = getResponseListener ();
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
     * @param DN Current distinguished name of the entry.
     * @param newRDN New relative distinguished name for the entry (for example,
     * "cn=newName").
     * @param deleteOldRDN If <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @exception LDAPException Failed to rename the specified entry.
     */
    public void rename (String DN, String newRDN, boolean deleteOldRDN )
        throws LDAPException {
        rename(DN, newRDN, null, deleteOldRDN);
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
     * @param DN Current distinguished name of the entry.
     * @param newRDN New relative distinguished name for the entry (for example,
     * "cn=newName").
     * @param deleteOldRDN If <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to rename the specified entry.
     */
    public void rename (String DN, String newRDN, boolean deleteOldRDN,
                        LDAPConstraints cons )
        throws LDAPException {
        rename(DN, newRDN, null, deleteOldRDN, cons);
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
     * @param DN Current distinguished name of the entry.
     * @param newRDN New relative distinguished name for the entry (for example,
     * "cn=newName").
     * @param newParentDN If not null, the distinguished name for the
     * entry under which the entry should be moved (for example, to move
     * an entry under the Accounting subtree, specify this argument as
     * "ou=Accounting, o=Ace Industry, c=US").
     * @param deleteOldRDN If <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @exception LDAPException Failed to rename the specified entry.
     */
     public void rename(String dn,
                        String newRDN,
                        String newParentDN,
                        boolean deleteOldRDN) throws LDAPException {
          rename(dn, newRDN, newParentDN, deleteOldRDN, m_defaultConstraints);
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
     * @param DN Current distinguished name of the entry.
     * @param newRDN New relative distinguished name for the entry (for example,
     * "cn=newName").
     * @param newParentDN If not null, the distinguished name for the
     * entry under which the entry should be moved (for example, to move
     * an entry under the Accounting subtree, specify this argument as
     * "ou=Accounting, o=Ace Industry, c=US").
     * @param deleteOldRDN If <CODE>true</CODE>, the old name is not retained
     * as an attribute value (for example, the attribute value "cn=oldName" is
     * removed).  If <CODE>false</CODE>, the old name is retained
     * as an attribute value (for example, the entry might now have two values
     * for the cn attribute: "cn=oldName" and "cn=newName").
     * @param cons The set of preferences that you want applied to this operation.
     * @exception LDAPException Failed to rename the specified entry.
     * @see netscape.ldap.LDAPConstraints
     */
    public void rename (String DN,
                           String newRDN,
                           String newParentDN,
                           boolean deleteOldRDN,
                           LDAPConstraints cons)
        throws LDAPException {
        internalBind (cons);

        LDAPResponseListener myListener = getResponseListener ();
        try {
            JDAPModifyRDNRequest request = null;
            if ( newParentDN != null ) {
                request = new JDAPModifyRDNRequest (DN,
                                                newRDN,
                                                deleteOldRDN,
                                                newParentDN);
            } else {
                request = new JDAPModifyRDNRequest (DN,
                                                newRDN,
                                                deleteOldRDN);
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
     * Adds an entry to the directory.
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPEntry
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener add(LDAPEntry entry,
                                    LDAPResponseListener listener)
                                    throws LDAPException{
        return add(entry, listener, m_defaultConstraints);
    }
 
    /**
     * Adds an entry to the directory and allows you to specify constraints
     * for this LDAP add operation by using an <CODE>LDAPConstraints</CODE>
     * object. For example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPEntry
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener add(LDAPEntry entry,
                                    LDAPResponseListener listener,
                                    LDAPConstraints cons)
                                    throws LDAPException{

        if (cons == null) {
            cons = m_defaultConstraints;
        }
        
        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }

        LDAPAttributeSet attrs = entry.getAttributeSet ();
        LDAPAttribute[] attrList = new LDAPAttribute[attrs.size()];
        for( int i = 0; i < attrs.size(); i++ )
            attrList[i] = (LDAPAttribute)attrs.elementAt( i );
        int attrPosition = 0;

        sendRequest (new JDAPAddRequest (entry.getDN(), attrList),
                    listener, cons);

        return listener;
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and password. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param dn If non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param passwd If non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name and passwd as password.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener bind(String dn,
                                     String passwd,
                                     LDAPResponseListener listener)
                                     throws LDAPException{
        return bind(dn, passwd, listener, m_defaultConstraints);
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and password  and allows you
     * to specify constraints for this LDAP add operation by using an
     *  <CODE>LDAPConstraints</CODE> object. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param dn If non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param passwd If non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name and passwd as password.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener bind(String dn,
                                     String passwd,
                                     LDAPResponseListener listener,
                                     LDAPConstraints cons) 
                                     throws LDAPException{
        if (cons == null) {
            cons = m_defaultConstraints;
        }

        m_prevBoundDN = m_boundDN;
        m_prevBoundPasswd = m_boundPasswd;
        m_boundDN = dn;
        m_boundPasswd = passwd;
        
        if (m_thread == null) {
            connect();
        }
        else if (m_thread.getClientCount() > 1) {
            disconnect();
            connect();
        }

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }

        sendRequest(new JDAPBindRequest(m_protocolVersion, m_boundDN, m_boundPasswd),
            listener, cons);

        return listener;
    }
    
    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     * 
     * @param dn If non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param mechanisms A list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param packageName  A package from which to instantiate the Mechanism
     * Driver, e.g. "myclasses.SASL.mechanisms". If null, a system default is used.
     * @param props The possibly null additional configuration properties for
     * the session.
     * @param cbh A class which may be called by the SASL framework to
     * obtain additional information required.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener bind(String dn,
                                     String[] mechanisms,
                                     String packageName,
                                     Properties props,
                                     CallbackHandler cbh,
                                     LDAPResponseListener listener)
                                     throws LDAPException{

        return bind(dn, mechanisms, packageName, props, cbh,
                    listener, m_defaultConstraints);
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and a specified SASL mechanism
     * or set of mechanisms. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     * 
     * @param dn If non-null and non-empty, specifies that the connection
     * and all operations through it should be authenticated with dn as the
     * distinguished name.
     * @param mechanisms A list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param packageName  A package from which to instantiate the Mechanism
     * Driver, e.g. "myclasses.SASL.mechanisms". If null, a system default is used.
     * @param props The possibly null additional configuration properties for
     * the session.
     * @param cbh A class which may be called by the SASL framework to
     * obtain additional information required.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to bind to the directory.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener bind(String dn,
                                     String[] mechanisms,
                                     String packageName,
                                     Properties props,
                                     CallbackHandler cbh,
                                     LDAPResponseListener listener,
                                     LDAPConstraints cons)
                                     throws LDAPException{
        return null;
    }

    /**
     * Deletes the entry for the specified DN from the directory.
     * 
     * @param dn Distinguished name of the entry to delete.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener delete(String dn,
                                       LDAPResponseListener listener)
                                       throws LDAPException{
        
        return delete(dn, listener, m_defaultConstraints);
    }

    /**
     * Deletes the entry for the specified DN from the directory.
     * 
     * @param dn Distinguished name of the entry to delete.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener delete(String dn,
                                      LDAPResponseListener listener,
                                      LDAPConstraints cons)
                                       throws LDAPException{
        if (cons == null) {
            cons = m_defaultConstraints;
        }

        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }
        
        sendRequest (new JDAPDeleteRequest(dn), listener, cons);
        
        return listener;

    }
    
    /**
     * Makes a single change to an existing entry in the directory (for
     * example, changes the value of an attribute, adds a new attribute
     * value, or removes an existing attribute value).<BR>
     * The LDAPModification object specifies both the change to be made and
     * the LDAPAttribute value to be changed.
     * 
     * @param dn Distinguished name of the entry to modify.
     * @param mod A single change to be made to an entry.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPModification
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener modify(String dn,
                                       LDAPModification mod,
                                       LDAPResponseListener listener)
                                       throws LDAPException{

        return modify(dn, mod, listener, m_defaultConstraints);
    }
    
    /**
     * Makes a single change to an existing entry in the directory (for
     * example, changes the value of an attribute, adds a new attribute
     * value, or removes an existing attribute value).<BR>
     * The LDAPModification object specifies both the change to be made and
     * the LDAPAttribute value to be changed.
     * 
     * @param dn Distinguished name of the entry to modify.
     * @param mod A single change to be made to an entry.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPModification
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener modify(String dn,
                                       LDAPModification mod,
                                       LDAPResponseListener listener,
                                       LDAPConstraints cons)
                                       throws LDAPException{
        if (cons == null) {
            cons = m_defaultConstraints;
        }

        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }

        LDAPModification[] modList = { mod };
        sendRequest (new JDAPModifyRequest (dn, modList), listener, cons);        

        return listener;
    }

    /**
     * Makes a set of changes to an existing entry in the directory (for
     * example, changes attribute values, adds new attribute values, or
     * removes existing attribute values).
     * <P>
     * @param dn Distinguished name of the entry to modify.
     * @param mods A set of changes to be made to the entry.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPModificationSet
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener modify(String dn,
                                       LDAPModificationSet mods,
                                       LDAPResponseListener listener)
                                       throws LDAPException{
        return modify(dn,mods, listener, m_defaultConstraints);
    }
    
    /**
     * Makes a set of changes to an existing entry in the directory (for
     * example, changes attribute values, adds new attribute values, or
     * removes existing attribute values).
     * 
     * @param dn Distinguished name of the entry to modify.
     * @param mods A set of changes to be made to the entry.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPModificationSet
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener modify(String dn,
                                       LDAPModificationSet mods,
                                       LDAPResponseListener listener,
                                       LDAPConstraints cons)
                                       throws LDAPException{
        if (cons == null) {
            cons = m_defaultConstraints;
        }

        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }

        LDAPModification[] modList = new LDAPModification[mods.size()];
        for( int i = 0; i < mods.size(); i++ ) {
            modList[i] = mods.elementAt( i );
        }

        sendRequest (new JDAPModifyRequest (dn, modList), listener, cons);        
        return listener;

    }    

    /**
     * Renames an existing entry in the directory.
     * 
     * @param dn Current distinguished name of the entry.
     * @param newRdn New relative distinguished name for the entry.
     * @param deleteOldRdn   If true, the old name is not retained as an
     * attribute value.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener rename(String dn,
                                       String newRdn,
                                       boolean deleteOldRdn,
                                       LDAPResponseListener listener)
                                       throws LDAPException{
        return rename(dn, newRdn, deleteOldRdn, listener, m_defaultConstraints);
    }

    /**
     * Renames an existing entry in the directory.
     * 
     * @param dn Current distinguished name of the entry.
     * @param newRdn New relative distinguished name for the entry.
     * @param deleteOldRdn   If true, the old name is not retained as an
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener rename(String dn,
                                       String newRdn,
                                       boolean deleteOldRdn,
                                       LDAPResponseListener listener,
                                       LDAPConstraints cons)
                                       throws LDAPException{
        if (cons == null) {
            cons = m_defaultConstraints;
        }
        
        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }

        sendRequest (new JDAPModifyRDNRequest (dn, newRdn, deleteOldRdn),
                     listener, cons);
        
        return listener;

    }
        
    
    /**
     * Performs the search specified by the criteria that you enter. <P>
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base The base distinguished name to search from
     * @param scope The scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter Search filter specifying the search criteria.
     * @param attrs List of attributes that you want returned in the
     * search results.
     * @param typesOnly If true, returns the names but not the values of the
     * attributes found.  If false, returns the names and values for
     * attributes found
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPSearchListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchListener)
     */
    public LDAPSearchListener search(String base,
                                     int scope,
                                     String filter,
                                     String attrs[],
                                     boolean typesOnly,
                                     LDAPSearchListener listener)
                                     throws LDAPException {
        
        return search(base, scope, filter, attrs, typesOnly,
                      listener, m_defaultConstraints);
    }

    /**
     * Performs the search specified by the criteria that you enter.
     * This method also allows you to specify constraints for the search
     * (such as the maximum number of entries to find or the
     * maximum time to wait for search results). <P>
     * To abandon the search, use the <CODE>abandon</CODE> method.
     *
     * @param base The base distinguished name to search from
     * @param scope The scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
     * (search the base DN and all entries within its subtree) <P>
     * </UL>
     * <P>
     * @param filter Search filter specifying the search criteria.
     * @param attrs List of attributes that you want returned in the search
     * results.
     * @param typesOnly If true, returns the names but not the values of the
     * attributes found.  If false, returns the names and  values for
     * attributes found.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to this search (for example, the
     * maximum number of entries to return).
     * @return LDAPSearchListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchListener)
     */
    public LDAPSearchListener search(String base,
                                     int scope,
                                     String filter,
                                     String attrs[],
                                     boolean typesOnly,
                                     LDAPSearchListener listener,
                                     LDAPSearchConstraints cons)
                                     throws LDAPException {
        if (cons == null) {
            cons = m_defaultConstraints;
        }

        internalBind (cons);
        
        if (listener == null) {
            listener = new LDAPSearchListener(/*asynchOp=*/true, cons);
        }
        
        JDAPSearchRequest request = null;        
        try {
            request = new JDAPSearchRequest (base, scope, cons.getDereference(),
                cons.getMaxResults(), cons.getServerTimeLimit(),
                typesOnly, filter, attrs);
        }
        catch (IllegalArgumentException e) {
            throw new LDAPException(e.getMessage(), LDAPException.PARAM_ERROR);
        }

        sendRequest (request, listener, cons);
        return listener;
        
    }
    
    /**
     * Compare an attribute value with one in the directory. The result can 
     * be obtained by calling <CODE>getResultCode</CODE> on the 
     * <CODE>LDAPResponse</CODE> from the <CODE>LDAPResponseListener</CODE>.
     * The code will be <CODE>LDAPException.COMPARE_TRUE</CODE> or 
     * <CODE>LDAPException.COMPARE_FALSE</CODE>. 
     * 
     * @param dn Distinguished name of the entry to compare.
     * @param attr Attribute with a value to compare.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     */
    public LDAPResponseListener compare(String dn, 
                                        LDAPAttribute attr, 
                                        LDAPResponseListener listener)
                                        throws LDAPException {

        return compare(dn, attr, listener, m_defaultConstraints);
    }
    
    /**
     * Compare an attribute value with one in the directory. The result can 
     * be obtained by calling <CODE>getResultCode</CODE> on the 
     * <CODE>LDAPResponse</CODE> from the <CODE>LDAPResponseListener</CODE>.
     * The code will be <CODE>LDAPException.COMPARE_TRUE</CODE> or 
     * <CODE>LDAPException.COMPARE_FALSE</CODE>. 
     * 
     * @param dn Distinguished name of the entry to compare.
     * @param attr Attribute with a value to compare.
     * @param listener Handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to this operation.
     * @return LDAPResponseListener Handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     */
    public LDAPResponseListener compare(String dn, 
                                        LDAPAttribute attr, 
                                        LDAPResponseListener listener,
                                        LDAPConstraints cons) 
                                        throws LDAPException {
        if (cons == null) {
            cons = m_defaultConstraints;
        }
        
        internalBind (cons);

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }

        Enumeration en = attr.getStringValues();
        String val = (String)en.nextElement();
        JDAPAVA ava = new JDAPAVA(attr.getName(), val);
        
        sendRequest (new JDAPCompareRequest (dn, ava), listener, cons);
        return listener;
    }
    
    /**
     * Cancels the ldap request with the specified id and discards
     * any results already received.
     * 
     * @param id A LDAP request id
     * @exception LDAPException Failed to send request.
     */
    public void abandon(int id) throws LDAPException {

        if (!isConnected()) {
            return;
        }
        
        for (int i=0; i<3; i++) {
            try {
                /* Tell listener thread to discard results */
                m_thread.abandon( id );

                /* Tell server to stop sending results */
                m_thread.sendRequest(null, new JDAPAbandonRequest(id), null, m_defaultConstraints);

                /* No response is forthcoming */
                break;
            } catch (NullPointerException ne) {
                // do nothing
            }
        }
        if (m_thread == null) {
            throw new LDAPException("Failed to send abandon request to the server.",
              LDAPException.OTHER);
        }
    }
    
    /**
     * Cancels all outstanding search requests associated with this
     * LDAPSearchListener object and discards any results already received.
     * 
     * @param searchlistener A search listener returned from a search. 
     * @exception LDAPException Failed to send request.
     */
    public void abandon(LDAPSearchListener searchlistener)throws LDAPException {
        int[] ids = searchlistener.getIDs();
        for (int i=0; i < ids.length; i++) {
            searchlistener.removeRequest(ids[i]);
            abandon(ids[i]);
        }
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
     * int sizeLimit = ( (Integer)ld.getOption( LDAPv2.SIZELIMIT ) ).intValue();
     * System.out.println( "Maximum number of results: " + sizeLimit );
     * </PRE>
     *
     * @param option You can specify one of the following options:
     * <TABLE CELLPADDING=5>
     * <TR VALIGN=BASELINE ALIGN=LEFT>
     * <TH>Option</TH><TH>Data Type</TH><TH>Description</TH></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.PROTOCOL_VERSION</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the version of the LDAP protocol used by the
     * client.
     * <P>By default, the value of this option is 2.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.DEREF</CODE></TD>
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
     * <CODE>LDAPv2.SIZELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to return.
     * If this option is set to 0, there is no maximum limit.
     * <P>By default, the value of this option is 1000.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.TIMELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of milliseconds to wait for results
     * before timing out. If this option is set to 0, there is no maximum
     * time limit.
     * <P>By default, the value of this option is 0.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS</CODE></TD>
     * <TD><CODE>Boolean</CODE></TD>
     * <TD>Specifies whether or not your client follows referrals automatically.
     * If <CODE>true</CODE>, your client follows referrals automatically.
     * If <CODE>false</CODE>, an <CODE>LDAPReferralException</CODE> is raised
     * when referral is detected.
     * <P>By default, the value of this option is <CODE>false</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS_REBIND_PROC</CODE></TD>
     * <TD><CODE>LDAPRebind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPRebind</CODE> interface.  You must define this class and
     * the <CODE>getRebindAuthentication</CODE> method that will be used to
     * get the distinguished name and password to use for authentication.
     * Modifying this option sets the <CODE>LDAPv2.BIND</CODE> option to null.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.BIND>/CODE></TD>
     * <TD><CODE>LDAPBind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPBind</CODE>
     * interface.  You must define this class and the
     * <CODE>bind</CODE> method that will be used to autheniticate
     * to the server on referrals. Modifying this option sets the 
     * <CODE>LDAPv2.REFERRALS_REBIND_PROC</CODE> to null.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS_HOP_LIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of referrals in a sequence that
     * your client will follow.  (For example, if REFERRALS_HOP_LIMIT is 5,
     * your client will follow no more than 5 referrals in a row when resolving
     * a single LDAP request.)
     * <P>By default, the value of this option is 10.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.BATCHSIZE</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the number of search results to return at a time.
     * (For example, if BATCHSIZE is 1, results are returned one at a time.)
     * <P>By default, the value of this option is 1.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv3.CLIENTCONTROLS</CODE></TD>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the client controls that may affect the handling of LDAP
     * operations in the LDAP classes. These controls are used by the client
     * and are not passed to the LDAP server. At this time, no client controls
     * are defined for clients built with the Netscape LDAP classes. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv3.SERVERCONTROLS</CODE></TD>
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
     * <P>By default, the value of this option is 100.</TD></TR>
     * </TABLE><P>
     * @return The value for the option wrapped in an object.  (You
     * need to cast the returned value as its appropriate type. For
     * example, when getting the SIZELIMIT option, cast the returned
     * value as an <CODE>Integer</CODE>.)
     * @exception LDAPException Failed to get the specified option.
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPConstraints
     * @see netscape.ldap.LDAPSearchConstraints
     * @see netscape.ldap.LDAPReferralException
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getSearchConstraints
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     */
    public Object getOption( int option ) throws LDAPException {
        if (option == LDAPv2.PROTOCOL_VERSION) {
            return new Integer(m_protocolVersion);
        }

        return getOption(option, m_defaultConstraints);
    }

    private static Object getOption( int option, LDAPSearchConstraints cons )
        throws LDAPException {
        switch (option) {
            case LDAPv2.DEREF:
              return new Integer (cons.getDereference());
            case LDAPv2.SIZELIMIT:
              return new Integer (cons.getMaxResults());
            case LDAPv2.TIMELIMIT:
              return new Integer (cons.getServerTimeLimit());
            case LDAPv2.REFERRALS:
              return new Boolean (cons.getReferrals());
            case LDAPv2.REFERRALS_REBIND_PROC:
              return cons.getRebindProc();
            case LDAPv2.BIND:
              return cons.getBindProc();
            case LDAPv2.REFERRALS_HOP_LIMIT:
              return new Integer (cons.getHopLimit());
            case LDAPv2.BATCHSIZE:
              return new Integer (cons.getBatchSize());
            case LDAPv3.CLIENTCONTROLS:
              return cons.getClientControls();
            case LDAPv3.SERVERCONTROLS:
              return cons.getServerControls();
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
     * ld.setOption( LDAPv2.SIZELIMIT, newLimit );
     * System.out.println( "Changed the maximum number of results to " + newLimit.intValue() );
     * </PRE>
     *
     * @param option You can specify one of the following options:
     * <TABLE CELLPADDING=5>
     * <TR VALIGN=BASELINE ALIGN=LEFT>
     * <TH>Option</TH><TH>Data Type</TH><TH>Description</TH></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.PROTOCOL_VERSION</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the version of the LDAP protocol used by the
     * client.
     * <P>By default, the value of this option is 2.  If you want
     * to use LDAP v3 features (such as extended operations or
     * controls), you need to set this value to 3. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.DEREF</CODE></TD>
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
     * <CODE>LDAPv2.SIZELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of search results to return.
     * If this option is set to 0, there is no maximum limit.
     * <P>By default, the value of this option is 1000.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.TIMELIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of milliseconds to wait for results
     * before timing out. If this option is set to 0, there is no maximum
     * time limit.
     * <P>By default, the value of this option is 0.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS</CODE></TD>
     * <TD><CODE>Boolean</CODE></TD>
     * <TD>Specifies whether or not your client follows referrals automatically.
     * If <CODE>true</CODE>, your client follows referrals automatically.
     * If <CODE>false</CODE>, an <CODE>LDAPReferralException</CODE> is
     * raised when a referral is detected.
     * <P>By default, the value of this option is <CODE>false</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS_REBIND_PROC</CODE></TD>
     * <TD><CODE>LDAPRebind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPRebind</CODE>
     * interface.  You must define this class and the
     * <CODE>getRebindAuthentication</CODE> method that will be used to get
     * the distinguished name and password to use for authentication. 
     * Modifying this option sets the <CODE>LDAPv2.BIND</CODE> option to null.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.BIND>/CODE></TD>
     * <TD><CODE>LDAPBind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPBind</CODE>
     * interface.  You must define this class and the
     * <CODE>bind</CODE> method that will be used to autheniticate
     * to the server on referrals. Modifying this option sets the 
     * <CODE>LDAPv2.REFERRALS_REBIND_PROC</CODE> to null.
     * <P>By default, the value of this option is <CODE>null</CODE>.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.REFERRALS_HOP_LIMIT</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the maximum number of referrals in a sequence that
     * your client will follow.  (For example, if REFERRALS_HOP_LIMIT is 5,
     * your client will follow no more than 5 referrals in a row when resolving
     * a single LDAP request.)
     * <P>By default, the value of this option is 10.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv2.BATCHSIZE</CODE></TD>
     * <TD><CODE>Integer</CODE></TD>
     * <TD>Specifies the number of search results to return at a time.
     * (For example, if BATCHSIZE is 1, results are returned one at a time.)
     * <P>By default, the value of this option is 1.</TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv3.CLIENTCONTROLS</CODE></TD>
     * <TD><CODE>LDAPControl[]</CODE></TD>
     * <TD>Specifies the client controls that may affect handling of LDAP
     * operations in the LDAP classes. These controls are used by the client
     * and are not passed to the server. At this time, no client controls
     * are defined for clients built with the Netscape LDAP classes. </TD></TR>
     * <TR VALIGN=BASELINE><TD>
     * <CODE>LDAPv3.SERVERCONTROLS</CODE></TD>
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
     * <P>By default, the value of this option is 100.</TD></TR>
     * </TABLE><P>
     * @param value The value to assign to the option.  The value must be
     * the java.lang object wrapper for the appropriate parameter
     * (e.g. boolean->Boolean,
     *   integer->Integer)
     * @exception LDAPException Failed to set the specified option.
     * @see netscape.ldap.LDAPRebind
     * @see netscape.ldap.LDAPConstraints
     * @see netscape.ldap.LDAPSearchConstraints
     * @see netscape.ldap.LDAPReferralException
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPConnection#getSearchConstraints
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     */
    public void setOption( int option, Object value ) throws LDAPException {
        if (option == LDAPv2.PROTOCOL_VERSION) {
            setProtocolVersion(((Integer)value).intValue());
            return;
        }
        setOption(option, value, m_defaultConstraints);
    }

    private static void setOption( int option, Object value, LDAPSearchConstraints cons ) throws LDAPException {
      try {
        switch (option) {
        case LDAPv2.DEREF:
          cons.setDereference(((Integer)value).intValue());
          return;
        case LDAPv2.SIZELIMIT:
          cons.setMaxResults(((Integer)value).intValue());
          return;
        case LDAPv2.TIMELIMIT:
          cons.setTimeLimit(((Integer)value).intValue());
          return;
        case LDAPv2.SERVER_TIMELIMIT:
          cons.setServerTimeLimit(((Integer)value).intValue());
          return;
        case LDAPv2.REFERRALS:
          cons.setReferrals(((Boolean)value).booleanValue());
          return;
        case LDAPv2.BIND:
          cons.setBindProc((LDAPBind)value);
          return;
        case LDAPv2.REFERRALS_REBIND_PROC:
          cons.setRebindProc((LDAPRebind)value);
          return;
        case LDAPv2.REFERRALS_HOP_LIMIT:
          cons.setHopLimit(((Integer)value).intValue());
          return;
        case LDAPv2.BATCHSIZE:
          cons.setBatchSize(((Integer)value).intValue());
          return;
        case LDAPv3.CLIENTCONTROLS:
          if ( value == null )
            cons.setClientControls( (LDAPControl[]) null );
          else if ( value instanceof LDAPControl )
            cons.setClientControls( (LDAPControl) value );
          else if ( value instanceof LDAPControl[] )
            cons.setClientControls( (LDAPControl[])value );
          else
            throw new LDAPException ( "invalid LDAPControl",
                                      LDAPException.PARAM_ERROR );
          return;
        case LDAPv3.SERVERCONTROLS:
          if ( value == null )
            cons.setServerControls( (LDAPControl[]) null );
          else if ( value instanceof LDAPControl )
            cons.setServerControls( (LDAPControl) value );
          else if ( value instanceof LDAPControl[] )
            cons.setServerControls( (LDAPControl[])value );
          else
            throw new LDAPException ( "invalid LDAPControl",
                                      LDAPException.PARAM_ERROR );
          return;
        case MAXBACKLOG:
            int val = ((Integer)value).intValue();
            if ( val < 1 ) {
                throw new LDAPException ( "MAXBACKLOG must be at least 1",
                                          LDAPException.PARAM_ERROR );
            } else {
                cons.setMaxBacklog(((Integer)value).intValue());
            }
            return;
        default:
          throw new LDAPException ("invalid option",
                                   LDAPException.PARAM_ERROR );
        }
      } catch (ClassCastException cc) {
          throw new LDAPException ("invalid option value",
                                   LDAPException.PARAM_ERROR );
      }
    }

    /**
     * Returns an array of the latest controls (if any) from server.
     * <P>
     * To retrieve the controls from a search result, call the 
     * <CODE>getResponseControls</CODE> method from the <CODE>LDAPSearchResults
     * </CODE> object returned with the result.
     * @return An array of the controls returned by an operation, or
     * null if none.
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPSearchResults#getResponseControls
     */
    public LDAPControl[] getResponseControls() {
      LDAPControl[] controls = null;

      /* Get the latest controls returned for our thread */
      synchronized(m_responseControlTable) {
          Vector responses = (Vector)m_responseControlTable.get(m_thread);

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
     * if you want to create a slightly different set of constraints
     * that you want to apply to a particular operation.
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
     * @return A copy of the <CODE>LDAPConstraints</CODE> object representing the
     * set of constraints that apply (by default) to all operations
     * performed through this connection.
     * @see netscape.ldap.LDAPConstraints
     * @see netscape.ldap.LDAPConnection#getOption
     */
    public LDAPConstraints getConstraints () {
        return (LDAPConstraints)getSearchConstraints();
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
     * if you want to create a slightly different set of search constraints
     * that you want to apply to a particular search.
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
     *                                          LDAPv2.SCOPE_SUB,
     *                                          "(objectclass=*)",
     *                                          myAttrs,
     *                                          false,
     *                                          myOptions );
     * ...
     * </PRE>
     *
     * @return A copy of the <CODE>LDAPSearchConstraints</CODE> object 
     * representing the set of search constraints that apply (by default) to 
     * all searches performed through this connection.
     * @see netscape.ldap.LDAPSearchConstraints
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)  
     */
    public LDAPSearchConstraints getSearchConstraints () {
        return (LDAPSearchConstraints)m_defaultConstraints.clone();
    }


    /**
     * Set the default constraint set for all operations. 
     * @param cons <CODE>LDAPConstraints</CODE> object to be used as the default
     * constraint set.
     * @see netscape.ldap.LDAPConnection#getConstraints
     * @see netscape.ldap.LDAPConstraints
     */
    public void setConstraints(LDAPConstraints cons) {
        m_defaultConstraints.setHopLimit(cons.getHopLimit());
        m_defaultConstraints.setReferrals(cons.getReferrals());
        m_defaultConstraints.setTimeLimit(cons.getTimeLimit());
        m_defaultConstraints.setBindProc(cons.getBindProc());
        m_defaultConstraints.setRebindProc(cons.getRebindProc());

        LDAPControl[] tClientControls = cons.getClientControls();
        LDAPControl[] oClientControls = null;

        if ( (tClientControls != null) &&
             (tClientControls.length > 0) ) {
            oClientControls = new LDAPControl[tClientControls.length]; 
            for( int i = 0; i < tClientControls.length; i++ ) {
                oClientControls[i] = (LDAPControl)tClientControls[i].clone();
            }
        } 
        m_defaultConstraints.setClientControls(oClientControls);
 
        LDAPControl[] tServerControls = cons.getServerControls();
        LDAPControl[] oServerControls = null;

        if ( (tServerControls != null) && 
             (tServerControls.length > 0) ) {
            oServerControls = new LDAPControl[tServerControls.length];
            for( int i = 0; i < tServerControls.length; i++ ) {
                oServerControls[i] = (LDAPControl)tServerControls[i].clone();
            }
        }
        m_defaultConstraints.setServerControls(oServerControls);
    }
    
    /**
     * Set the default constraint set for all search operations. 
     * @param cons <CODE>LDAPSearchConstraints</CODE> object to be used as the
     * default constraint set.
     * @see netscape.ldap.LDAPConnection#getSearchConstraints
     * @see netscape.ldap.LDAPSearchConstraints
     */
    public void setSearchConstraints(LDAPSearchConstraints cons) {
        m_defaultConstraints = (LDAPSearchConstraints)cons.clone();
    }

    /**
     * Gets the stream for reading from the listener socket
     *
     * @return The stream for reading from the listener socket, or
     * <CODE>null</CODE> if there is none
     */
    public InputStream getInputStream() {
        return (m_thread != null) ? m_thread.getInputStream() : null;
    }

    /**
     * Sets the stream for reading from the listener socket if
     * there is one
     *
     * @param is The stream for reading from the listener socket
     */
    public void setInputStream( InputStream is ) {
        if ( m_thread != null ) {
            m_thread.setInputStream( is );
        }
    }

    /**
     * Gets the stream for writing to the socket
     *
     * @return The stream for writing to the socket, or
     * <CODE>null</CODE> if there is none
     */
    public OutputStream getOutputStream() {
        return (m_thread != null) ? m_thread.getOutputStream() : null;
    }

    /**
     * Sets the stream for writing to the socket
     *
     * @param os The stream for writing to the socket, if there is one
     */
    public void setOutputStream( OutputStream os ) {
        if ( m_thread != null ) {
            m_thread.setOutputStream( os );
        }
    }

    /**
     * Get a new listening agent from the internal buffer of available agents.
     * These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @return response listener object
     */
    synchronized LDAPResponseListener getResponseListener () {
        if (m_responseListeners == null) {
            m_responseListeners = new Vector (5);
        }

        LDAPResponseListener l;
        if ( m_responseListeners.size() < 1 ) {
            l = new LDAPResponseListener ( /*asynchOp=*/false );
        }
        else {
            l = (LDAPResponseListener)m_responseListeners.elementAt (0);
            m_responseListeners.removeElementAt (0);
        }
        return l;
    }

    /**
     * Get a new search listening agent from the internal buffer of available
     * agents. These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @return A search response listener object
     */
    private synchronized LDAPSearchListener getSearchListener (
        LDAPSearchConstraints cons ) {
        if (m_searchListeners == null) {
            m_searchListeners = new Vector (5);
        }

        LDAPSearchListener l;
        if ( m_searchListeners.size() < 1 ) {
            l = new LDAPSearchListener ( /*asynchOp=*/false, cons );
        }
        else {
            l = (LDAPSearchListener)m_searchListeners.elementAt (0);
            m_searchListeners.removeElementAt (0);
        }
        return l;
    }

    /**
     * Put a listening agent into the internal buffer of available agents.
     * These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @param l Listener to buffer
     */
    synchronized void releaseResponseListener (LDAPResponseListener l) {
        if (m_responseListeners == null) {
            m_responseListeners = new Vector (5);
        }

        l.reset ();
        m_responseListeners.addElement (l);
    }

    /**
     * Put a search listening agent into the internal buffer of available
     * agents. These objects are used to make the asynchronous LDAP
     * operations synchronous.
     * @param Listener to buffer
     */
    synchronized void releaseSearchListener (LDAPSearchListener l) {
        if (m_searchListeners == null) {
            m_searchListeners = new Vector (5);
        }

        l.reset ();
        m_searchListeners.addElement (l);
    }

    /**
     * Checks the message (assumed to be a return value).  If the resultCode
     * is anything other than SUCCESS, it throws an LDAPException describing
     * the server's (error) response.
     * @param m Server response to validate
     * @exception LDAPException failed to check message
     */
    void checkMsg (LDAPMessage m) throws LDAPException {
      if (m.getProtocolOp() instanceof JDAPResult) {
          JDAPResult response = (JDAPResult)(m.getProtocolOp());
          int resultCode = response.getResultCode ();

          if (resultCode == JDAPResult.SUCCESS) {
              return;
          }

          if (resultCode == JDAPResult.REFERRAL) {
              throw new LDAPReferralException ("referral", resultCode,
                                               response.getReferrals());
          }

          if (resultCode == JDAPResult.LDAP_PARTIAL_RESULTS) {
              throw new LDAPReferralException ("referral", resultCode,
                                               response.getErrorMessage());
          } else {
              throw new LDAPException ("error result", resultCode,
                response.getErrorMessage(),
                response.getMatchedDN());
          }

      } else if (m.getProtocolOp() instanceof JDAPSearchResultReference) {
          String[] referrals =
            ((JDAPSearchResultReference)m.getProtocolOp()).getUrls();
          throw new LDAPReferralException ("referral",
                                           JDAPResult.SUCCESS, referrals);
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
     * @param current The target thread.
     * @param con The server response controls.
     */
    void setResponseControls( LDAPConnThread current, LDAPResponseControl con ) {
        synchronized(m_responseControlTable) {
            Vector v = (Vector)m_responseControlTable.get(current);

            // if the current thread already contains response controls from
            // a previous operation
            if ((v != null) && (v.size() > 0)) {

                // look at each response control
                for (int i=v.size()-1; i>=0; i--) {
                    LDAPResponseControl response = 
                      (LDAPResponseControl)v.elementAt(i);
    
                    // if this response control belongs to this connection
                    if (response.getConnection().equals(this)) {
 
                        // if the given control is null or 
                        // the given control is not null and the current 
                        // control does not correspond to the new LDAPMessage
                        if ((con == null) || 
                            (con.getMsgID() != response.getMsgID())) {
                            v.removeElement(response);
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
                if (con != null) {
                    v = new Vector();
                }
            }          

            if (con != null) {
                v.addElement(con);
                m_responseControlTable.put(current, v);
            }

            /* Do some garbage collection: check if any attached threads have
               exited */
            /* Now check all threads in the list */
            Enumeration e = m_attachedList.elements();
            while( e.hasMoreElements() ) {
                LDAPConnThread aThread = (LDAPConnThread)e.nextElement();
                if ( !aThread.isAlive() ) {
                    m_responseControlTable.remove( aThread );
                    m_attachedList.removeElement( aThread );
                }
            }
        }
        /* Make sure we're registered */
        if ( m_attachedList.indexOf( current ) < 0 ) {
            m_attachedList.addElement( current );
        }
    }

    /**
     * Set up connection for referral.
     * @param u referral URL
     * @param cons search constraints
     * @return new LDAPConnection, already connected and authenticated
     */
    private LDAPConnection prepareReferral( LDAPUrl u,
                                            LDAPConstraints cons )
        throws LDAPException {
        LDAPConnection connection = new LDAPConnection (this.getSocketFactory());
        connection.setOption(REFERRALS, new Boolean(true));
        connection.setOption(REFERRALS_REBIND_PROC, cons.getRebindProc());
        connection.setOption(BIND, cons.getBindProc());
  
        // need to set the protocol version which gets passed to connection
        connection.setOption(PROTOCOL_VERSION,
                              new Integer(m_protocolVersion));

        connection.setOption(REFERRALS_HOP_LIMIT,
                              new Integer(cons.getHopLimit()-1));
        connection.connect (u.getHost(), u.getPort());
        return connection;
    }

    /**
     * Establish the LDAPConnection to the referred server. This one is used
     * for bind operation only since we need to keep this new connection for
     * the subsequent operations. This new connection will be destroyed in
     * two circumstances: disconnect gets called or the client binds as
     * someone else.
     * @return The new LDAPConnection to the referred server
     */
    LDAPConnection createReferralConnection(LDAPReferralException e,
      LDAPConstraints cons) throws LDAPException {
        if (cons.getHopLimit() <= 0) {
            throw new LDAPException("exceed hop limit",
                                    e.getLDAPResultCode(), e.getLDAPErrorMessage());
        }
        if (!cons.getReferrals()) {
            throw e;
        }

        LDAPUrl u[] = e.getURLs();
        // If there are no referrals (because the server isn't set up for
        // them), give up here
        if ((u == null) || (u.length <= 0) || (u[0].equals(""))) {
            throw new LDAPException("No target URL in referral",
                                    LDAPException.NO_RESULTS_RETURNED);
        }

        LDAPConnection connection = null;
        connection = prepareReferral(u[0], cons);
        String DN = u[0].getDN();
        if ((DN == null) || (DN.equals(""))) {
            DN = m_boundDN;
        }
        connection.authenticate(m_protocolVersion, DN, m_boundPasswd);
        return connection;
    }

    /**
     * Follows a referral.
     * @param e referral exception
     * @param cons search constraints
     */
    void performReferrals(LDAPReferralException e,
                          LDAPConstraints cons, int ops,
                          /* unions of different operation parameters */
                          String dn, int scope, String filter, String types[],
                          boolean attrsOnly, LDAPModification mods[],
                          LDAPEntry entry,
                          LDAPAttribute attr,
                          /* result */
                          Vector results
        ) throws LDAPException {

        if (cons.getHopLimit() <= 0) {
            throw new LDAPException("exceed hop limit",
                                    e.getLDAPResultCode(),
                                    e.getLDAPErrorMessage());
        }
        if (!cons.getReferrals()) {
            if (ops == JDAPProtocolOp.SEARCH_REQUEST) {
                LDAPSearchResults res = new LDAPSearchResults();
                res.add(e);
                results.addElement(res);
                return;
            } else {
                throw e;
            }
        }

        LDAPUrl u[] = e.getURLs();
        // If there are no referrals (because the server isn't configured to
        // return one), give up here
        if ( u == null ) {
            return;
        }

        for (int i = 0; i < u.length; i++) {
            String newDN = u[i].getDN();
            String DN = null;
            if ((newDN == null) || (newDN.equals(""))) {
                DN = dn;
            } else {
                DN = newDN;
            }
            // If this was a one-level search, and a direct subordinate
            // has a referral, there will be a "?base" in the URL, and
            // we have to rewrite the scope from one to base
            if ( u[i].getUrl().indexOf("?base") > -1 ) {
                scope = SCOPE_BASE;
            }

            LDAPSearchResults res = null;
            LDAPSearchConstraints newcons = (LDAPSearchConstraints)cons.clone();
            newcons.setHopLimit( cons.getHopLimit()-1 );

            try {
                if ((m_referralConnection != null) && 
                    (u[i].getHost().equals(m_referralConnection.getHost())) &&
                    (u[i].getPort() == m_referralConnection.getPort())) {
                    performReferrals(m_referralConnection, newcons, ops, DN, 
                                     scope, filter, types, attrsOnly, mods,
                                     entry, attr, results);
                    continue;
                }
            } catch (LDAPException le) {
                if (le.getLDAPResultCode() != 
                    LDAPException.INSUFFICIENT_ACCESS_RIGHTS) {
                    throw le;
                }
            }

            LDAPConnection connection = null;
            connection = prepareReferral( u[i], cons );
            if (cons.getRebindProc() == null && cons.getBindProc() == null) {
                connection.authenticate (null, null);
            } else if (cons.getBindProc() == null) {
                LDAPRebindAuth auth =
                  cons.getRebindProc().getRebindAuthentication(u[i].getHost(),
                                                               u[i].getPort());
                connection.authenticate(auth.getDN(), auth.getPassword());
            } else {
                cons.getBindProc().bind(connection);
            }
            performReferrals(connection, newcons, ops, DN, scope, filter,
                             types, attrsOnly, mods, entry, attr, results);
        }
    }

    void performReferrals(LDAPConnection connection, 
                          LDAPConstraints cons, int ops, String dn, int scope,
                          String filter, String types[], boolean attrsOnly,
                          LDAPModification mods[], LDAPEntry entry,
                          LDAPAttribute attr,
                          Vector results) throws LDAPException {
 
        LDAPSearchResults res = null;
        try {
            switch (ops) {
                case JDAPProtocolOp.SEARCH_REQUEST:

                    res = connection.search(dn, scope, filter,
                                            types, attrsOnly, 
                                            (LDAPSearchConstraints)cons);
                    if (res != null) {
                        res.closeOnCompletion(connection);
                        results.addElement(res);
                    } else {
                        if ((m_referralConnection == null) ||
                          (!connection.equals(m_referralConnection)))
                            connection.disconnect();
                    }
                    break;
                case JDAPProtocolOp.MODIFY_REQUEST:
                    connection.modify(dn, mods, cons);
                    break;
                case JDAPProtocolOp.ADD_REQUEST:
                    if ((dn != null) && (!dn.equals("")))
                         entry.setDN(dn);
                    connection.add(entry, cons);
                    break;
                case JDAPProtocolOp.DEL_REQUEST:
                    connection.delete(dn, cons);
                    break;
                case JDAPProtocolOp.MODIFY_RDN_REQUEST:
                    connection.rename(dn, filter /* newRDN */, 
                      attrsOnly /* deleteOld */, cons);
                    break;
                case JDAPProtocolOp.COMPARE_REQUEST:
                    boolean bool = connection.compare(dn, attr, cons);
                    results.addElement(new Boolean(bool));
                    break;
                default:
                    /* impossible */
                    break;
            }
        } catch (LDAPException ee) {
            throw ee;
        } finally {
            if ((connection != null) && 
                ((ops != JDAPProtocolOp.SEARCH_REQUEST) || (res == null)) &&
                ((m_referralConnection == null) || 
                 (!connection.equals(m_referralConnection)))) {
                connection.disconnect();
            }
        }
    }

    /**
     * Follows a referral for an extended operation.
     * @param e referral exception
     * @param cons search constraints
     */
    private LDAPExtendedOperation performExtendedReferrals(
        LDAPReferralException e,
        LDAPConstraints cons, LDAPExtendedOperation op )
        throws LDAPException {

        if (cons.getHopLimit() <= 0) {
            throw new LDAPException("exceed hop limit",
                                    e.getLDAPResultCode(),
                                    e.getLDAPErrorMessage());
        }
        if (!cons.getReferrals()) {
            throw e;
        }

        LDAPUrl u[] = e.getURLs();
        // If there are no referrals (because the server isn't configured to
        // return one), give up here
        if ( u == null ) {
            return null;
        }

        for (int i = 0; i < u.length; i++) {
            try {
                LDAPConnection connection = prepareReferral( u[i], cons );
                LDAPExtendedOperation results =
                    connection.extendedOperation( op );
                connection.disconnect();
                return results; /* return right away if operation is successful */
            } catch (LDAPException ee) {
                continue;
            }
        }
        return null;
    }

    /**
     * Creates and returns a new <CODE>LDAPConnection</CODE> object that
     * contains the same information as the current connection, including:
     * <UL>
     * <LI>the default search constraints
     * <LI>host name and port number of the LDAP server
     * <LI>the DN and password used to authenticate to the LDAP server
     * </UL>
     * <P>
     * @return The <CODE>LDAPconnection</CODE> object representing the
     * new object.
     */
    public synchronized Object clone() {
        try {
            LDAPConnection c = (LDAPConnection)super.clone();

            if (!isConnected()) {
                this.internalBind(m_defaultConstraints);
            }

            c.m_defaultConstraints =
                (LDAPSearchConstraints)m_defaultConstraints.clone();
            c.m_responseListeners = null;
            c.m_searchListeners = null;
            c.m_bound = this.m_bound;
            c.m_connMgr = m_connMgr;
            c.m_connSetupDelay = m_connSetupDelay;
            c.m_boundDN = this.m_boundDN;
            c.m_boundPasswd = this.m_boundPasswd;
            c.m_prevBoundDN = this.m_prevBoundDN;
            c.m_prevBoundPasswd = this.m_prevBoundPasswd;
            c.m_anonymousBound = this.m_anonymousBound;
            c.m_factory = this.m_factory;
            c.m_thread = this.m_thread; /* share current connection thread */

            Vector v = (Vector)m_threadConnTable.get(this.m_thread);
            if (v != null) {
                v.addElement(c);
            } else {
                printDebug("Failed to clone");
                return null;
            }

            c.m_thread.register(c);
            return c;
        } catch (Exception e) {
        }
        return null;
    }

    /**
     * This is called when a search result has been retrieved from the incoming
     * queue. We use the notification to unblock the listener thread, if it
     * is waiting for the backlog to lighten.
     */
    void resultRetrieved() {
        if ( m_thread != null ) {
            m_thread.resultRetrieved();
        }
    }

    /**
     * Sets up basic connection privileges for Communicator.
     * @return true if in Communicator and connections okay.
     */
    private static boolean checkCommunicator() {
        try {
            java.lang.reflect.Method m = LDAPCheckComm.getMethod(
              "netscape.security.PrivilegeManager", "enablePrivilege");
            if (m == null) {
                printDebug("Method is null");
                return false;
            }

            Object[] args = new Object[1];
            args[0] = new String("UniversalConnect");
            m.invoke( null, args);
            printDebug( "UniversalConnect enabled" );
            args[0] = new String("UniversalPropertyRead");
            m.invoke( null, args);
            printDebug( "UniversalPropertyRead enabled" );
            return true;
        } catch (LDAPException e) {
            printDebug("Exception: "+e.toString());
        } catch (Exception ie) {
            printDebug("Exception on invoking " + "enablePrivilege: "+ie.toString());
        }
        return false;
    }

    /**
     * Reports if the class is running in a Netscape browser.
     * @return <CODE>true</TRUE> if the class is running in a Netscape browser.
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
     * Prints out the LDAP Java SDK version and the highest LDAP
     * protocol version supported by the SDK. To view this
     * information, open a terminal window, and enter:
     * <PRE>java netscape.ldap.LDAPConnection
     * </PRE>
     * @param args Not currently used.
     */
    public static void main(String[] args) {
        System.out.println("LDAP SDK Version is "+SdkVersion);
        System.out.println("LDAP Protocol Version is "+ProtocolVersion);
    }

    /**
     * Option specifying the maximum number of unread entries to be cached in any
     * LDAPSearchResults without suspending reading from the server.
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#setOption
     */
    public static final int MAXBACKLOG   = 30;

    private static boolean isCommunicator = checkCommunicator();
    private static boolean debug = false;
}
