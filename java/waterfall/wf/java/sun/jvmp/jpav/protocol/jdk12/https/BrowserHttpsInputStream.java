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
 * $Id: BrowserHttpsInputStream.java,v 1.1 2001/05/09 17:30:00 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

/*
 * @(#)BrowserHttpsInputStream.java	1.2 00/04/27
 * 
 * Copyright (c) 1996-1999 Sun Microsystems, Inc. All Rights Reserved.
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
 * CopyrightVersion 1.0
 */

package sun.jvmp.jpav.protocol.jdk12.https;

import java.net.URLConnection;
import java.io.IOException;
import java.io.InputStream;

 /**
 * HTTPS URL input stream support.  The intention here is to allow input from the
 * server to be flushed prior to being closed.  The browser API is used for reading
 * https response input.
 *
 * @author R.H. Yang
 */
public
class BrowserHttpsInputStream extends java.io.InputStream
{
    // parent URLConnection

    BrowserHttpsURLConnection   urlConnection;
    
    // seek pointer

    long                        seekPos;

    // "cookie" for native connection (handle/ptr/whatever)

    long                        nativeConnection;

    // stream state

    boolean                     open;


    // Construct input stream attached to specified Https URL Connection

    public BrowserHttpsInputStream( BrowserHttpsURLConnection uc ) 
                                                    throws IOException {
        urlConnection = uc;
        openStream();
        open = true;
    }

    /**
     * Reads the next byte of data from the input stream. The value byte is
     * returned as an <code>int</code> in the range <code>0</code> to
     * <code>255</code>. If no byte is available because the end of the stream
     * has been reached, the value <code>-1</code> is returned. This method
     * blocks until input data is available, the end of the stream is detected,
     * or an exception is thrown.
     *
     * <p> A subclass must provide an implementation of this method.
     *
     * @return     the next byte of data, or <code>-1</code> if the end of the
     *             stream is reached.
     * @exception  IOException  if an I/O error occurs.
     */
    public synchronized int read() throws IOException {
        ensureOpen();

        if ( 0 == nativeConnection )
            return -1;

        
        // Read from native code

        int numRead = readStream( temp, 0, 1 );

        if ( 1 == numRead ) {
            ++seekPos;

            return temp[0] & 0xff;
        } else {
            return numRead;
        }
    }

    /**
     * Reads up to <code>len</code> bytes of data from the input stream into
     * an array of bytes.  An attempt is made to read as many as
     * <code>len</code> bytes, but a smaller number may be read, possibly
     * zero. The number of bytes actually read is returned as an integer.
     *
     * <p> This method blocks until input data is available, end of file is
     * detected, or an exception is thrown.
     *
     * <p> If <code>b</code> is <code>null</code>, a
     * <code>NullPointerException</code> is thrown.
     *
     * <p> If <code>off</code> is negative, or <code>len</code> is negative, or
     * <code>off+len</code> is greater than the length of the array
     * <code>b</code>, then an <code>IndexOutOfBoundsException</code> is
     * thrown.
     *
     * <p> If <code>len</code> is zero, then no bytes are read and
     * <code>0</code> is returned; otherwise, there is an attempt to read at
     * least one byte. If no byte is available because the stream is at end of
     * file, the value <code>-1</code> is returned; otherwise, at least one
     * byte is read and stored into <code>b</code>.
     *
     * <p> The first byte read is stored into element <code>b[off]</code>, the
     * next one into <code>b[off+1]</code>, and so on. The number of bytes read
     * is, at most, equal to <code>len</code>. Let <i>k</i> be the number of
     * bytes actually read; these bytes will be stored in elements
     * <code>b[off]</code> through <code>b[off+</code><i>k</i><code>-1]</code>,
     * leaving elements <code>b[off+</code><i>k</i><code>]</code> through
     * <code>b[off+len-1]</code> unaffected.
     *
     * <p> In every case, elements <code>b[0]</code> through
     * <code>b[off]</code> and elements <code>b[off+len]</code> through
     * <code>b[b.length-1]</code> are unaffected.
     *
     * <p> If the first byte cannot be read for any reason other than end of
     * file, then an <code>IOException</code> is thrown. In particular, an
     * <code>IOException</code> is thrown if the input stream has been closed.
     *
     * @param      b     the buffer into which the data is read.
     * @param      off   the start offset in array <code>b</code>
     *                   at which the data is written.
     * @param      len   the maximum number of bytes to read.
     * @return     the total number of bytes read into the buffer, or
     *             <code>-1</code> if there is no more data because the end of
     *             the stream has been reached.
     * @exception  IOException  if an I/O error occurs.
     * @see        java.io.InputStream#read()
     */
    public synchronized int read(byte b[], int off, int len) throws IOException {
        ensureOpen();

        // Check for boundary conditions

	if (b == null) {
	    throw new NullPointerException();
	} else if ((off < 0) || (off > b.length) || (len < 0) ||
		   ((off + len) > b.length) || ((off + len) < 0)) {
	    throw new IndexOutOfBoundsException();
	} else if (len == 0) {
	    return 0;
	}

        if ( 0 == nativeConnection )
            return -1;


        // Read from native code

        int numRead = readStream( b, off, len );
            
        if ( numRead > 0 ) {
            seekPos += numRead;
        }

        return ( numRead > 0 ? numRead : -1 );
    }

    /** 
     * Skips n bytes of input.
     * @param n the number of bytes to skip
     * @return	the actual number of bytes skipped.
     * @exception IOException If an I/O error has occurred.
     */
     // (Stolen from SocketInputStream)
    public long skip(long numbytes) throws IOException {
        synchronized( this ) {
            ensureOpen();
        }

	if (numbytes <= 0) {
	    return 0;
	}
	long n = numbytes;
	int buflen = (int) Math.min(1024, n);
	byte data[] = new byte[buflen];
	while (n > 0) {
	    int r = read(data, 0, (int) Math.min((long) buflen, n));
	    if (r < 0) {
		break;
	    }
	    n -= r;
	}

        synchronized( this ) {
            seekPos += ( numbytes - n );
        }
	return numbytes - n;
    }

    /**
     * Returns the number of bytes that can be read (or skipped over) from
     * this input stream without blocking by the next caller of a method for
     * this input stream.  The next caller might be the same thread or or
     * another thread.
     *
     * @return     the number of bytes that can be read from this input stream
     *             without blocking.
     * @exception  IOException  if an I/O error occurs.
     */
    public synchronized int available() throws IOException {
        ensureOpen();

        return ( 0 == nativeConnection ? 0 : bytesAvailable() );
    }

    /**
     * Closes this input stream and releases any system resources associated
     * with the stream.
     *
     * @exception  IOException  if an I/O error occurs.
     */
    public synchronized void close() throws IOException {
	if (this.open == true)
	{
	    closeStream();
	    this.open = false;
	}
    }

    /**
     * Check to make sure that this stream has not been closed
     */
    private void ensureOpen() throws IOException {
	if ( !this.open )
	    throw new IOException("Stream closed");
    }

    protected void finalize() {
        try {
	    close();
        } catch( IOException e ) {
        }
    }

    private byte temp[] = new byte[1];          // for single-byte reads


    //////////////////////////////////////////////////////////////////////////
    // native implementation

    private native void openStream()            throws IOException;

    private native int  readStream( byte[] buf, int offs, int len )
                                                throws IOException;

    private native int  bytesAvailable()        throws IOException;

    private native void closeStream()           throws IOException;
}

