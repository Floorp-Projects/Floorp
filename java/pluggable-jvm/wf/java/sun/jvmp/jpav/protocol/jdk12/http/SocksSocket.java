/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is The Waterfall Java Plugin Module
 * 
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 *
 * $Id: SocksSocket.java,v 1.1 2001/05/10 18:12:31 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

/*
 * @(#)SocksSocket.java	1.4 00/02/16
 * 
 * Copyright (c) 1995, 1996 Sun Microsystems, Inc. All Rights Reserved.
 * 
 * This software is the confidential and proprietary information of Sun
 * Microsystems, Inc. ("Confidential Information").  You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered into
 * with Sun.
 * 
 * SUN MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
 * SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT. SUN SHALL NOT BE LIABLE FOR ANY DAMAGES
 * SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
 * THIS SOFTWARE OR ITS DERIVATIVES.
 * 
 * CopyrightVersion 1.1_beta
 * 
 * @author Stanley Man-Kit Ho
 */

package sun.jvmp.jpav.protocol.jdk12.http;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.net.Socket;
import java.net.SocketException;

/**
 * This class implements client sockets (also called just 
 * "sockets") through SOCKS. A socket is an endpoint for communication 
 * between two machines. 
 * <p>
 * The actual work of the socket is performed by an instance of the 
 * <code>SocketImpl</code> class. An application, by changing 
 * the socket factory that creates the socket implementation, 
 * can configure itself to create sockets appropriate to the local 
 * firewall. 
 *
 * @author  Stanley Man-Kit Ho
 * @version 1.00, 04/23/98
 * @see     java.net.Socket
 * @since   JDK1.0
 */
public class SocksSocket extends Socket {

    private Socket socket = null;

    /* SOCKS related constants */

    private static final int SOCKS_PROTO_VERS		= 4;
    private static final int SOCKS_REPLY_VERS		= 4;

    private static final int COMMAND_CONNECT		= 1;
    private static final int COMMAND_BIND		= 2;

    private static final int REQUEST_GRANTED		= 90;
    private static final int REQUEST_REJECTED		= 91;
    private static final int REQUEST_REJECTED_NO_IDENTD  = 92;
    private static final int REQUEST_REJECTED_DIFF_IDENTS = 93;

    public static final int socksDefaultPort	= 1080;

    InetAddress address = null;
    int	port = -1;

    /** 
     * Creates a stream socket and connects it to the specified port 
     * number on the named host using SOCKS. 
     * <p>
     * If the application has specified a server socket factory, that 
     * factory's <code>createSocketImpl</code> method is called to create 
     * the actual socket implementation. Otherwise a "plain" socket is created.
     *
     * @param      host   the host name.
     * @param      port   the port number.
     * @param      socksAddress  the host name of the SOCKS server.
     * @param      socksPort    the port number of the SOCKS server.
     * @exception  IOException  if an I/O error occurs when creating the socket.
     * @since      JDK1.0
     */
    public SocksSocket(String host, int port, String socksAddress, int socksPort)
	throws UnknownHostException, IOException
    {
	this(InetAddress.getByName(host), port, InetAddress.getByName(socksAddress), socksPort);	
    }

    /** 
     * Creates a stream socket and connects it to the specified port 
     * number at the specified IP address using SOCKS. 
     * <p>
     * If the application has specified a socket factory, that factory's 
     * <code>createSocketImpl</code> method is called to create the 
     * actual socket implementation. Otherwise a "plain" socket is created.
     *
     * @param      address   the IP address.
     * @param      port      the port number.
     * @param      socksAddress  the IP address of the SOCKS server.
     * @param      socksPort    the port number of the SOCKS server.
     * @exception  IOException  if an I/O error occurs when creating the socket.
     * @since      JDK1.0
     */
    public SocksSocket(InetAddress address, int port, InetAddress socksAddress, int socksPort) throws IOException {
	connect(address, port, socksAddress, socksPort);
    }
  

    /** 
     * Creates a socket and connects it to the specified address on
     * the specified port using SOCKS v4.
     * @param address the address
     * @param port the specified port
     * @param socksAddress the address of the SOCKS proxy
     * @param socksPort the specified port of the SOCKS proxy
     */
    private void connect(InetAddress address, int port, InetAddress socksAddress, int socksPort) throws IOException {

	if (socksPort == -1)
	    socksPort = socksDefaultPort;

        this.address = address;
        this.port = port;

	if (socksAddress != null)
	{
	    socket = new Socket(socksAddress, socksPort);

	    /**
	     * Connect to the SOCKS server using the SOCKS connection protocol.
	     */
	    sendSOCKSCommandPacket(COMMAND_CONNECT, address, port);

	    int protoStatus = getSOCKSReply();

	    switch (protoStatus) {
	      case REQUEST_GRANTED:
		// connection set up, return control to the socket client
		return;

	      case REQUEST_REJECTED:
	      case REQUEST_REJECTED_NO_IDENTD:
		    throw new SocketException("SOCKS server cannot conect to identd");

	      case REQUEST_REJECTED_DIFF_IDENTS:
		throw new SocketException("User name does not match identd name");
	    }
	}
	else  
	{
	    socket = new Socket(address, port);
	}
    }


