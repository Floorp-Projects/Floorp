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


public interface PlugletInstancePeer {
    public static final int NETSCAPE_WINDOW = 3;
    /**
     * Returns the MIME type of the pluglet instance. 
     */
    public String getMIMEType();
    /**
     * Returns the mode of the pluglet instance, i.e. whether the pluglet 
     * is embedded in the html, or full page.
     */
    public int getMode();
    /**
     * Returns the value of a variable associated with the pluglet manager.
     * @param variable the pluglet manager variable to get
     */
     public String getValue(int variable);
    /**
     * This operation is called by the pluglet instance when it wishes to send a stream of data to the browser. It constructs a
     * new output stream to which the pluglet may send the data. When complete, the Close and Release methods should be
     * called on the output stream.
     * @param type type MIME type of the stream to create
     * @param target the target window name to receive the data
     */
    public OutputStream newStream(String type, String target);
    /** This operation causes status information to be displayed on the window associated with the pluglet instance.
     * @param message the status message to display
     */ 
    public void showStatus(String message);
}