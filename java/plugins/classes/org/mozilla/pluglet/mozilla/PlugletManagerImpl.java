/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
import org.mozilla.pluglet.*;
import java.net.URL;
public class PlugletManagerImpl implements PlugletManager {
    private long peer = 0; 
    public PlugletManagerImpl(long _peer) {
	peer = _peer;
	nativeInitialize();
    }
    /**
     * Causes the pluglets directory to be searched again for new pluglet 
     * libraries.
     *
     * @param reloadPages indicates whether currently visible pages should 
     * also be reloaded
     */
    public native void reloadPluglets(boolean reloadPages);

    /**
     * Returns the user agent string for the browser. 
     *
     */
    public native String userAgent();
    /**
     * Fetches a URL.
     * @param pluglet the pluglet making the request. 
     *        If null, the URL is fetched in the background.
     * @param url the URL to fetch
     * @param target the target window into which to load the URL
     * @param notifyData when present, URLNotify is called passing the 
     *        notifyData back to the client.
     * @param altHost an IP-address string that will be used instead of 
     *        the host specified in the URL. This is used to
     * @param prevent DNS-spoofing attacks. Can be defaulted to null 
     *        meaning use the host in the URL.
     * @param referrer the referring URL (may be null)
     * @param forceJSEnabled forces JavaScript to be enabled for 
     *        'javascript:' URLs, even if the user currently has JavaScript 
     *        disabled (usually specify false)
     */
    public void getURL(Pluglet pluglet,
                       URL url, String target,
                       PlugletStreamListener streamListener,
                       String altHost, URL referrer,
                       boolean forceJSEnabled) {
        throw(new UnsupportedOperationException("PlagletManager.getURL not implemented yet"));
    }
    /**
     * Posts to a URL with post data and/or post headers.
     *
     * @param pluglet the pluglet making the request. If null, the URL
     *   is fetched in the background.
     * @param url the URL to fetch
     * @param target the target window into which to load the URL
     * @param postDataLength the length of postData (if non-null)
     * @param postData the data to POST. null specifies that there is not post
     *   data
     * @param isFile whether the postData specifies the name of a file to 
     *   post instead of data. The file will be deleted afterwards.
     * @param notifyData when present, URLNotify is called passing the 
     *   notifyData back to the client.
     * @param altHost n IP-address string that will be used instead of the 
     *   host specified in the URL. This is used to prevent DNS-spoofing 
     *   attacks. Can be defaulted to null meaning use the host in the URL.
     * @param referrer the referring URL (may be null)
     * @param forceJSEnabled forces JavaScript to be enabled for 'javascript:'
     *   URLs, even if the user currently has JavaScript disabled (usually 
     *   specify false) 
     * @param postHeadersLength the length of postHeaders (if non-null)
     * @param postHeaders the headers to POST. null specifies that there 
     *   are no post headers
     */

    public void postURL(Pluglet pluglet,
                        URL url,
                        int postDataLen, 
                        byte[] postData,
                        boolean isFile,
                        String target,
                        PlugletStreamListener streamListener,
                        String altHost, 
                        URL referrer,
                        boolean forceJSEnabled,
                        int postHeadersLength, 
                        byte[] postHeaders) {
        throw(new UnsupportedOperationException("PlagletManager.postURL not implemented yet"));
    }
    protected void finalize() {
        nativeFinalize();
    }
    private native void nativeFinalize();
    private native void nativeInitialize(); 
}






