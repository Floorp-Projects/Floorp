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
package org.mozilla.pluglet;
import org.mozilla.pluglet.mozilla.*;
import java.awt.Panel;
import java.awt.print.PrinterJob;

public interface PlugletInstance {
    /**
     * Initializes a newly created pluglet instance, passing to it the pluglet
     * instance peer which it should use for all communication back to the browser.
     * 
     * @param peer the corresponding pluglet instance peer
     */
    void initialize(PlugletInstancePeer peer);
    /**
     * Called to instruct the pluglet instance to start. This will be called after
     * the pluglet is first created and initialized, and may be called after the
     * pluglet is stopped (via the Stop method) if the pluglet instance is returned
     * to in the browser window's history.
     */
    void start();
    /**
     * Called to instruct the pluglet instance to stop, thereby suspending its state.
     * This method will be called whenever the browser window goes on to display
     * another page and the page containing the pluglet goes into the window's history
     * list.
     */
    void stop();
    /**
     * Called to instruct the pluglet instance to destroy itself. This is called when
     * it become no longer possible to return to the pluglet instance, either because 
     * the browser window's history list of pages is being trimmed, or because the
     * window containing this page in the history is being closed.
     */
    void destroy();
    /**
     * Called to tell the pluglet that the initial src/data stream is
     * ready. 
     * 
     * @result PlugletStreamListener
     */
    PlugletStreamListener newStream();
    /**
     * Called when the window containing the pluglet instance changes.
     *
     * @param frame the pluglet panel
     */
    void setWindow(Panel frame);
     /**
     * Called to instruct the pluglet instance to print itself to a printer.
     *
     * @param print printer information.
     */ 
    void print(PrinterJob printerJob);
}
    











