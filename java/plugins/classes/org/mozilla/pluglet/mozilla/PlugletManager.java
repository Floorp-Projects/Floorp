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
import org.mozilla.pluglet.*;
import java.net.URL;

/**
 * The <code>PlugletManager</code> interface includes functionality to get and post
 * URLs and return <code>userAgent</code> for the browser. It also includes a function 
 * for reloading all Pluglets in the Pluglets 
 * directory, allowing Pluglets to be installed and run without restarting the browser. 
 */
public interface  PlugletManager {   
    /**
     * This method reloads all Pluglets in the Pluglets directory. 
     * The browser knows about all installed Pluglets (and Plugins) 
     * at startup. But if the user adds or removes any Pluglets 
     * (or Plugins), the browser does not see them until it is restarted. 
     * This method lets the user install a new Pluglet and load it, 
     * or remove one, without having to restart the browser.
     * <p>
     * @param reloadPages <code>Boolean</code> value indicates whether currently visible
     * pages should also be reloaded.
     */
    public void reloadPluglets(boolean reloadPages);
    /**
     * Returns the <code>userAgent String</code> for the browser.
     * <code>userAgent</code> is a property of the <code>navigator</code>
     * object and contains information about the browser. 
     * <p>
     * @return Returns a <code>String</code> for the <code>userAgent</code>
     * for the browser.
     */
    public String userAgent();
    /**
     * Fetches a URL.   
     * <p>
     * @param pluglet This is the <code>Pluglet</code> instance making the request. 
     * If <code>null</code>, the URL is fetched in the background.
     * <p>
     * @param url This is the URL to fetch.
     * <p>
     * @param target This is the target window into which to load the URL.
     * <p>
     * @param streamListener This is an instance of <code>PlugletStreamListener</code>.
     * <p>
     * @param altHost This is an IP-address string that will be used instead of 
     * the host specified in the URL. This is used to prevent DNS-spoofing
     * attacks. It can be defaulted to <code>null</code>, which will mean: use the host
     * in the URL.
     * <p>
     * @param referrer This is the referring URL. (It may be <code>null</code>).
     * <p>
     * @param forceJSEnabled This will force JavaScript to be enabled for 
     * <code>javascript:</code> URLs, even if the user currently has JavaScript 
     * disabled. (Usually this should be set <code>false</code>.)
     */
    public void getURL(Pluglet pluglet,
                       URL url, String target,
                       PlugletStreamListener streamListener,
                       String altHost, URL referrer,
                       boolean forceJSEnabled);
    /**
     * Posts to a URL with post data and/or post headers.<p>
     * @param pluglet This is the <code>Pluglet</code> instance making the request. 
     * If <code>null</code>, the URL is fetched in the background.<p>
     * @param url This is the URL to fetch.<p>
     * @param postDataLen This is the length of <code>postData</code>
     * (if not <code>null</code>).<p>
     * @param postData This is the data to post. <code>null</code> specifies that there 
     * is no post data.<p>
     * @param isFile This indicates whether <code>postData</code> specifies the name 
     * of a file to post rather than data. The file will be deleted afterwards.<p>
     * @param target This is the target window into which to load the URL.<p>
     * @param streamListener This is an instance of <code>PlugletStreamListner</code>.<p>
     * @param altHost This is an IP-address string that will be used instead 
     * of the host specified in the URL. This is used to prevent DNS-spoofing 
     * attacks. It can be defaulted to <code>null</code>, which will mean: use the host 
     * in the URL.<p>
     * @param referrer This is the referring URL. (It may be <code>null</code>.)<p>
     * @param forceJSEnabled This will force JavaScript to be enabled for <code>javascript:</code>
     * URLs, even if the user currently has JavaScript disabled (usually 
     * specify false).<p> 
     * @param postHeadersLength This is the length of <code>postHeaders</code> 
     * (if not <code>null</code>).<p>
     * @param postHeaders These are the headers to POST. <code>null</code> specifies that there 
     * are no post headers.
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
            byte[] postHeaders);
}


