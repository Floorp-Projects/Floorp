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
 * $Id: BrowserHttpsURLConnection.java,v 1.1 2001/05/09 17:30:00 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav.protocol.jdk12.https;

import java.net.*;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import sun.net.www.http.*;
import sun.net.www.MessageHeader;
import sun.net.www.protocol.http.*;
import java.util.Date;
import java.text.SimpleDateFormat;
import java.util.TimeZone;
import java.io.FileNotFoundException;
import java.io.BufferedInputStream;
import java.security.*;
import java.security.Permission;
import java.net.SocketPermission;


public
class BrowserHttpsURLConnection extends java.net.URLConnection
{
     /*
     * Message Headers hold the request and response headers send along
     * this HTTP request. This is currently available only in all IE version
     */
    private MessageHeader    responseHeaders = null;
    private MessageHeader    requestHeaders = null;

    /*
     * Native object reference 
     */
    private long nativeConnection = 0;

    /*
     * Initialize an HTTPS URLConnection ... could check that the URL
     * is an "https" URL, and that the handler is also an HTTPS one,
     * but that's established by other code in this package.
     */

    public Permission getPermission() throws IOException {
	int port = url.getPort();
	port = port < 0 ? 80 : port;
	String host = url.getHost() + ":" + port;
	Permission permission = new SocketPermission(host, "connect");
	return permission;
    }

    /*
     * Initialize an HTTPS URLConnection ... could check that the URL
     * is an "https" URL, and that the handler is also an HTTPS one,
     * but that's established by other code in this package.
     */
    public BrowserHttpsURLConnection (URL url) throws IOException {
	super (url);
    }
     /**
     * Implements the HTTP protocol handler's "connect" method,
     * establishing an SSL connection to the server as necessary.
     */
    public synchronized void connect () throws IOException {
        if (connected)
            return;

        // Check whether the applet is allowed to connect to the host.
        // This is necessary because we will enable privilege to 
        // connect to the proxy
        SecurityManager m = System.getSecurityManager();
        if (m != null)  {
            m.checkConnect(url.getHost(), url.getPort());
        }
        
        // Add the necessary headers if necessary
        java.security.AccessController.doPrivileged(new PrivilegedAction() {
                public Object run() {
                    packageRequestHeaders();
                    return null;
                }
            });
	connected = true;
    }
    
        /*
     * @return the raw request headers for this URL connection
     * 
     */
    private byte[] getRequestHeaders() {

        // Send the request headers
        byte[] rawRequestHeaders = null;
        if (requestHeaders!=null) {
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            PrintStream ps = new PrintStream(bos);
            requestHeaders.print(ps);
            ps.flush();
            rawRequestHeaders = bos.toByteArray();
        }
        return rawRequestHeaders;
    }

     /*
     * <p>
     * Sets the response headers from this URLConnection
     * </p>
     *
     * @param rawResponseHeaders Response headers 
     */
    private void setResponseHeaders(byte[] rawResponseHeaders) {

        // Extract the response headers
        if (rawResponseHeaders!=null) {
            ByteArrayInputStream bis = 
		new ByteArrayInputStream(rawResponseHeaders);
            try {
                responseHeaders = new MessageHeader(bis);
            } catch(IOException e) {
                e.printStackTrace();
            }
        }             
    }

    /**
     * <p>
     * Package the necessary request headers for this connection
     * </p>
     * 
     * @param mh MessageHeader that will contain all the request headers
     *
     */
    protected void packageRequestHeaders() {

        // Set the if modified since flag
        if (getIfModifiedSince()!=0) {
            TimeZone tz = TimeZone.getTimeZone("GMT");
            Date modTime = new Date(getIfModifiedSince());
            SimpleDateFormat df = new SimpleDateFormat();
            df.setTimeZone(tz);       
            setRequestProperty("If-Modified-Since", df.format(modTime));
        }                  
        setRequestPropertyIfNotSet("Content-Type", "application/x-www-form-urlencoded");
    }

      /**
     * Returns the value of the specified header field. Names of  
     * header fields to pass to this method can be obtained from 
     * getHeaderFieldKey.
     *
     * @param   name   the name of a header field. 
     * @return  the value of the named header field, or <code>null</code>
     *          if there is no such field in the header.
     * @see     java.net.URLConnection#getHeaderFieldKey(int)
     * @since   JDK1.0
     */
    public String getHeaderField(String name) {
        if (responseHeaders!=null) 
            return responseHeaders.findValue(name);
        return null;
    }

     /**
     * Returns the key for the <code>n</code><sup>th</sup> header field.
     *
     * @param   n   an index.
     * @return  the key for the <code>n</code><sup>th</sup> header field,
     *          or <code>null</code> if there are fewer than <code>n</code>
     *          fields.
     * @since   JDK1.0
     */
    public String getHeaderFieldKey(int n) {
        if (responseHeaders!=null) 
            return responseHeaders.getKey(n);
        return null;
    }

