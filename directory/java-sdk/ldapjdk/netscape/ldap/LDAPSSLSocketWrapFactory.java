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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap;

import java.io.*;
import java.net.*;
import java.util.Hashtable;

/**
 * Creates an SSL socket connection to an LDAP Server. This class is provided
 * by the package in which the SSL socket does not extend Socket object.
 * The class internally provides a wrapper to convert the SSL socket extending
 * the Object class to the one extending the Socket class.
 * This factory class implements the <CODE>LDAPSocketFactory</CODE> interface.
 * <P>
 *
 * To use this class, pass the instance of this factory object to the
 * <CODE>LDAPConnection</CODE> constructor.
 *
 * @version 1.0
 * @see LDAPSocketFactory
 * @see LDAPConnection#LDAPConnection(netscape.ldap.LDAPSocketFactory)
 */
public class LDAPSSLSocketWrapFactory implements LDAPSSLSocketFactoryExt {

    /**
     * The constructor with the specified package for security
     * @param className The name of a class which has an implementation
     * of SSL Socket extending Object class.
     */
    public LDAPSSLSocketWrapFactory(String className) {
        m_packageName = new String(className);
    }

    /**
     * The constructor with the specified package for security and the
     * specified cipher suites.
     * @param className The name of a class which has an implementation
     * of SSL Socket extending Object class.
     * @param cipherSuites The cipher suites.
     */
    public LDAPSSLSocketWrapFactory(String className, Object cipherSuites) {
        m_packageName = new String(className);
        m_cipherSuites = cipherSuites;
    }

    /**
     * Returns socket to the specified host name and port number.
     * @param host The host to connect to
     * @param port The port number
     * @return The socket to the host name and port number as passed in.
     * @exception LDAPException A socket to the specified host and port
     * could not be created.
     */
    public Socket makeSocket(String host, int port) throws LDAPException {

        LDAPSSLSocket s = null;

        try {
            if (m_cipherSuites == null)
                s = new LDAPSSLSocket(host, port, m_packageName);
            else
                s = new LDAPSSLSocket(host, port, m_packageName,
                  m_cipherSuites);
            return s;
        } catch (Exception e) {
            System.err.println("Exception: "+e.toString());
            throw new LDAPException("Failed to create SSL socket",
              LDAPException.CONNECT_ERROR);
        }
    }

    /**
     * Returns <code>true</code> if client authentication is to be used.
     * @return true if client authentication is enabled, otherwise, false if
     * client authentication is disabled.
     */
    public boolean isClientAuth() {
        return m_clientAuth;
    }

    /**
     * <B>(Not implemented yet)</B> <BR>
     * Enables client authentication for an application running in
     * a java VM which provides transparent certificate database management.
     * Calling this method has no effect after makeSocket() has been
     * called.
     * @exception LDAPException Since this method is not yet implemented,
     * calling this method throws an exception.
     */
    public void enableClientAuth() throws LDAPException {
        throw new LDAPException("Client Authentication is not implemented yet.");
    }

    /**
     * Returns the name of the class that implements SSL sockets for this factory.
     *
     * @return The name of the class that implements SSL sockets for this factory.
     */
    public String getSSLSocketImpl() {
        return m_packageName;
    }

    /**
     * Returns the suite of ciphers used for SSL connections made through
     * sockets created by this factory.
     *
     * @return The suite of ciphers used.
     */
    public Object getCipherSuites() {
        return m_cipherSuites;
    }

    /**
     * Indicates if client authentication is on.
     */
    private boolean m_clientAuth = false;

    /**
     * Name of class implementing SSLSocket.
     */
    private String m_packageName = null;

    /**
     * The cipher suites
     */
    private Object m_cipherSuites = null;
}

// LDAPSSLSocket class wraps the implementation of the SSL socket
class LDAPSSLSocket extends Socket {

    public LDAPSSLSocket(String host, int port, String packageName)
      throws LDAPException {
        super();
        m_packageName = packageName;
        try {
            // instantiate the SSLSocketFactory implementation, and
            // find the right constructor
            Class c = Class.forName(m_packageName);
            java.lang.reflect.Constructor[] m = c.getConstructors();

            for (int i = 0; i < m.length; i++) {
                /* Check if the signature is right: String, int */
                Class[] params = m[i].getParameterTypes();

                if ((params.length == 2) &&
                    (params[0].getName().equals("java.lang.String")) &&
                    (params[1].getName().equals("int"))) {
                    Object[] args = new Object[2];
                    args[0] = host;
                    args[1] = new Integer(port);
                    m_socket = (Object)(m[i].newInstance(args));
                    return;
                }
            }
            throw new LDAPException("No appropriate constructor in " +
              m_packageName, LDAPException.PARAM_ERROR);
        } catch (ClassNotFoundException e) {
            throw new LDAPException("Class " + m_packageName + " not found",
              LDAPException.OTHER);
        } catch (Exception e) {
            throw new LDAPException("Failed to create SSL socket",
              LDAPException.CONNECT_ERROR);
        }
    }

