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
import java.io.OutputStream;

/**
 * The <code>PlugletPeer</code> interface is the set of functions implemented
 * by the browser to support a <code>Pluglet</code> instance. When a <code>Pluglet</code>
 * instance is constructed, a <code>PlugeletPeer</code> object is passed to its initializer. 
 * The peer object represents the instantiation of the <code>Pluglet</code> 
 * instance on the page.
 */
public interface PlugletPeer {
    /**
     * This is a <code>static final</code> integer variable set to 3.
     */
    public static final int NETSCAPE_WINDOW = 3;
    /**
     * Returns the MIME type of the <code>Pluglet</code> instance.
     * <p>
     * @return Returns a <code>String</code> for the MIME type of the <code>Pluglet</code> 
     * instance. 
     */
    public String getMIMEType();
    /**
     * Returns an <code>int</code> (integer value) indicating whether the 
     * Pluglet is embedded in HTML in the page via an <code>OBJECT</code> 
     * or <code>EMBED</code> element and is part of the page,
     * or whether the Pluglet is in a full page of its own.<p>
     * A full-page Pluglet can occur when a file of the Pluglet MIME type
     * is entered in the Address/URL field of the browser; when JavaScript
     * sets the document URL (<code>document.URL</code>) to that file; 
     * or when an applet redirects the browser to the file 
     * (via <code>java.net.HttpURLConnection</code>). 
     * <p>
     * @return Returns an <code>int</code> (integer value) representing the mode.
     * A value of 1 indicates the Pluglet is embedded in a page; a value of 2 
     * indicates it is in a full page of its own.
     */
    public int getMode();
    /**
     * Returns the value of a variable associated with the
     * <code>PlugletManager</code> instance.<p>
     * @param variable This is the <code>PlugletManager</code> instance 
     * variable to get.<p>
     * @return Returns a <code>String</code> representing the value of the variable.
     */
     public String getValue(int variable);
    /**
     * This method is called by the <code>Pluglet</code> instance when it wishes 
     * to send a stream of data to the browser. It constructs a
     * new output stream to which the <code>Pluglet</code> instance may send data. 
     * <p>
     * @param type The MIME type of the stream to create.<p>
     * @param target The name of the target window to receive the data.<p>
     * @return Returns the resulting output stream.
     */
    public OutputStream newStream(String type, String target);
    /** Invoking this method causes status information to be displayed 
     * at the bottom of the window associated with the <code>Pluglet</code> instance.
     * <p>
     * @param message This is the status message to display.
     */ 
    public void showStatus(String message);
    /**
     * Sets the desired size of the window associated with the 
     * <code>Pluglet</code> instance.<p>
     * @param width The width of the new window.
     * @param height The height of the new window.
     */
    public void setWindowSize(int width, int height);
    /**
     * For the <code>Pluglet</code> instance, returns the tag information 
     * associated with it. This is an object of type
     * <code>PlugletTagInfo</code>, which contains all the name-value 
     * pairs for the attributes of the tag/element.<p>
     * @return Gets the <code>Pluglet</code> instance tag information.
     */
    public PlugletTagInfo getTagInfo();
}
