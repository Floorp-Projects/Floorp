/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* MailToURLConnection.java
 *
 * Created on 09 August 2005, 11:21
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package grendel.URL.mailto;
import java.io.FilePermission;
import java.io.IOException;

import java.net.URL;
import java.net.URLConnection;

import java.security.Permission;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;


/**
 *
 * @author hash9
 */
public class MailToURLConnection extends URLConnection {
    
    Map<String, List<String>> requestProperties = new HashMap<String, List<String>>();
    
    
    /**
     * Constructs a URL connection to the specified URL. A connection to
     * the object referenced by the URL is not created.
     *
     * @param   url   the specified URL.
     */
    protected MailToURLConnection(URL url) {
        super(url);
    }
    
    /**
     *
     * Returns a permission object representing the permission
     * necessary to make the connection represented by this
     * object. This method returns null if no permission is
     * required to make the connection. By default, this method
     * returns <code>java.security.AllPermission</code>. Subclasses
     * should override this method and return the permission
     * that best represents the permission required to make a
     * a connection to the URL. For example, a <code>URLConnection</code>
     * representing a <code>file:</code> URL would return a
     * <code>java.io.FilePermission</code> object.
     *
     * <p>The permission returned may dependent upon the state of the
     * connection. For example, the permission before connecting may be
     * different from that after connecting. For example, an HTTP
     * sever, say foo.com, may redirect the connection to a different
     * host, say bar.com. Before connecting the permission returned by
     * the connection will represent the permission needed to connect
     * to foo.com, while the permission returned after connecting will
     * be to bar.com.
     *
     * <p>Permissions are generally used for two purposes: to protect
     * caches of objects obtained through URLConnections, and to check
     * the right of a recipient to learn about a particular URL. In
     * the first case, the permission should be obtained
     * <em>after</em> the object has been obtained. For example, in an
     * HTTP connection, this will represent the permission to connect
     * to the host from which the data was ultimately fetched. In the
     * second case, the permission should be obtained and tested
     * <em>before</em> connecting.
     *
     *
     * @return the permission object representing the permission
     * necessary to make the connection represented by this
     * URLConnection.
     * @exception IOException if the computation of the permission
     * requires network or file I/O and an exception occurs while
     * computing it.
     */
    public Permission getPermission() throws IOException {
        return new FilePermission(toString(), "write");
    }
    
    /**
     * Opens a communications link to the resource referenced by this
     * URL, if such a connection has not already been established.
     * <p>
     * If the <code>connect</code> method is called when the connection
     * has already been opened (indicated by the <code>connected</code>
     * field having the value <code>true</code>), the call is ignored.
     * <p>
     * URLConnection objects go through two phases: first they are
     * created, then they are connected.  After being created, and
     * before being connected, various options can be specified
     * (e.g., doInput and UseCaches).  After connecting, it is an
     * error to try to set them.  Operations that depend on being
     * connected, like getContentLength, will implicitly perform the
     * connection, if necessary.
     *
     *
     * @exception IOException  if an I/O error occurs while opening the
     *               connection.
     * @see java.net.URLConnection#connected
     * @see #getConnectTimeout()
     * @see #setConnectTimeout(int)
     * @throws SocketTimeoutException if the timeout expires before
     *               the connection can be established
     */
    public void connect() throws IOException {
        connected = true;
        // Open the compose window.
    }
    
    /**
     * Returns the value of the named general request property for this
     * connection.
     *
     *
     * @param key the keyword by which the request is known (e.g., "accept").
     * @return the value of the named general request property for this
     *           connection. If key is null, then null is returned.
     * @see #setRequestProperty(java.lang.String, java.lang.String)
     * @throws IllegalStateException if already connected
     */
    public String getRequestProperty(String key) {
        if (connected)
            throw new IllegalStateException("Already connected");
        List<String> l = requestProperties.get(key);
        if ((l== null)||l.size()==0) {
            return null;
        } 
        return l.get(l.size()-1);
    }
    
    /**
     * Returns an unmodifiable Map of general request
     * properties for this connection. The Map keys
     * are Strings that represent the request-header
     * field names. Each Map value is a unmodifiable List
     * of Strings that represents the corresponding
     * field values.
     *
     *
     * @return a Map of the general request properties for this connection.
     * @since 1.4
     * @throws IllegalStateException if already connected
     */
    public java.util.Map<String, java.util.List<String>> getRequestProperties() {
        if (connected)
            throw new IllegalStateException("Already connected");
        return requestProperties;
    }
    
    /**
     * Sets the general request property. If a property with the key already
     * exists, overwrite its value with the new value.
     *
     * <p> NOTE: HTTP requires all request properties which can
     * legally have multiple instances with the same key
     * to use a comma-seperated list syntax which enables multiple
     * properties to be appended into a single property.
     *
     *
     * @param key     the keyword by which the request is known
     *                  (e.g., "<code>accept</code>").
     * @param value   the value associated with it.
     * @see #getRequestProperty(java.lang.String)
     * @throws IllegalStateException if already connected
     * @throws NullPointerException if key is <CODE>null</CODE>
     */
    public void setRequestProperty(String key, String value) {
        if (connected)
            throw new IllegalStateException("Already connected");
        List<String> l = requestProperties.get(key);
        if (l==null) {
            l = new ArrayList<String>();
            requestProperties.put(key,l);
        } 
        l.add(value);
    }
}
