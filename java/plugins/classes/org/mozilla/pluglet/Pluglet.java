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
package org.mozilla.pluglet;
import org.mozilla.pluglet.mozilla.*;
import java.awt.Frame;
import java.awt.print.PrinterJob;

/**
 * A <code>Pluglet</code> is a Plugin written in the Java programming language.
 * It is dispatched when a certain MIME type is encountered by a browser.
 * This interface includes functions to initialize, start, stop, destroy, 
 * and print an instance of <code>Pluglet</code>.
 */ 
public interface Pluglet {
    /**
     * Initializes a newly created <code>Pluglet</code> instance, passing to it an instance of
     * PlugletPeer, which it should use for communication with the browser.
     * <p>
     * @param peer This is the instance of <code>PlugletPeer</code> that should be used for
     * communication with the browser.
     */
    void initialize(PlugletPeer peer);
    /**
     * Called to instruct the <code>Pluglet</code> instance to start. This will be called after
     * the <code>Pluglet</code> is first created and initialized, and may be called after the
     * <code>Pluglet</code> is stopped (via the <code>stop()</code> method) if the 
     * <code>Pluglet</code> instance is revisited in the browser window's history. 
     */
    void start();
    /**
     * Called to instruct the <code>Pluglet</code> instance to stop and suspend its state.
     * This method will be called whenever the browser window displays another
     * page and the page containing the <code>Pluglet</code> goes into the browser's history
     * list.
     */
    void stop();
    /**
     * Called to instruct the <code>Pluglet</code> instance to destroy itself. This is called when
     * it is no longer possible to return to the <code>Pluglet</code> instance -- either because 
     * the browser window's history list of pages is being trimmed, or because the
     * window containing this page in the history is being closed.
     */
    void destroy();
    /**
     * This is called to tell the <code>Pluglet</code> instance that the stream data                     
     * for an SRC or DATA attribute (corresponding to an EMBED or OBJECT           
     * tag) is ready to be read; it is also called for a full-page Pluglet.
     * The <code>Pluglet</code> is expected to return an instance of  
     * <code>PlugletStreamListener</code>, to which data and notifications
     * will be sent.
     * <p>
     * @return <code>PlugletStreamListener</code> instance, the listener the browser will use to
     * give the <code>Pluglet</code> the data.
     */
    PlugletStreamListener newStream();
    /**
     * Called by the browser to set or change the frame containing the <code>Pluglet</code> 
     * instance.
     * <p>
     * @param frame the <code>Pluglet</code> instance frame that changes.
     */
    void setWindow(Frame frame);
     /**
     * Called to instruct the <code>Pluglet</code> instance to print itself to a printer.
     * <p>
     * @param printerJob This is an object of type <code>PrinterJob</code>. It is used to
     * control printing.
     */ 
    void print(PrinterJob printerJob);
}
    