    /**
     * Returns the value for the <code>n</code><sup>th</sup> header field. 
     * It returns <code>null</code> if there are fewer than 
     * <code>n</code> fields. 
     * <p>
     * This method can be used in conjunction with the 
     * <code>getHeaderFieldKey</code> method to iterate through all 
     * the headers in the message. 
          *
     * @param   n   an index.
     * @return  the value of the <code>n</code><sup>th</sup> header field.
     * @see     java.net.URLConnection#getHeaderFieldKey(int)
     * @since   JDK1.0
     */
    public String getHeaderField(int n) {
        if (responseHeaders!=null) 
            return responseHeaders.getValue(n);
        return null;
    }

    /**
     * Sets the general request property. 
     *
     * @param   key     the keyword by which the request is known
     *                  (e.g., "<code>accept</code>").
     * @param   value   the value associated with it.
     */
    public synchronized void setRequestProperty(String key, String value) {
        if (connected)
            throw new IllegalAccessError("Already connected");
	        // Create the requests holder if necessary
        if (requestHeaders==null) {
            requestHeaders= new MessageHeader();
        }
        requestHeaders.set(key, value); 
    }

    public synchronized void setRequestPropertyIfNotSet(String key, String value
) {
        if (connected)
            throw new IllegalAccessError("Already connected");

        // Create the requests holder if necessary
        if (requestHeaders==null) {
            requestHeaders= new MessageHeader();
        }
        requestHeaders.setIfNotSet(key, value); 
    }

    /**
     * Returns the value of the named general request property for this
     * connection.
     *
     * @return  the value of the named general request property for this
     *           connection.
     */
    public String getRequestProperty(String key) {
        if (connected)
            throw new IllegalAccessError("Already connected");

        if (requestHeaders!=null)
            return requestHeaders.findValue(key); 
        return null;
	    }

    /**
     * Returns an input stream that reads from this open connection.
     *
     * @return     an input stream that reads from this open connection.
     * @exception  IOException              if an I/O error occurs while
     *               creating the input stream.
     * @exception  UnknownServiceException  if the protocol does not support
     *               input.
     * @since   JDK1.0
     */
    public InputStream getInputStream() throws IOException
    {
        if (!doInput)
            throw new ProtocolException("Cannot read from URLConnection"
                   + " if doInput=false (call setDoInput(true))");

        if (!connected)
            connect();

        if (ins == null)
        {
            byte buffer[] = null;

            // If we are in input/output mode, we might just want the response 
            // from a POST.
            if (doOutput) {
                if (!postResponseReady) 
                    throw new UnknownServiceException("Input from HTTPS not expected until OutputStream is closed");

                buffer = postResponse;
                postResponse=null;

                // If null comes back then probably the URL wasn't found.

                if ( null == buffer )
                    throw new FileNotFoundException( getURL().toString() );

                ins = new ByteArrayInputStream( buffer );
            } else {
                ins = new BufferedInputStream( new BrowserHttpsInputStream( this
 ) );
            }

	                // Check if we have response headers, if we don't we are going to
            // enter some heuristic values for the headers we can
            if (responseHeaders == null)
                responseHeaders = new MessageHeader();

            String contentType= getHeaderField("Content-Type");

            if (contentType==null) {
                try {
                    contentType = guessContentTypeFromStream(ins);
                } catch(java.io.IOException e) {
                }
                if (contentType == null) {
                    if (url.getFile().endsWith("/")) {
                        contentType = "text/html";
                    } else {
                        contentType = guessContentTypeFromName(url.getFile());
                    }
                    if (contentType == null) 
                        contentType = "content/unknown";
                }
                responseHeaders.add("Content-Type", contentType);
            }

            if ( null != buffer )
                responseHeaders.setIfNotSet("Content-Length",String.valueOf(buffer.length));
        }

        return ins;
    }

    InputStream ins = null;

    OutputStream outs = null;

    /**
     * Returns an output stream that writes to this connection.
     *
     * @return     an output stream that writes to this connection.
     * @exception  IOException              if an I/O error occurs while
     *               creating the output stream.
     * @exception  UnknownServiceException  if the protocol does not support
     *               output.
     * @since   JDK1.0
     */
        public OutputStream getOutputStream() throws IOException {

        if (!doOutput)
            throw new ProtocolException("cannot write to a URLConnection"
                               + " if doOutput=false - call setDoOutput(true)");

        if (ins!=null)
            throw new ProtocolException("Cannot write output after reading input.");

        if (!connected)
            connect();

        if (outs == null) 
            outs = new BrowserHttpsOutputStream(this);

        return outs;
    }


    byte[] postResponse = null;
    boolean postResponseReady = false;
}
