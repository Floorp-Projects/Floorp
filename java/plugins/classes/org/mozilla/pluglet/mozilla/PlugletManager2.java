/* 
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.mozilla.pluglet.mozilla;
import java.net.URL;

/**
 * This interface extends the functionality of the <code>PlugletManager</code> interface,
 * including methods to begin and end a wait cursor, determine if a URL protocol is 
 * supported, and get proxy information for a URL.
 */
public interface PlugletManager2 extends PlugletManager {
    /**
     * Puts up a wait cursor.
     */
    public void beginWaitCursor();
    /**
     * Restores the previous (non-wait) cursor.
     */
    public void endWaitCursor();
    /**
     * Returns <code>true</code> if a URL protocol (e.g., <code>http</code>) is supported.
     *<p>
     * @param protocol This is the protocol name.
     * <p>
     * @return <code>Boolean</code> returns <code>true</code> if the URL protocol is supported.
     */
    public boolean isURLProtocolSupported(String protocol);
    /**
     * For a given URL, this method returns a <code>String</code> 
     * for the proxy information. 
     * The result will be in the following format:
     * <p>
     * <ul>
     *   <li> <code>DIRECT</code>  -- means no proxy required<br>
     *   <li> <code>PROXY xxx.xxx.xxx.xxx</code> -- use proxy 
     * (where <code>xxx.xxx.xxx.xxx</code> is the IP Address)<br>
     *   <li> <code>SOCKS xxx.xxx.xxx.xxx</code> -- use SOCKS 
     * (where <code>xxx.xxx.xxx.xxx</code> is the IP Address)<br>
     *   <li> Mixed. e.g., <code>PROXY 111.111.111.111;PROXY 112.112.112.112;
     *                           PROXY 111.111.111.111;SOCKS 112.112.112.112</code> ....
     * </ul>
     * <p>
     * Which proxy/SOCKS to use is determined by the <code>Pluglet</code>.<p>
     * @param url This is the URL for which proxy information is desired. <p>
     * @return Information (<code>String</code>) about the proxy. See above for the format. 

     */
    public String findProxyForURL(URL url);
}




