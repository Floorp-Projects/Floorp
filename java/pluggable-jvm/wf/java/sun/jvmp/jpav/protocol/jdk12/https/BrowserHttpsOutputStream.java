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
 * $Id: BrowserHttpsOutputStream.java,v 1.1 2001/05/10 18:12:32 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

/*
 * @(#)BrowserHttpsOutputStream.java	1.5 00/04/27
 * 
 * Copyright (c) 1996-1997 Sun Microsystems, Inc. All Rights Reserved.
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
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;

/**
 * HTTPS URL POST support. This class buffers all the output stream
 * until the stream is closed, then it will do a POST on the web
 * server through the browser API.
 *
 * @author Stanley Man-Kit Ho
 */
public
class BrowserHttpsOutputStream extends java.io.ByteArrayOutputStream
{
    // URLConnection of the HTTPS connection.
    BrowserHttpsURLConnection urlConnection = null;

    // Determine if the HTTPS output stream has been closed.
    private boolean bConnected = false;

    /**
     * <P> Create BrowserHttpsOutputStream object.
     * </P>
     *
     * @param url URL of the HTTPS connection.
     */
    public BrowserHttpsOutputStream(BrowserHttpsURLConnection urlConnection)
    {
	super();
	this.urlConnection = urlConnection;
	this.bConnected = true;
    }


    /**
     * <P> Create BrowserHttpsOutputStream object.
     * </P>
     *
     * @param url URL of the HTTPS connection.
     * @param size Size of the output stream buffer.
     */
    public BrowserHttpsOutputStream(BrowserHttpsURLConnection urlConnection, int size)
    {
	super(size);
	this.urlConnection = urlConnection;
	this.bConnected = true;
    }

    /**
     * <P> Closes this output stream.
     * </P>
     */
    public synchronized void close() throws IOException
    {
	// Make sure the stream has not been closed multiple times
	if (bConnected)
	{
	    bConnected = false;
	    super.close();

	    byte[] postResponse;
	    byte[] buf = toByteArray();

	    if (buf == null)
		postResponse = postHttpsURL(urlConnection, urlConnection.getURL().toString(), null, 0);
	    else
		postResponse = postHttpsURL(urlConnection, urlConnection.getURL().toString(), buf, buf.length);

	    if (urlConnection.getDoInput()) {
		urlConnection.postResponse = postResponse;
		urlConnection.postResponseReady = true;
	    }
	}
    }

    /**
     * <P> Native code to call back into the browser to do a POST.
     * </P>
     *
     * @param url URL of the HTTPS connection
     * @param buf Data to post.
     */
    private native byte[] postHttpsURL(URLConnection urlConnection, String url, byte[] buf, int length);
}