    public LDAPSSLSocket(String host, int port, String packageName,
      Object cipherSuites) throws LDAPException {
        super();
        m_packageName = packageName;
        String cipherClassName = null;
        if (cipherSuites != null)
            cipherClassName = cipherSuites.getClass().getName();

        try {
            // instantiate the SSLSocketFactory implementation, and
            // find the right constructor
            Class c = Class.forName(m_packageName);
            java.lang.reflect.Constructor[] m = c.getConstructors();

            for (int i = 0; i < m.length; i++) {
                /* Check if the signature is right: String, int */
                Class[] params = m[i].getParameterTypes();
                if (cipherSuites == null)
                    throw new LDAPException("Cipher Suites is required");

                if ((params.length == 3) &&
                    (params[0].getName().equals("java.lang.String")) &&
                    (params[1].getName().equals("int")) &&
                    (params[2].getName().equals(cipherClassName))) {
                    Object[] args = new Object[3];
                    args[0] = host;
                    args[1] = new Integer(port);
                    args[2] = cipherSuites;
                    m_socket = (Object)(m[i].newInstance(args));
                    return;
                }
            }
            throw new LDAPException("No appropriate constructor in " +
              m_packageName, LDAPException.PARAM_ERROR);
        } catch (ClassNotFoundException e) {
            throw new LDAPException("Class " + m_packageName + " not found",
              LDAPException.OTHER);
        } catch (Exception e) {
            throw new LDAPException("Failed to create SSL socket",
              LDAPException.CONNECT_ERROR);
        }
    }

    public InputStream getInputStream() {
        try {
            Object obj = invokeMethod(m_socket, "getInputStream", null);
            return (InputStream)obj;
        } catch (LDAPException e) {
            printDebug(e.toString());
        }

        return null;
    }

    public OutputStream getOutputStream() {
        try {
            Object obj = invokeMethod(m_socket, "getOutputStream", null);
            return (OutputStream)obj;
        } catch (LDAPException e) {
            printDebug(e.toString());
        }

        return null;
    }

    public void close() throws IOException {
        try {
            invokeMethod(m_socket, "close", null);
        } catch (LDAPException e) {
            printDebug(e.toString());
        }
    }

    public void close(boolean wait) throws IOException {
        try {
            Object[] args = new Object[1];
            args[0] = new Boolean(wait);
            invokeMethod(m_socket, "close", args);
        } catch (LDAPException e) {
            printDebug(e.toString());
        }
    }

    public InetAddress getInetAddress() {
        try {
            Object obj = invokeMethod(m_socket, "getInetAddress", null);
            return (InetAddress)obj;
        } catch (LDAPException e) {
            printDebug(e.toString());
        }

        return null;
    }

    public int getLocalPort() {
        try {
            Object obj = invokeMethod(m_socket, "getLocalPort", null);
            return ((Integer)obj).intValue();
        } catch (LDAPException e) {
            printDebug(e.toString());
        }

        return -1;
    }

    public int getPort() {
        try {
            Object obj = invokeMethod(m_socket, "getPort", null);
            return ((Integer)obj).intValue();
        } catch (LDAPException e) {
           printDebug(e.toString());
        }

        return -1;
    }

    private Object invokeMethod(Object obj, String name, Object[] args) throws
      LDAPException {
        try {
            java.lang.reflect.Method m = getMethod(name);
            if (m != null) {
                return (m.invoke(obj, args));
            }
        } catch (Exception e) {
            throw new LDAPException("Invoking "+name+": "+
              e.toString(), LDAPException.PARAM_ERROR);
        }

        return null;
    }

    private java.lang.reflect.Method getMethod(String name) throws
      LDAPException {
        try {
            java.lang.reflect.Method method = null;
            if ((method = (java.lang.reflect.Method)(m_methodLookup.get(name)))
              != null)
                return method;

            Class c = Class.forName(m_packageName);
            java.lang.reflect.Method[] m = c.getMethods();
            for (int i = 0; i < m.length; i++ ) {
                if (m[i].getName().equals(name)) {
                    m_methodLookup.put(name, m[i]);
                    return m[i];
                }
            }
            throw new LDAPException("Method " + name + " not found in " +
              m_packageName);
        } catch (ClassNotFoundException e) {
            throw new LDAPException("Class "+ m_packageName + " not found");
        }
    }

    private void printDebug(String msg) {
        if (m_debug) {
            System.out.println(msg);
        }
    }

    private final boolean m_debug = true;
    private Object m_socket;
    private Hashtable m_methodLookup = new Hashtable();
    private String m_packageName = null;
}

