/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
package org.mozilla.pluglet.mozilla;
import java.net.URL;
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
     * Returns true if a URL protocol (e.g. "http") is supported.
     *
     * @param protocol the protocol name
     */
    public boolean supportsURLProtocol(String protocol);
    /**
     * Returns the proxy info for a given URL. The result will be in the
     * following format
     * 
     *   i)   "DIRECT"  -- no proxy
     *   ii)  "PROXY xxx.xxx.xxx.xxx"   -- use proxy
     *   iii) "SOCKS xxx.xxx.xxx.xxx"  -- use SOCKS
     *   iv)  Mixed. e.g. "PROXY 111.111.111.111;PROXY 112.112.112.112",
     *                    "PROXY 111.111.111.111;SOCKS 112.112.112.112"....
     *
     * Which proxy/SOCKS to use is determined by the plugin.
     */
    public String findProxyForURL(URL url);
}




