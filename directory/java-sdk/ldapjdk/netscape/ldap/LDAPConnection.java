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
       implements LDAPv3, LDAPAsynchronousConnection, Cloneable, Serializable {

    static final long serialVersionUID = -8698420087475771145L;

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
     * is of the type <CODE>Float</CODE>. For example:<P>
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
     * is of the type <CODE>Float</CODE>. For example:<P>
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
     * of the type <CODE>String</CODE>. For example:<P>
     * <PRE>
     *      ...
     *      String authTypes = ( String )myConn.getProperty( myConn.LDAP_PROPERTY_SECURITY );
     *      System.out.println( "Supported authentication types: " + authTypes );
     *      ... </PRE>
     * @see netscape.ldap.LDAPConnection#getProperty(java.lang.String)
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
     * When the property is set with the <CODE>getProperty</CODE> method,
     * the property value must be either a String (represents a file name)
     * an OutputStream or an instance of LDAPTraceWriter. To stop tracing,
     * <CODE>null</CODE> should be  passed as the property value.
     * 
     * @see netscape.ldap.LDAPConnection#setProperty(java.lang.String, java.lang.Object)
     */
    public final static String TRACE_PROPERTY = "com.netscape.ldap.trace";

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
    /**
     * Internal variables
     */
    private LDAPSearchConstraints m_defaultConstraints =
        new LDAPSearchConstraints();
    
    // A clone of constraints for the successful bind. Used by 
    // "smart failover" for the automatic rebind
    private LDAPConstraints m_rebindConstraints;

    private Vector m_responseListeners;
    private Vector m_searchListeners;

    private String m_boundDN;
    private String m_boundPasswd;
    private int m_protocolVersion = LDAP_VERSION;
    
    private LDAPConnSetupMgr m_connMgr;
    private int m_connSetupDelay = -1;
    private int m_connectTimeout = 0;
    private LDAPSocketFactory m_factory;

    // A flag if m_factory is used to start TLS
    private boolean m_isTLSFactory;
    
    /* m_thread does all socket i/o for the object and any clones */
    private transient LDAPConnThread m_thread = null;

    /* To manage received server controls on a per-thread basis */
    private Hashtable m_responseControlTable = new Hashtable();
    private LDAPCache m_cache = null;

    // A flag set if startTLS() called successfully
    private boolean m_useTLS;

    // OID for the extended operation startTLS
    final static String OID_startTLS = "1.3.6.1.4.1.1466.20037"; 

    private Object m_security = null;
    private LDAPSaslBind m_saslBinder = null;
    private Properties m_securityProperties;
    private Hashtable m_properties = new Hashtable();
    private LDAPConnection m_referralConnection;

    /**
     * Properties
     */
    private final static Float SdkVersion = new Float(4.17f);
    private final static Float ProtocolVersion = new Float(3.0f);
    private final static String SecurityVersion = new String("none,simple,sasl");
    private final static Float MajorVersion = new Float(4.0f);
    private final static Float MinorVersion = new Float(0.17f);
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
     *  @param cache the <CODE>LDAPCache</CODE> object representing
     *  the cache that the current connection should use
     *  @see netscape.ldap.LDAPCache
     *  @see netscape.ldap.LDAPConnection#getCache
     */
    public void setCache(LDAPCache cache) {
        if (m_cache != null) {
            m_cache.removeReference();
        }
        if (cache != null) {
            cache.addReference();
        }
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
     * @return the <CODE>LDAPCache</CODE> object representing
     * the cache that the current connection should use
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
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_SDK
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_PROTOCOL
     * @see netscape.ldap.LDAPConnection#LDAP_PROPERTY_SECURITY
     */
    public Object getProperty(String name) throws LDAPException {
        return m_properties.get( name );
    }

    /**
     * Change a property of a connection. <P>
     *
     * The following properties are defined:<BR> 
     * com.netscape.ldap.schema.quoting - "standard" or "NetscapeBug"<BR> 
     * Note: if this property is not set, the SDK will query the server 
     * to determine if attribute syntax values and objectclass superior 
     * values must be quoted when adding schema.<BR>
     * com.netscape.ldap.saslpackage - the default is "com.netscape.sasl"<BR> 
     * <P>
     *
     * @param name name of the property to set
     * @param val value to set
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

        } else if ( name.equalsIgnoreCase( TRACE_PROPERTY ) ) {

            Object traceOutput = null;
            if (val == null) {
                m_properties.remove(TRACE_PROPERTY);
            }                
            else {
                if (m_thread != null) {
                    traceOutput = createTraceOutput(val);
                }
                m_properties.put( TRACE_PROPERTY, val ); 
            }

            if (m_thread != null) {
                m_thread.setTraceOutput(traceOutput);
            }

        // This is used only by the ldapjdk test cases to simulate a
        // server problem and to test fail-over and rebind            
        } else if ( name.equalsIgnoreCase( "breakConnection" ) ) {
            m_connMgr.breakConnection();

        } else {
            throw new LDAPException("Unknown property: " + name);
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
    Object createTraceOutput(Object out) throws LDAPException {
                
        if (out instanceof String) { // trace file name
            OutputStream os = null;
            String file = (String)out;
            if (file.length() == 0) {
                os = System.err;
            }
            else {
                try {
                    boolean appendMode = (file.charAt(0) == '+');
                    if (appendMode) {
                        file = file.substring(1);
                    }                        
                    FileOutputStream fos = new FileOutputStream(file, appendMode);
                    os = new BufferedOutputStream(fos);
                }
                catch (IOException e) {
                    throw new LDAPException(
                        "Can not open output trace file " + file + " " + e);
                }
            }
            return os;
        }        
        else if (out instanceof OutputStream)  {
            return out;
        }       
        else if (out instanceof LDAPTraceWriter)  {
            return out;
        }       
        else {
            throw new LDAPException(TRACE_PROPERTY + " must be an OutputStream, a file name or an instance of LDAPTraceWriter" );
        }

    }
    
    /**
     * Sets the LDAP protocol version that your client prefers to use when
     * connecting to the LDAP server.
     * <P>
     *
     * @param version the LDAP protocol version that your client uses
     */
    private void setProtocolVersion(int version) {
        m_protocolVersion = version;
    }

    /**
     * Returns the host name of the LDAP server to which you are connected.
     * @return host name of the LDAP server.
     */
    public String getHost () {
        if (m_connMgr != null) {
            return m_connMgr.getHost();
        }
        return null;
    }

    /**
     * Returns the port number of the LDAP server to which you are connected.
     * @return port number of the LDAP server.
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
     * @return distinguished name used for authentication over this connection.
     */
    public String getAuthenticationDN () {
        return m_boundDN;
    }

    /**
     * Returns the password used for authentication over this connection.
     * @return password used for authentication over this connection.
     */
    public String getAuthenticationPassword () {
        return m_boundPasswd;
    }
    
    /**
     * Returns the maximum time to wait for the connection to be established.
     * @return the maximum connect time in seconds or 0 (unlimited)
     * @see netscape.ldap.LDAPConnection#setConnectTimeout
     */
    public int getConnectTimeout () {
        return m_connectTimeout;
    }

    /**
     * Specifies the maximum time to wait for the connection to be established.
     * If the value is 0, the time is not limited.
     * @param timeout the maximum connect time in seconds or 0 (unlimited)
     */
    public void setConnectTimeout (int timeout) {
        if (timeout < 0) {
            throw new IllegalArgumentException("Timeout value can not be negative");
        }
        m_connectTimeout = timeout;
        if (m_connMgr != null) {
            m_connMgr.setConnectTimeout(m_connectTimeout);
        }
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
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     */
    public int getConnSetupDelay () {
        return m_connSetupDelay;
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
     * @see netscape.ldap.LDAPConnection#NODELAY_SERIAL
     * @see netscape.ldap.LDAPConnection#NODELAY_PARALLEL
     * @see netscape.ldap.LDAPConnection#connect(java.lang.String, int)
     */
    public void setConnSetupDelay (int delay) {
        m_connSetupDelay = delay;
        if (m_connMgr != null) {
            m_connMgr.setConnSetupDelay(delay);
        }
    }

    /**
     * Gets the object representing the socket factory used to establish
     * a connection to the LDAP server or for the start TLS operation.
     * <P>
     *
     * @return the object representing the socket factory used to
     * establish a connection to a server or for the start TLS operation.
     * @see netscape.ldap.LDAPSocketFactory
     * @see netscape.ldap.LDAPSSLSocketFactory
     * @see netscape.ldap.LDAPConnection#setSocketFactory(netscape.ldap.LDAPSocketFactory)
     * @see netscape.ldap.LDAPConnection#startTLS
     */
    public LDAPSocketFactory getSocketFactory () {
        return m_factory;
    }

    /**
     * Specifies the object representing the socket factory that you
     * want to use to establish a connection to a server or for the
     * start TLS operation.
     * <P>
     * If the socket factory is to be used to establish a connection 
     * <CODE>setSocketFactory()</CODE> must be called before 
     * <CODE>connect()</CODE>. For the start TLS operation 
     * <CODE>setSocketFactory()</CODE> must be called after <CODE>connect()</CODE>.
     * @param factory the object representing the socket factory that
     * you want to use to establish a connection to a server or for
     * the start TLS operation.
     * @see netscape.ldap.LDAPSocketFactory
     * @see netscape.ldap.LDAPSSLSocketFactory
     * @see netscape.ldap.LDAPConnection#getSocketFactory
     * @see netscape.ldap.LDAPConnection#startTLS
     */
    public void setSocketFactory (LDAPSocketFactory factory) {
        m_factory = factory;
        m_isTLSFactory = false;
    }

    /**
     * Indicates whether the connection represented by this object
     * is open at this time.
     * @return <CODE>true</CODE> if connected to an LDAP server over this connection.
     * If not connected to an LDAP server, returns <CODE>false</CODE>.
     */
    public synchronized boolean isConnected() {
        return m_thread != null && m_thread.isConnected();
    }

    /**
     * Indicates whether this client has authenticated to the LDAP server
     * @return <CODE>true,</CODE>, if authenticated. If not
     * authenticated, or if authenticated as an anonymous user (with
     * either a blank name or password), returns <CODE>false</CODE>.
     */
    public boolean isAuthenticated () {
        boolean bound = false;
        synchronized (this) {
            bound = (m_thread != null) && m_thread.isBound();
        }
        return bound;
    }

    /**
     * Mark the connection bind state
     */
    synchronized void setBound(boolean bound) {
        if (m_thread != null) {
            if (!bound) {
                m_thread.setBound(false);
            }
            // sasl bind
            else if (m_saslBinder != null) {
                 m_thread.setBound(true);
            }
            // simple bind
            else {
                m_thread.setBound(!isAnonymousUser());
            }
        }
    }

    
    boolean isAnonymousUser() {
        return (m_boundDN == null) || m_boundDN.equals("") ||
               (m_boundPasswd == null) || m_boundPasswd.equals("");        
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
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     * @see netscape.ldap.LDAPConnection#setConnectTimeout
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
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     * @see netscape.ldap.LDAPConnection#setConnectTimeout
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
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     * @see netscape.ldap.LDAPConnection#setConnectTimeout
     */
    public void connect(String host, int port, String dn, String passwd,
        LDAPConstraints cons) throws LDAPException {
        connect(host, port, dn, passwd, cons, true);
    }

    /**
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void connect(String host, int port, String dn, String passwd,
        LDAPSearchConstraints cons) throws LDAPException {
        connect(host, port, dn, passwd, (LDAPConstraints)cons);
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
        m_connMgr = new LDAPConnSetupMgr(hostList, portList,
                                         m_isTLSFactory ? null : m_factory);
        m_connMgr.setConnSetupDelay(m_connSetupDelay);
        m_connMgr.setConnectTimeout(m_connectTimeout);
    
        connect();

        if (doAuthenticate) {
            authenticate(dn, passwd, cons);
        }
    }

    void connect(LDAPUrl[] urls) throws LDAPException {
        m_connMgr = new LDAPConnSetupMgr(urls, m_factory);
        m_connMgr.setConnSetupDelay(m_connSetupDelay);
        m_connMgr.setConnectTimeout(m_connectTimeout);
        connect();
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
     * @see netscape.ldap.LDAPConnection#setConnSetupDelay
     */
    public void connect(int version, String host, int port, String dn,
        String passwd, LDAPConstraints cons) throws LDAPException {

        setProtocolVersion(version);
        connect(host, port, dn, passwd, cons);
    }

    /**
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void connect(int version, String host, int port, String dn,
        String passwd, LDAPSearchConstraints cons) throws LDAPException {

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

        if (m_connMgr == null) {
            throw new LDAPException ( "no connection parameters",
                                      LDAPException.PARAM_ERROR );
        }        

        if (m_thread == null) {
            m_thread = new LDAPConnThread(m_connMgr, m_cache, getTraceOutput());
        }

        m_thread.connect(this);

        checkClientAuth();
    }

    /**
     * Returns the trace output object if set by the user
     */
    Object getTraceOutput() throws LDAPException {
        
        // Check first if trace output has been set using setProperty()
        Object traceOut = m_properties.get(TRACE_PROPERTY);
        if (traceOut != null) {
            return createTraceOutput(traceOut);
        }
        
        // Check if the property has been set with java -Dcom.netscape.ldap.trace
        // If the property does not have a value, send the trace to the System.err,
        // otherwise use the value as the output file name
        try {
            traceOut = System.getProperty(TRACE_PROPERTY);
            if (traceOut != null) {
                return createTraceOutput(traceOut);
            }
        }
        catch (Exception e) {
            ;// In browser access to property might not be allowed
        }
        return null;
    }        
        
        
    /**
     * Performs certificate-based authentication if client authentication was
     * specified at construction time.
     * @exception LDAPException if certificate-based authentication fails.
     */
    private void checkClientAuth() throws LDAPException {

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
     * @param searchResults the search results returned when the search
     * was started
     * @exception LDAPException Failed to notify the LDAP server.
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)
     * @see netscape.ldap.LDAPSearchResults
     */
    public void abandon( LDAPSearchResults searchResults )
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
     * Authenticates to the LDAP server (to which you are currently
     * connected) using the specified name and password.
     * If you are not connected to the LDAP server, this
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
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, String passwd) throws LDAPException {
        authenticate(m_protocolVersion, dn, passwd, m_defaultConstraints);
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
    public void authenticate(String dn, String passwd,
      LDAPConstraints cons) throws LDAPException {
        authenticate(m_protocolVersion, dn, passwd, cons);
    }

    /**
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void authenticate(String dn, String passwd,
      LDAPSearchConstraints cons) throws LDAPException {
        authenticate(dn, passwd, (LDAPConstraints)cons);
    }

    /**
     * Authenticates to the LDAP server (that you are currently
     * connected to) using the specified name and password, and
     * requesting that the server use at least the specified
     * protocol version. If the server doesn't support that
     * level, an LDAPException is thrown with the error code
     * PROTOCOL_ERROR.
     *
     * @param version required LDAP protocol version
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(int version, String dn, String passwd)
      throws LDAPException {
        authenticate(version, dn, passwd, m_defaultConstraints);
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
    public void authenticate(int version, String dn, String passwd,
      LDAPConstraints cons) throws LDAPException {
        m_protocolVersion = version;
        m_boundDN = dn;
        m_boundPasswd = passwd;
        forceNonSharedConnection();
        simpleBind(cons);
    }

    /**
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void authenticate(int version, String dn, String passwd,
      LDAPSearchConstraints cons) throws LDAPException {
        authenticate(version, dn, passwd, (LDAPConstraints)cons);
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
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param cbh a class which the SASL framework can call to obtain 
     * additional required information
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void authenticate(String dn, Hashtable props,
                             /*CallbackHandler*/ Object cbh)
        throws LDAPException {

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
     * @param mechanisms a list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @see netscape.ldap.LDAPConnection#authenticate(java.lang.String,
     * java.util.Hashtable, java.lang.Object)
     */
    public void authenticate(String dn, String[] mechanisms,
                             Hashtable props, /*CallbackHandler*/ Object cbh)
        throws LDAPException {

        authenticate( dn, mechanisms, DEFAULT_SASL_PACKAGE, props, cbh );
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
     * @param mechanism a single mechanism name, e.g. "GSSAPI"
     * @param packageName a package containing a SASL ClientFactory,
     * e.g. "myclasses.SASL". If null, a system default is used.
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @deprecated Please use authenticate without packageName
     * instead.
     */
    public void authenticate(String dn, String mechanism, String packageName,
                             Hashtable props, /*CallbackHandler*/ Object cbh)
        throws LDAPException {

        authenticate( dn, new String[] {mechanism}, packageName, props, cbh );
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
    public void authenticate(String dn, String[] mechanisms,
                             String packageName,
                             Hashtable props, /*CallbackHandler*/ Object cbh)
        throws LDAPException {

        forceNonSharedConnection();

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
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the specified name and password  and allows you
     * to specify constraints for this LDAP add operation by using an
     *  <CODE>LDAPConstraints</CODE> object. If the object
     * has been disconnected from an LDAP server, this method attempts to
     * reconnect to the server. If the object had already authenticated, the
     * old authentication is discarded.
     * 
     * @param version Required LDAP protocol version.
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
    public LDAPResponseListener authenticate(int version,
                                             String dn,
                                             String passwd,
                                             LDAPResponseListener listener,
                                             LDAPConstraints cons) 
                                             throws LDAPException{
        if (cons == null) {
            cons = m_defaultConstraints;
        }

        m_boundDN = dn;
        m_boundPasswd = passwd;
        m_protocolVersion = version;

        forceNonSharedConnection();

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }

        sendRequest(new JDAPBindRequest(version, m_boundDN, m_boundPasswd),
            listener, cons);

        return listener;
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
     * @param version Required LDAP protocol version.
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
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener authenticate(int version,
                                             String dn,
                                             String passwd,
                                             LDAPResponseListener listener) 
                                             throws LDAPException{
        return authenticate( version, dn, passwd, listener, m_defaultConstraints );
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
     * @param dn distinguished name used for authentication
     * @param passwd password used for authentication
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind(String dn, String passwd) throws LDAPException {
        authenticate(m_protocolVersion, dn, passwd, m_defaultConstraints);
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
    public void bind(String dn, String passwd,
                     LDAPConstraints cons) throws LDAPException {
        authenticate(m_protocolVersion, dn, passwd, cons);
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
    public void bind(int version, String dn, String passwd)
        throws LDAPException {
        authenticate(version, dn, passwd, m_defaultConstraints);
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
    public void bind(int version, String dn, String passwd,
      LDAPConstraints cons) throws LDAPException {
        authenticate(version, dn, passwd, cons);
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
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind(String dn, Hashtable props,
                     /*CallbackHandler*/ Object cbh)
        throws LDAPException {
        authenticate(dn, props, cbh);
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
     * @param mechanisms a list of acceptable mechanisms. The first one
     * for which a Mechanism Driver can be instantiated is returned.
     * @param cbh a class which the SASL framework can call to
     * obtain additional required information
     * @exception LDAPException Failed to authenticate to the LDAP server.
     * @see netscape.ldap.LDAPConnection#bind(java.lang.String,
     * java.util.Hashtable, java.lang.Object)
     */
    public void bind(String dn, String[] mechanisms,
                     Hashtable props, /*CallbackHandler*/ Object cbh)
        throws LDAPException {
        authenticate(dn, mechanisms, props, cbh);
    }

    /**
     * Begin using the Transport Layer Security (TLS) protocol for session
     * privacy.
     * <P>
     * Before <CODE>startTLS()</CODE> is called, a socket factory of type
     * <CODE>LDAPTLSSocketFactory</CODE> must be set for the connection.
     * The factory must be set after the <CODE>connect()</CODE> call. 
     * <P>
     * <PRE>
     * LDAPConnection ldc = new LDAPConnection();
     * try {
     *     ldc.connect(host, port);
     *     ldc.setSocketFactory(new JSSSocketFactory());
     *     ldc.startTLS();
     *     ldc.autheticate(3, userdn, userpw);
     * }
     * catch (LDAPException e) {
     * ...
     * }
     * </PRE>
     * <P>
     * If the socket factory of the connection is not capable of initiating a
     * TLS session , an LDAPException is thrown with the error code
     * TLS_NOT_SUPPORTED.
     * <P>
     * If the server does not support the transition to a TLS session, an
     * LDAPException is thrown with the error code returned by the server.
     * If there are outstanding LDAP operations on the connection or the 
     * connection is already in the secure mode, an LDAPException is thrown. 
     * 
     * @exception LDAPException Failed to convert to a TLS session.
     * @see netscape.ldap.LDAPTLSSocketFactory
     * @see netscape.ldap.LDAPConnection#setSocketFactory(netscape.ldap.LDAPSocketFactory)
     * @see netscape.ldap.LDAPConnection#isTLS
     * @since LDAPJDK 4.17
     */        
    public void startTLS() throws LDAPException {        

        if (m_useTLS) {
            throw new LDAPException("Already using TLS",
                                    LDAPException.OTHER);
        }

        if (m_factory == null || !( m_factory instanceof LDAPTLSSocketFactory)) {
            throw new LDAPException("No socket factory for the startTLS operation",
                                    LDAPException.OTHER);
        }

        // Denote that m_factory is to be used from now on used for TLS and not for
        // SSL connections.
        m_isTLSFactory = true;

        checkConnection(/*rebind=*/true);

        // If some requests still in progress, throw exception
        synchronized (this) {
            if (isConnected() && m_thread.getRequestCount() != 0) {
                throw new LDAPException(
                        "Connection has outstanding LDAP operations",
                        LDAPException.OTHER);
            }
        }

        // Send startTLS extended op
        try {
            LDAPExtendedOperation response = 
                extendedOperation(new LDAPExtendedOperation(OID_startTLS, null),
                                  m_defaultConstraints);
        }
        catch (LDAPException ex) {
            ex.setExtraMessage("Cannot start TLS");
            throw ex;
        }

        /**
         * Server accepted startTLS request, try to initiate TLS over the
         * current socket.
         */
        try {
            m_thread.layerSocket((LDAPTLSSocketFactory)m_factory);
            m_useTLS = true;
        }
        catch (LDAPException ex) {
            ex.setExtraMessage("Failed to start TLS");
            throw ex;
        }
        catch (Exception ex) {
            throw new LDAPException("Failed to start TLS",
                                    LDAPException.OTHER);
        }
    }

    /**
     * Indicates if the session is currently protected by TLS.
     * @return <CODE>true</CODE> if TLS was activated.
     * @since LDAPJDK 4.17
     * @see netscape.ldap.LDAPConnection#startTLS
     */
    public boolean isTLS() { 
        return m_useTLS;
    }

    /**
     * If this connection is cloned (the physical connection is shared),
     *  create a new physical connection.
     */
    void forceNonSharedConnection() throws LDAPException{
        checkConnection(/*rebind=*/false);
        if (m_thread != null && m_thread.getClientCount() > 1) {
            reconnect(/*rebind=*/false);
        }
    }

    /**
     * Internal routine. Binds to the LDAP server.
     * @exception LDAPException failed to bind
     */
    private void simpleBind (LDAPConstraints cons) throws LDAPException {

        m_saslBinder = null;

        LDAPResponseListener myListener = new LDAPResponseListener ( /*asynchOp=*/false );
        try {

            if (m_referralConnection != null && m_referralConnection.isConnected()) {
                m_referralConnection.disconnect();
            }
            m_referralConnection = null;
            
            setBound(false);
            sendRequest(new JDAPBindRequest(m_protocolVersion, m_boundDN,
                                            m_boundPasswd),
                        myListener, cons);
            checkMsg( myListener.getResponse() );

            setBound(true);
            m_rebindConstraints = (LDAPConstraints)cons.clone();

        } catch (LDAPReferralException e) {
            m_referralConnection = createReferralConnection(e, cons);
        }
    }

    /**
     * Send a request to the server
     */
    synchronized void sendRequest(JDAPProtocolOp oper, LDAPMessageQueue myListener,
                     LDAPConstraints cons) throws LDAPException {

        boolean requestSent=false;
        boolean restoreTried=false;

        while (!requestSent) {

            try {
                m_thread.sendRequest(this, oper, myListener, cons);

                /**
                 * In Java, a lost socket connected is often not detected until
                 * a read is attempted following a write. Wait for the response
                 * from the server to be sure the connection is really there.
                 */
                if (!myListener.isAsynchOp()) {
                    myListener.waitFirstMessage();
                }

                requestSent=true;
            }

            catch (IllegalArgumentException e) {
                throw new LDAPException(e.getMessage(), LDAPException.PARAM_ERROR);
            }
            catch(NullPointerException e) {
                if (isConnected() || restoreTried) {
                    break; // give up 
                }
                // else try to restore the connection
            }
            catch (LDAPException e) {
                if (e.getLDAPResultCode() != e.SERVER_DOWN || restoreTried) {
                    throw e; // give up
                }
                // else try to restore the connection
            }

            // Try to restore the connection if needed, but no more then once
            if (!requestSent && !restoreTried) {
                restoreTried = true;                
                myListener.reset();
                boolean rebind = !(oper instanceof JDAPBindRequest);
                restoreConnection(rebind);
            }
        }

        if (!requestSent) {
            throw new LDAPException("Failed to send request",
                LDAPException.OTHER);
        }
    }

    /**
     * Check and restore the connection if needed.
     * This method is used by the "smart failover" feature. If a server
     * or network error has occurred, an attempt is made to automatically
     * restore the connection on the next ldap operation request
     * @param rebind true if needs rebind after reconnect
     */
    private void checkConnection(boolean rebind) throws LDAPException {

        if (isConnected()) {
            return;
        }
        // If the user has invoked disconnect() no attempt is made
        // to restore the connection
        if (m_connMgr == null) {
            throw new LDAPException("not connected", LDAPException.OTHER);
        }

        restoreConnection(rebind);
    }
    
    /**
     * Reconnect and reauthenticate
     */
    private void restoreConnection(boolean rebind) throws LDAPException {
        connect();
        if (m_useTLS) {
            m_useTLS = false;
            startTLS();
        }                

        if (!rebind) {
            return;
        }

        if (m_saslBinder != null) {
            m_saslBinder.bind(this, false);
        }
        else if (m_rebindConstraints != null) {
            //Needs bind
            simpleBind(m_rebindConstraints);
        }
    }

    /**
     * Gets the authentication method used to bind:<BR>
     * "none", "simple", or "sasl"
     *
     * @return the authentication method, or "none"
     */
    public String getAuthenticationMethod() {
        if (!isAuthenticated()) {
            return "none";
        }
        else {
            return (m_saslBinder == null) ? "simple" : "sasl";
        }
        
    }

    /**
     * Disconnect from the server and then reconnect using the current
     * credentials and authentication method.<P>
     * If TLS was enabled with the startTLS() call, reenable TLS after
     * reconnect.
     * @exception LDAPException if not previously connected, or if
     * there is a failure on disconnecting or on connecting 
     */
    public void reconnect() throws LDAPException {        
        reconnect(/*rebind=*/true);
    }

    void reconnect(boolean rebind) throws LDAPException {
        // Save connect parameters that get cleared by disconnect()
        boolean useTLS = m_useTLS;
        LDAPConnSetupMgr connMgr = m_connMgr;
        LDAPConstraints rebindCons = m_rebindConstraints;

        disconnect();
        m_useTLS = useTLS;
        m_connMgr = connMgr;
        m_rebindConstraints = rebindCons;
        restoreConnection(rebind);
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
        if (!isConnected()) {
            return;
        }

        m_thread.deregister(this);

        if (m_referralConnection != null && m_referralConnection.isConnected()) {
            m_referralConnection.disconnect();
        }
        m_referralConnection = null;

        if (m_cache != null) {
            m_cache.removeReference();
            m_cache = null;
        }

        m_responseControlTable.clear();
        m_rebindConstraints = null;
        m_thread = null;
        m_connMgr = null;
        m_useTLS = false;
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
     * @param DN distinguished name of the entry  to retrieve
     * @exception LDAPException Failed to find or read the specified entry
     * from the directory.
     * @return LDAPEntry returns the specified entry or raises an exception
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
     * @param DN distinguished name of the entry to retrieve
     * @param cons preferences for the read operation
     * @exception LDAPException Failed to find or read the specified entry
     * from the directory.
     * @return LDAPEntry returns the specified entry or raises an exception
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
     * @param DN distinguished name of the entry to retrieve
     * @param attrs names of attributes to retrieve
     * @return LDAPEntry returns the specified entry (or raises an
     * exception if the entry is not found).
     * @exception LDAPException Failed to read the specified entry from
     * the directory.
     */
    public LDAPEntry read (String DN, String attrs[]) throws LDAPException {
        return read(DN, attrs, m_defaultConstraints);
    }

    public LDAPEntry read (String DN, String attrs[],
        LDAPSearchConstraints cons) throws LDAPException {
        LDAPSearchResults results =
            search (DN, SCOPE_BASE,
                    "(|(objectclass=*)(objectclass=ldapsubentry))",
                    attrs, false, cons);
        if (results == null) {
            return null;
        }
        LDAPEntry entry = results.next();
        
        // cleanup required for referral connections
        while (results.hasMoreElements()) {
            results.nextElement();
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
     * @param toGet LDAP URL specifying the entry to read
     * @return LDAPEntry returns the entry specified by the URL (or raises
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
        if (toGet.isSecure()) {
            LDAPSocketFactory factory = toGet.getSocketFactory();
            if (factory == null) {
                throw new LDAPException("No socket factory for LDAPUrl",
                                         LDAPException.OTHER);
            }
            connection.setSocketFactory(factory);
        }
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
     * @param toGet LDAP URL representing the search to perform
     * @return LDAPSearchResults the results of the search as an enumeration.
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
     *    int errCode = e.getLDAPResultCode();
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
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
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
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
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

        checkConnection(/*rebind=*/true);

        /* Is this a persistent search? */
        boolean isPersistentSearch = false;
        LDAPControl[] controls =
            (LDAPControl[])getOption(LDAPv3.SERVERCONTROLS, cons);
        for (int i = 0; (controls != null) && (i < controls.length); i++) {
            if ( controls[i] instanceof
                 netscape.ldap.controls.LDAPPersistSearchControl ) {
                isPersistentSearch = true;
                break;
            }
        }

        // Persistent search is an asynchronous operation
        LDAPSearchListener myListener = isPersistentSearch ? new LDAPSearchListener(/*asynchOp=*/true, cons) :
                                        getSearchListener ( cons );

        int deref = cons.getDereference();

        JDAPSearchRequest request = null;        
        try {
            request = new JDAPSearchRequest (base, scope, deref,
                cons.getMaxResults(), cons.getServerTimeLimit(),
                attrsOnly, filter, attrs);
        } catch (IllegalArgumentException e) {
            throw new LDAPException(e.getMessage(), LDAPException.PARAM_ERROR);
        }

        // if using cache, then need to add the key to the search listener.
        if ((m_cache != null) && (isKeyValid)) {
            myListener.setKey(key);

        }
        
        try {
            sendRequest (request, myListener, cons);
        }
        catch (LDAPException e) {
            releaseSearchListener (myListener);
            throw e;                    
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
                LDAPMessage response = myListener.completeSearchOperation();
                Enumeration results = myListener.getAllMessages().elements();

                checkSearchMsg(returnValue, response, cons, base, scope,
                               filter, attrs, attrsOnly);

                while (results.hasMoreElements ()) {
                    LDAPMessage msg = (LDAPMessage)results.nextElement();

                    checkSearchMsg(returnValue, msg, cons, base, scope, filter, attrs,
                        attrsOnly);
                }
            } finally {
                releaseSearchListener (myListener);
            }
        } else {
            /*
            * Asynchronous to retrieve one at a time, check to make sure
            * the search didn't fail
            */
            LDAPMessage firstResult = myListener.nextMessage ();
            if ( firstResult instanceof LDAPResponse ) {
                try {
                    checkSearchMsg(returnValue, firstResult, cons, base, scope,
                                   filter, attrs, attrsOnly);
                } finally {
                    releaseSearchListener (myListener);
                }
            } else {
                try {
                    checkSearchMsg(returnValue, firstResult, cons, base,
                                   scope, filter, attrs, attrsOnly);
                } catch ( LDAPException ex ) {
                    releaseSearchListener (myListener);
                    throw ex;
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

        value.setMsgID(msg.getMessageID());

        try {
            checkMsg (msg);
            // not the JDAPResult
            if (msg.getProtocolOp().getType() != JDAPProtocolOp.SEARCH_RESULT) {
                value.add (msg);
            }
        } catch (LDAPReferralException e) {
            Vector res = new Vector();
            
            try {
                performReferrals(e, cons, JDAPProtocolOp.SEARCH_REQUEST, dn,
                    scope, filter, attrs, attrsOnly, null, null, null, res);
            }
            catch (LDAPException ex) {
                if (msg.getProtocolOp() instanceof JDAPSearchResultReference) {
                    // Ignore Search Result Referral Errors ?
                    if (cons.getReferralErrors() == cons.REFERRAL_ERROR_CONTINUE) {
                        return; // Don't want to miss all remaining results
                    }
                    else {
                        throw ex;
                    }
                }
                throw ex;
            }

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
     * @param DN the distinguished name of the entry to use in
     * the comparison
     * @param attr the attribute to compare against the entry.
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
        checkConnection(/*rebind=*/true);

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
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public boolean compare( String DN, LDAPAttribute attr,
        LDAPSearchConstraints cons) throws LDAPException {
        return compare(DN, attr, (LDAPConstraints) cons);
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
     * attributes of the new entry
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to add the specified entry to the
     * directory.
     * @see netscape.ldap.LDAPEntry
     * @see netscape.ldap.LDAPConstraints
     */
    public void add( LDAPEntry entry, LDAPConstraints cons )
        throws LDAPException {
        checkConnection(/*rebind=*/true);

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
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void add( LDAPEntry entry, LDAPSearchConstraints cons )
        throws LDAPException {
        add(entry, (LDAPConstraints) cons);
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
     * extended operation and the data to use in the operation
     * @param cons preferences for the extended operation
     * @exception LDAPException Failed to execute the operation
     * @return LDAPExtendedOperation object representing the extended response
     * returned by the server.
     * @see netscape.ldap.LDAPExtendedOperation
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op,
                                                    LDAPConstraints cons)
        throws LDAPException {
        checkConnection(/*rebind=*/true);

        LDAPResponseListener myListener = getResponseListener ();
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
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public LDAPExtendedOperation extendedOperation( LDAPExtendedOperation op,
                                                    LDAPSearchConstraints cons)
        throws LDAPException {

        return extendedOperation(op, (LDAPConstraints)cons);
    }

    /**
     * Makes a single change to an existing entry in the directory.
     * For example, changes the value of an attribute, adds a new
     * attribute value, or removes an existing attribute value. <P>
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
     * @see netscape.ldap.LDAPModification
     */
    public void modify( String DN, LDAPModification mod ) throws LDAPException {
        modify(DN, mod, m_defaultConstraints);
    }

   /**
    * Makes a single change to an existing entry in the directory and
    * allows you to specify preferences for this LDAP modify operation
    * by using an <CODE>LDAPConstraints</CODE> object.
    * For example, you can specify whether or not to follow referrals.
    * You can also apply LDAP v3 controls to the operation.
    * <P>
    *
    * @param DN the distinguished name of the entry to modify
    * @param mod a single change to make to the entry
    * @param cons the set of preferences to apply to this operation
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
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void modify( String DN, LDAPModification mod,
        LDAPSearchConstraints cons ) throws LDAPException {
        modify (DN, mod, (LDAPConstraints)cons);
    }        

    /**
     * Makes a set of changes to an existing entry in the directory.
     * For example, changes attribute values, adds new attribute values,
     * or removes existing attribute values. <P>
     *
     * Use the <CODE>LDAPModificationSet</CODE> object to specify the set
     * of changes to make.  Changes are specified in terms
     * of attribute values.  You must specify each attribute value to modify, add,
     * or remove by an <CODE>LDAPAttribute</CODE> object.
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
     * @param DN the distinguished name of the entry to modify
     * @param mods a set of changes to make to the entry
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
     * by using an <CODE>LDAPConstraints</CODE> object.
     * For example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN the distinguished name of the entry to modify
     * @param mods a set of changes to make to the entry
     * @param cons the set of preferences to apply to this operation
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
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
     public void modify (String DN, LDAPModificationSet mods,
         LDAPSearchConstraints cons) throws LDAPException {
         modify(DN, mods, (LDAPConstraints)cons);
    }    
     
    /**
     * Makes a set of changes to an existing entry in the directory.
     * For example, changes attribute values, adds new attribute values,
     * or removes existing attribute values. <P>
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
     * @see netscape.ldap.LDAPModification
     */
    public void modify (String DN, LDAPModification[] mods)
        throws LDAPException {
        modify(DN, mods, m_defaultConstraints);
    }

    /**
     * Makes a set of changes to an existing entry in the directory and
     * allows you to specify preferences for this LDAP modify operation
     * by using an <CODE>LDAPConstraints</CODE> object.
     * For example, you can specify whether or not to follow referrals.
     * You can also apply LDAP v3 controls to the operation.
     * <P>
     *
     * @param DN the distinguished name of the entry to modify
     * @param mods an array of objects representing the changes to make
     * to the entry
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to make the specified changes to the
     * directory entry.
     * @see netscape.ldap.LDAPModification
     * @see netscape.ldap.LDAPConstraints
     */
    public void modify (String DN, LDAPModification[] mods,
         LDAPConstraints cons) throws LDAPException {
         checkConnection(/*rebind=*/true);

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
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void modify (String DN, LDAPModification[] mods,
         LDAPSearchConstraints cons) throws LDAPException {
        modify(DN, mods, (LDAPConstraints)cons);
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
     * @param DN distinguished name identifying the entry 
     * to remove from the directory
     * @param cons the set of preferences to apply to this operation
     * @exception LDAPException Failed to delete the specified entry from
     * the directory.
     * @see netscape.ldap.LDAPConstraints
     */
    public void delete( String DN, LDAPConstraints cons )
        throws LDAPException {
        checkConnection(/*rebind=*/true);

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
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void delete( String DN, LDAPSearchConstraints cons )
        throws LDAPException {
        delete(DN, (LDAPConstraints)cons);
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
    public void rename (String DN, String newRDN, boolean deleteOldRDN,
                        LDAPConstraints cons )
        throws LDAPException {
        rename(DN, newRDN, null, deleteOldRDN, cons);
    }

    /**
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void rename (String DN, String newRDN, boolean deleteOldRDN,
                        LDAPSearchConstraints cons )
        throws LDAPException {
        rename(DN, newRDN, deleteOldRDN, (LDAPConstraints)cons);
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
     * @see netscape.ldap.LDAPConstraints
     */
    public void rename (String DN,
                           String newRDN,
                           String newParentDN,
                           boolean deleteOldRDN,
                           LDAPConstraints cons)
        throws LDAPException {
        checkConnection(/*rebind=*/true);

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
     * @deprecated Please use the method signature where <CODE>cons</CODE> is
     * <CODE>LDAPConstraints</CODE> instead of <CODE>LDAPSearchConstraints</CODE>
     */
    public void rename (String DN,
                           String newRDN,
                           String newParentDN,
                           boolean deleteOldRDN,
                           LDAPSearchConstraints cons)
        throws LDAPException {
        rename(DN, newRDN, newParentDN, deleteOldRDN, (LDAPConstraints)cons);
    }        

    /**
     * Adds an entry to the directory.
     *
     * @param entry LDAPEntry object specifying the distinguished name and
     * attributes of the new entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
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
     * attributes of the new entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request
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
        
        checkConnection(/*rebind=*/true);

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
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener bind(int version,
                                     String dn,
                                     String passwd,
                                     LDAPResponseListener listener)
                                     throws LDAPException{
        return bind(version, dn, passwd, listener, m_defaultConstraints);
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
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     */
    public LDAPResponseListener bind(String dn,
                                     String passwd,
                                     LDAPResponseListener listener)
                                     throws LDAPException{
        return bind(m_protocolVersion, dn, passwd, listener,
                    m_defaultConstraints);
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
     * @return LDAPResponseListener handler for messages returned from a server
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
        return bind( m_protocolVersion, dn, passwd, listener, cons );
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
     * @return LDAPResponseListener handler for messages returned from a server
     * in response to this request.
     * @exception LDAPException Failed to send request.
     * @see netscape.ldap.LDAPResponseListener
     * @see netscape.ldap.LDAPConstraints
     */
    public LDAPResponseListener bind(int version,
                                     String dn,
                                     String passwd,
                                     LDAPResponseListener listener,
                                     LDAPConstraints cons) 
                                     throws LDAPException{
        return authenticate( version, dn, passwd, listener, cons );
    }
    
    /**
     * Deletes the entry for the specified DN from the directory.
     * 
     * @param dn distinguished name of the entry to delete
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
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
     * @param dn distinguished name of the entry to delete
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
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

        checkConnection(/*rebind=*/true);

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }
        
        sendRequest (new JDAPDeleteRequest(dn), listener, cons);
        
        return listener;

    }
    
    /**
     * Makes a single change to an existing entry in the directory.
     * For example, changes the value of an attribute, adds a new attribute
     * value, or removes an existing attribute value.<BR>
     * The LDAPModification object specifies both the change to make and
     * the LDAPAttribute value to be changed.
     * 
     * @param dn distinguished name of the entry to modify
     * @param mod a single change to make to an entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
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
     * Makes a single change to an existing entry in the directory.
     * For example, changes the value of an attribute, adds a new attribute
     * value, or removes an existing attribute value.<BR>
     * The LDAPModification object specifies both the change to make and
     * the LDAPAttribute value to be changed.
     * 
     * @param dn distinguished name of the entry to modify
     * @param mod a single change to make to an entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
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

        checkConnection(/*rebind=*/true);

        if (listener == null) {
            listener = new LDAPResponseListener(/*asynchOp=*/true);
        }

        LDAPModification[] modList = { mod };
        sendRequest (new JDAPModifyRequest (dn, modList), listener, cons);        

        return listener;
    }

    /**
     * Makes a set of changes to an existing entry in the directory.
     * For example, changes attribute values, adds new attribute values, or
     * removes existing attribute values.
     * <P>
     * @param dn distinguished name of the entry to modify
     * @param mods a set of changes to make to the entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
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
     * Makes a set of changes to an existing entry in the directory.
     * For example, changes attribute values, adds new attribute values, or
     * removes existing attribute values).
     * 
     * @param dn distinguished name of the entry to modify
     * @param mods a set of changes to make to the entry
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons Constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
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

        checkConnection(/*rebind=*/true);

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
     * @param dn current distinguished name of the entry
     * @param newRdn new relative distinguished name for the entry
     * @param deleteOldRdn if true, the old name is not retained as an
     * attribute value
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
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
     * @param dn current distinguished name of the entry
     * @param newRdn new relative distinguished name for the entry
     * @param deleteOldRdn if true, the old name is not retained as an attribute
     * value
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to the operation
     * @return LDAPResponseListener handler for messages returned from a server
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
        
        checkConnection(/*rebind=*/true);

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
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
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
     * @return LDAPSearchListener handler for messages returned from a server
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
     * @param base the base distinguished name from which to search
     * @param scope the scope of the entries to search.  You can specify one
     * of the following: <P>
     * <UL>
     * <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (search only the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_ONE</CODE>
     * (search only entries under the base DN) <P>
     * <LI><CODE>LDAPv2.SCOPE_SUB</CODE>
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
     * @return LDAPSearchListener handler for messages returned from a server
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

        checkConnection(/*rebind=*/true);
        
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
     * @param dn distinguished name of the entry to compare
     * @param attr attribute with a value to compare
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @return LDAPResponseListener handler for messages returned from a server
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
     * @param dn distinguished name of the entry to compare
     * @param attr attribute with a value to compare
     * @param listener handler for messages returned from a server in response
     * to this request. If it is null, a listener object is created internally.
     * @param cons constraints specific to this operation
     * @return LDAPResponseListener handler for messages returned from a server
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
        
        checkConnection(/*rebind=*/true);

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
     * @param id an LDAP request id
     * @exception LDAPException Failed to send request.
     */
    public void abandon(int id) throws LDAPException {

        if (!isConnected()) {
            return;
        }
        
        try {
            /* Tell listener thread to discard results and send an abandon request */
            LDAPControl ctrls[] =  m_defaultConstraints.getServerControls();
            m_thread.abandon( id, ctrls );
                
        } catch (Exception ignore) {}
    }

    /**
     * Cancels all outstanding search requests associated with this
     * LDAPSearchListener object and discards any results already received.
     * 
     * @param searchlistener a search listener returned from a search
     * @exception LDAPException Failed to send request.
     */
    public void abandon(LDAPSearchListener searchlistener)throws LDAPException {
        int[] ids = searchlistener.getMessageIDs();
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
     * @param option you can specify one of the following options:
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
     * <CODE>LDAPv2.BIND</CODE></TD>
     * <TD><CODE>LDAPBind</CODE></TD>
     * <TD>Specifies an object with a class that implements the
     * <CODE>LDAPBind</CODE>
     * interface.  You must define this class and the
     * <CODE>bind</CODE> method that will be used to authenticate
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
     * <P>By default, the value of this option is 100. The value 0 means there
     *  is no limit.</TD></TR>
     * </TABLE><P>
     * @return the value for the option wrapped in an object.  (You
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
     * @param option you can specify one of the following options:
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
     * <CODE>LDAPv2.BIND</CODE></TD>
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
     * <P>By default, the value of this option is 100. The value 0 means there
     *  is no limit.</TD></TR>
     * </TABLE><P>
     * @param value the value to assign to the option.  The value must be
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
          cons.setMaxBacklog(((Integer)value).intValue());
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
     * @return an array of the controls returned by an operation, or
     * null if none.
     * @see netscape.ldap.LDAPControl
     * @see netscape.ldap.LDAPSearchResults#getResponseControls
     */
    public LDAPControl[] getResponseControls() {
        LDAPControl[] controls = null;
        Thread caller = Thread.currentThread();
      
        /* Get the latest controls for the caller thread */
        synchronized(m_responseControlTable) {
            ResponseControls rspCtrls =
                (ResponseControls) m_responseControlTable.get(caller);

            if (rspCtrls != null) {
                Vector v = rspCtrls.ctrls;
                controls = (LDAPControl[]) v.elementAt(0);
                v.removeElementAt(0);
                if (v.size() == 0) {
                    m_responseControlTable.remove(caller);
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

        synchronized(m_responseControlTable) {            
            Enumeration enum = m_responseControlTable.keys();          
            while (enum.hasMoreElements()) {
                Object client = enum.nextElement();
                ResponseControls rspCtrls = (ResponseControls)m_responseControlTable.get(client);

                if (msgID == rspCtrls.msgID) {
                    Vector v = rspCtrls.ctrls;
                    controls = (LDAPControl[]) v.elementAt(0);
                    v.removeElementAt(0);
                    if (v.size() == 0) {
                        m_responseControlTable.remove(client);
                    }
                    break;
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
     *                                          LDAPv2.SCOPE_SUB,
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
     * @see netscape.ldap.LDAPSearchConstraints
     * @see netscape.ldap.LDAPConnection#getOption
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean, netscape.ldap.LDAPSearchConstraints)  
     */
    public LDAPSearchConstraints getSearchConstraints () {
        return (LDAPSearchConstraints)m_defaultConstraints.clone();
    }


    /**
     * Set the default constraint set for all operations. 
     * @param cons <CODE>LDAPConstraints</CODE> object to use as the default
     * constraint set
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
     * @param cons <CODE>LDAPSearchConstraints</CODE> object to use as the
     * default constraint set
     * @see netscape.ldap.LDAPConnection#getSearchConstraints
     * @see netscape.ldap.LDAPSearchConstraints
     */
    public void setSearchConstraints(LDAPSearchConstraints cons) {
        m_defaultConstraints = (LDAPSearchConstraints)cons.clone();
    }

    /**
     * Gets the stream for reading from the listener socket
     *
     * @return the stream for reading from the listener socket, or
     * <CODE>null</CODE> if there is none
     */
    public InputStream getInputStream() {
        return (m_thread != null) ? m_thread.getInputStream() : null;
    }

    /**
     * Sets the stream for reading from the listener socket if
     * there is one
     *
     * @param is the stream for reading from the listener socket
     */
    public void setInputStream( InputStream is ) {
        if ( m_thread != null ) {
            m_thread.setInputStream( is );
        }
    }

    /**
     * Gets the stream for writing to the socket
     *
     * @return the stream for writing to the socket, or
     * <CODE>null</CODE> if there is none
     */
    public OutputStream getOutputStream() {
        return (m_thread != null) ? m_thread.getOutputStream() : null;
    }

    /**
     * Sets the stream for writing to the socket
     *
     * @param os the stream for writing to the socket, if there is one
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
     * @return a search response listener object
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
            l.setSearchConstraints(cons);
        }
        return l;
    }

    /**
     * Put a listening agent into the internal buffer of available agents.
     * These objects are used to make the asynchronous LDAP operations
     * synchronous.
     * @param l listener to buffer
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
     * @param l listener to buffer
     */
    synchronized void releaseSearchListener (LDAPSearchListener l) {

        if (l.isAsynchOp()) { // persistent search 
            return;
        }

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
     * Extract response controls from the message, if any avaialble.
     * @param m server response to validate
     * @exception LDAPException failed to check message
     */
    void checkMsg (LDAPMessage m) throws LDAPException {

      // Check for response controls
      LDAPControl[] ctrls = m.getControls();
      if (ctrls != null) {
          int msgID = m.getMessageID();
          setResponseControls(Thread.currentThread(), msgID, ctrls);
      }
    
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
     * @param current the target thread
     * @param con the server response controls
     */
    void setResponseControls( Thread client, int msgID, LDAPControl[] ctrls ) {
        synchronized(m_responseControlTable) {
            ResponseControls rspCtrls = (ResponseControls)m_responseControlTable.get(client);

            if (rspCtrls == null || rspCtrls.msgID != msgID) {
                rspCtrls = new ResponseControls(msgID, ctrls);

                m_responseControlTable.put(client, rspCtrls);
            }
            else {
                rspCtrls.addControls(ctrls);
            }
        }
    }

    /**
     * Set up connection for referral.
     * @param u referral URL
     * @param cons search constraints
     * @return new LDAPConnection, already connected and authenticated
     */
    private LDAPConnection referralConnect( LDAPUrl[] refList,
                                            LDAPConstraints cons )
        throws LDAPException {
        LDAPConnection connection = new LDAPConnection (getSocketFactory());
        
        // Set the same connection setup failover policy as for this connection
        connection.setConnSetupDelay(getConnSetupDelay());
        
        connection.setOption(REFERRALS, new Boolean(true));
        connection.setOption(REFERRALS_REBIND_PROC, cons.getRebindProc());
        connection.setOption(BIND, cons.getBindProc());
  
        Object traceOut = getProperty(TRACE_PROPERTY);
        if (traceOut != null) {
            connection.setProperty(TRACE_PROPERTY, traceOut);
        }
          
        // need to set the protocol version which gets passed to connection
        connection.setOption(PROTOCOL_VERSION,
                              new Integer(m_protocolVersion));

        connection.setOption(REFERRALS_HOP_LIMIT,
                              new Integer(cons.getHopLimit()-1));

        try { 
            connection.connect (refList);
        }
        catch (LDAPException e) {
            throw new LDAPException("Referral connect failed: " + e.getMessage(),
                e.getLDAPResultCode());
        }
        return connection;
    }

    private void referralRebind(LDAPConnection ldc, LDAPConstraints cons)
        throws LDAPException{
        try {
            if (cons.getRebindProc() == null && cons.getBindProc() == null) {
                ldc.authenticate (m_protocolVersion, null, null);
            } else if (cons.getBindProc() == null) {
                LDAPRebindAuth auth =
                  cons.getRebindProc().getRebindAuthentication(ldc.getHost(),
                                                               ldc.getPort());
                ldc.authenticate(m_protocolVersion, auth.getDN(), auth.getPassword());
            } else {
                cons.getBindProc().bind(ldc);
            }
        }
        catch (LDAPException e) {
            throw new LDAPException("Referral bind failed: " + e.getMessage(),
                e.getLDAPResultCode());
        }            
    }

    /**
     * Check for "ldap(s):///" referrals (no host:port) and replace them with
     * "ldap(s)://currentHost:currentPort".
     */
    private void adjustReferrals(LDAPUrl[] urls) {
        String host = null;
        int port =0;
        
        for (int i=0; urls != null && i < urls.length; i++) {
            host = urls[i].getHost();
            port = urls[i].getPort();
            if ( (host == null) || (host.length() < 1) ) {
                // If no host:port was specified, use the latest (hop-wise) parameters
                host = getHost();
                port = getPort();
                urls[i] = new  LDAPUrl (host, port,
                                        urls[i].getDN(),
                                        urls[i].getAttributeArray(),
                                        urls[i].getScope(),
                                        urls[i].getFilter(),
                                        urls[i].isSecure());                
            }
        }
    }

    /**
     * Establish the LDAPConnection to the referred server. This one is used
     * for bind operation only since we need to keep this new connection for
     * the subsequent operations. This new connection will be destroyed in
     * two circumstances: disconnect gets called or the client binds as
     * someone else.
     * @return the new LDAPConnection to the referred server
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

        LDAPUrl[] refList = e.getURLs();
        
        // If there are no referrals (because the server isn't set up for
        // them), give up here
        if (refList == null) {
            throw new LDAPException("No target URL in referral",
                                    LDAPException.NO_RESULTS_RETURNED);
        }
        adjustReferrals(refList);
        
        LDAPConnection connection = referralConnect(refList, cons);

        // which one did we connect to...
        LDAPUrl refURL = connection.m_connMgr.getLDAPUrl();

        String refDN = refURL.getDN();
        if ((refDN == null) || (refDN.equals(""))) {
            refDN = m_boundDN;
        }

        try {
            connection.authenticate(m_protocolVersion, refDN, m_boundPasswd);
        }
        catch (LDAPException authEx) {
            // Disconnect needed to terminate the LDAPConnThread
            try  {                
                connection.disconnect();
            }
            catch (LDAPException ignore) {}
            throw authEx;
        }
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

        LDAPUrl refURL = null;
        LDAPConnection connection = null;
        
        try {        

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

            LDAPUrl urls[] = e.getURLs();
            // If there are no referrals (because the server isn't configured to
            // return one), give up here
            if ( urls == null || urls.length == 0) {
                return;
            }
            adjustReferrals(urls);
                
            // Check if we can use m_referralConnection to follow this referral                
            if (m_referralConnection != null && m_referralConnection.isConnected()) {
                String refHost = m_referralConnection.getHost();
                int    refPort = m_referralConnection.getPort();
                try {
                    // Compare ipAddr:port for each referral with the m_referralConnection
                    String refAddr = InetAddress.getByName(refHost).getHostAddress();                    
                    for (int i = 0; i < urls.length; i++) {
                        String urlHost = urls[i].getHost();
                        int    urlPort = urls[i].getPort();
                        String urlAddr = InetAddress.getByName(urlHost).getHostAddress();
                        if (refAddr == urlAddr && refPort == urlPort) {
                            refURL = urls[i];
                            break;
                        }
                    }
                }
                catch (UnknownHostException ex) {
                    // Compare host names rather than ip addr
                    for (int i = 0; i < urls.length; i++) {
                        if (refHost == urls[i].getHost() &&
                            refPort == urls[i].getPort()) {
                            refURL = urls[i];
                            break;
                        }
                    }
                }
            }

            if (refURL != null) {
                connection = m_referralConnection;
            }
            else {
                connection = referralConnect( urls, cons );
                    
                // which one did we connect to...
                refURL = connection.m_connMgr.getLDAPUrl();
                    
                // Authenticate
                referralRebind(connection, cons);
            }

            String newDN = refURL.getDN();
            String DN = null;
            if ((newDN == null) || (newDN.equals(""))) {
                DN = dn;
            } else {
                DN = newDN;
            }
            // If this was a one-level search, and a direct subordinate
            // has a referral, there will be a "?base" in the URL, and
            // we have to rewrite the scope from one to base
            if ( refURL.getUrl().indexOf("?base") > -1 ) {
                scope = SCOPE_BASE;
            }

            LDAPSearchConstraints newcons = (LDAPSearchConstraints)cons.clone();
            newcons.setHopLimit( cons.getHopLimit()-1 );

            referralOperation(connection, newcons, ops, DN, scope, filter,
                             types, attrsOnly, mods, entry, attr, results);

        }
        catch (LDAPException ex) {
            if (refURL != null) {
                ex.setExtraMessage("Failed to follow referral to " + refURL);
            }
            else {
                ex.setExtraMessage("Failed to follow referral");
            }
            throw ex;
        }
    }

    void referralOperation(LDAPConnection connection, 
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
        if ( u == null || u.length == 0) {
            return null;
        }
        adjustReferrals(u);
        
        LDAPConnection connection = referralConnect( u, cons);
        referralRebind(connection, cons);
        LDAPExtendedOperation results =
            connection.extendedOperation( op );
        connection.disconnect();
        return results; /* return right away if operation is successful */
        
    }

    /**
     * Returns a new <CODE>LDAPConnection</CODE> object that shares
     * the physical connection to the LDAP server but has its own state.
     *     
     * The returned <CODE>LDAPConnection</CODE> object contains the same
     * state as the current connection, including:
     * <UL>
     * <LI>the default search constraints
     * <LI>host name and port number of the LDAP server
     * <LI>the DN and password used to authenticate to the LDAP server
     * <LI>the cache
     * </UL>
     * <P>
     * @return the <CODE>LDAPconnection</CODE> object representing the
     * new object.
     */
    public synchronized Object clone() {

        LDAPConnection c = null; 
        
        try {
            if (m_thread != null) {
                checkConnection(/*rebind=*/true);
            }
        }
        catch (LDAPException ignore) {}
        
        try {
            c = (LDAPConnection)super.clone();
            c.m_defaultConstraints =
                (LDAPSearchConstraints)m_defaultConstraints.clone();
            c.m_responseListeners = null;
            c.m_searchListeners = null;            
            c.m_properties = (Hashtable)m_properties.clone();
            c.m_responseControlTable = new Hashtable();

            if (c.m_cache != null) {
                c.m_cache.addReference();
            }

            if (isConnected()) {
                /* share current connection thread */
                c.m_thread.register(c);
            }
            else {
                c.m_thread  = null;
                c.m_connMgr = null;
            }

        } catch (Exception ignore) {}

        return c;
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
     * <PRE>LDAPConnection {ldap://dilly:389 (2) ldapVersion:3 bindDN:"uid=admin,o=iplanet.com"}</PRE>
     * 
     * For cloned connections, the number of LDAPConnection instances sharing the 
     * same physical connection is shown in parenthesis following the ldap url. 
     * If a LDAPConnection is not cloned, this number is omitted from the string
     * representation.
     *
     * @return string representation of the connection.
     * @see netscape.ldap.LDAPConnection#clone
     */    
    public String toString() {
        int cloneCnt = (m_thread == null) ? 0 : m_thread.getClientCount();
        StringBuffer sb = new StringBuffer("LDAPConnection {");
        //url
        if (m_connMgr != null) {
            sb.append(m_connMgr.getLDAPUrl().getServerUrl());
        }
        // clone count
        if (cloneCnt > 1) {
            sb.append(" (");
            sb.append(cloneCnt);
            sb.append(")");
        }
        // ldap version
        sb.append(" ldapVersion:");
        sb.append(m_protocolVersion);
        // bind DN
        sb.append(" bindDN:\"");
        if (getAuthenticationDN() != null) {
            sb.append(getAuthenticationDN());
        }
        sb.append("\"}");
        
        return sb.toString();
    }

    /** 
     * A helper class for collecting response controls. Used as a value 
     * in m_responseControlTable
     */
    class ResponseControls {
        int msgID;
        Vector ctrls;
        
        public ResponseControls(int msgID, LDAPControl[] ctrls) {
            this.msgID = msgID;
            this.ctrls = new Vector();
            this.ctrls.addElement(ctrls);
        }

        void addControls(LDAPControl[] ctrls) {
            this.ctrls.addElement(ctrls);
        }
    }

    
    /**
     * Prints out the LDAP Java SDK version and the highest LDAP
     * protocol version supported by the SDK. To view this
     * information, open a terminal window, and enter:
     * <PRE>java netscape.ldap.LDAPConnection
     * </PRE>
     * @param args not currently used
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
