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
 * $Id: Handler.java,v 1.1 2001/05/09 17:30:00 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

/*
 * @(#)Handler.java	1.44 96/11/24
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
 * CopyrightVersion 1.3
 *
 * Java Plug-in
 *
 * @author Jerome Dochez
 */

/*-
 *	HTTP stream opener
 */

package sun.jvmp.jpav.protocol.jdk12.https;

import java.io.IOException;
import java.net.URL;

/** open an http input stream given a URL */
public class Handler extends sun.jvmp.jpav.protocol.jdk12.http.Handler {

    /*
     * <p>
     * Delegate to the Https connection 
     * </p>
     */
    public java.net.URLConnection openConnection(URL u) throws IOException {

	/* for now, SSL export is legal, so this extension can be presented */
      /*
	try {
	    // Try if SSL extension is installed.
	    Class sslSession = Class.forName("javax.net.ssl.SSLSession");
	    return new HttpsURLConnection(u, this);
	}
	catch (Throwable e)  {
	    // else use browser HTTPS support.
	    return new BrowserHttpsURLConnection(u);
	    } */
      return new BrowserHttpsURLConnection(u);
    }
}