    /**
     * Read the response from the socks server.  Return the result code.
     */
    private int getSOCKSReply() throws IOException {
	InputStream in = getInputStream();
	byte response[] = new byte[8];
        int bytesReceived = 0;
        int len = response.length;

	for (int attempts = 0; bytesReceived<len &&  attempts<3; attempts++) {
	    int count = in.read(response, bytesReceived, len - bytesReceived);
	    if (count < 0)
		throw new SocketException("Malformed reply from SOCKS server");
	    bytesReceived += count;
	}

 	if (bytesReceived != len) {
 	    throw new SocketException("Reply from SOCKS server has bad length: " + bytesReceived);
  	}

	if (response[0] != 0) { // should be version0 
	    throw new SocketException("Reply from SOCKS server has bad version " + response[0]);
	}

	return response[1];	// the response code
    }

    /**
     * Just creates and sends out to the connected socket a SOCKS command
     * packet.
     */
    private void sendSOCKSCommandPacket(int command, InetAddress address,
					int port) throws IOException {
	
        byte commandPacket[] = makeCommandPacket(command, address, port);
	OutputStream out = getOutputStream();

	out.write(commandPacket);
    }

    /**
     * Create and return a SOCKS V4 command packet.
     */
    private byte[] makeCommandPacket(int command, InetAddress address,
					int port) { 

	// base packet size = 8, + 1 null byte 
	ByteArrayOutputStream byteStream = new ByteArrayOutputStream(8 + 1);

	byteStream.write(SOCKS_PROTO_VERS);
	byteStream.write(command);


	byteStream.write((port >> 8) & 0xff);
	byteStream.write((port >> 0) & 0xff);

	byte addressBytes[] = address.getAddress();
	byteStream.write(addressBytes, 0, addressBytes.length);

        String userName = (String) java.security.AccessController.doPrivileged(
               new sun.security.action.GetPropertyAction("user.name"));

	byte userNameBytes[] = new byte[userName.length()];
	userName.getBytes(0, userName.length(), userNameBytes, 0);

	byteStream.write(userNameBytes, 0, userNameBytes.length);
	byteStream.write(0);	// null termination for user name

	return byteStream.toByteArray();
    }


    /**
     * Returns the address to which the socket is connected.
     *
     * @return  the remote IP address to which this socket is connected.
     * @since   JDK1.0
     */
    public InetAddress getInetAddress() {
	return address;
    }

    /**
     * Gets the local address to which the socket is bound.
     *
     * @since   JDK1.1
     */
    public InetAddress getLocalAddress() {
	return socket.getLocalAddress();
    }

    /**
     * Returns the remote port to which this socket is connected.
     *
     * @return  the remote port number to which this socket is connected.
     * @since   JDK1.0
     */
    public int getPort() {
	return port;
    }

    /**
     * Returns the local port to which this socket is bound.
     *
     * @return  the local port number to which this socket is connected.
     * @since   JDK1.0
     */
    public int getLocalPort() {
	return socket.getLocalPort();
    }

    /**
     * Returns an input stream for this socket.
     *
     * @return     an input stream for reading bytes from this socket.
     * @exception  IOException  if an I/O error occurs when creating the
     *               input stream.
     * @since      JDK1.0
     */
    public InputStream getInputStream() throws IOException {
	return socket.getInputStream();
    }

    /**
     * Returns an output stream for this socket.
     *
     * @return     an output stream for writing bytes to this socket.
     * @exception  IOException  if an I/O error occurs when creating the
     *               output stream.
     * @since      JDK1.0
     */
    public OutputStream getOutputStream() throws IOException {
	return socket.getOutputStream();
    }

    /**
     * Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
     *
     * @since   JDK1.1
     */
    public void setTcpNoDelay(boolean on) throws SocketException {
	socket.setTcpNoDelay(on);
    }

    /**
     * Tests if TCP_NODELAY is enabled.
     *
     * @since   JDK1.1
     */
    public boolean getTcpNoDelay() throws SocketException {
	return socket.getTcpNoDelay();
    }

    /**
     * Enable/disable SO_LINGER with the specified linger time.  
     *
     * @since   JDK1.1
     */
    public void setSoLinger(boolean on, int val) throws SocketException {
	socket.setSoLinger(on, val);
    }

    /**
     * Returns setting for SO_LINGER. -1 returns implies that the
     * option is disabled.
     *
     * @since   JDK1.1
     */
    public int getSoLinger() throws SocketException {
	return socket.getSoLinger();
    }

    /**
     *  Enable/disable SO_TIMEOUT with the specified timeout, in
     *  milliseconds.  With this option set to a non-zero timeout,
     *  a read() call on the InputStream associated with this Socket
     *  will block for only this amount of time.  If the timeout expires,
     *  a <B>java.io.InterruptedIOException</B> is raised, though the
     *  Socket is still valid. The option <B>must</B> be enabled
     *  prior to entering the blocking operation to have effect. The 
     *  timeout must be > 0.
     *  A timeout of zero is interpreted as an infinite timeout.
     *
     * @since   JDK 1.1
     */
    public synchronized void setSoTimeout(int timeout) throws SocketException {
	socket.setSoTimeout(timeout);
    }

    /**
     * Returns setting for SO_TIMEOUT.  0 returns implies that the
     * option is disabled (i.e., timeout of infinity).
     *
     * @since   JDK1.1
     */
    public synchronized int getSoTimeout() throws SocketException {
	return socket.getSoTimeout();
    }

    /**
     * Closes this socket. 
     *
     * @exception  IOException  if an I/O error occurs when closing this socket.
     * @since      JDK1.0
     */
    public synchronized void close() throws IOException {
	socket.close();
    }

    /**
     * Converts this socket to a <code>String</code>.
     *
     * @return  a string representation of this socket.
     * @since   JDK1.0
     */
    public String toString() {
	return "SocksSocket[addr=" + getInetAddress() +
	    ",port=" + getPort() + 
	    ",localport=" + getLocalPort() + "]";
    }
}
