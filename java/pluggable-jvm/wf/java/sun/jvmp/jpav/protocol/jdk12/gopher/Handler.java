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
 * $Id: Handler.java,v 1.1 2001/05/10 18:12:30 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav.protocol.jdk12.gopher;

import java.net.URL;
import java.io.IOException;
import sun.jvmp.jpav.protocol.jdk12.http.HttpURLConnection;
import sun.jvmp.applet.*;
import sun.jvmp.jpav.*;

/**
 * A class to handle the gopher protocol.
 */

public class Handler extends sun.net.www.protocol.gopher.Handler {

    public java.net.URLConnection openConnection(URL u) throws IOException {
	/* if set for proxy usage then go through the gopher code to get
	 * the url connection.
         */
	ProxyHandler handler = ActivatorProxyHandler.getDefaultProxyHandler();
        ProxyInfo pinfo = null;

        if (handler != null)
           pinfo = handler.getProxyInfo(u);

	try  {
	    if (pinfo != null && pinfo.isProxyUsed())
	    {
		return new HttpURLConnection(u, pinfo.getProxy(), pinfo.getPort());
	    }
	    else 
	    {
  		/* make a direct gopher connection */
		return super.openConnection(u);
	    }
        } catch(IOException e)  {
	    return super.openConnection(u);
	}
    }
}


