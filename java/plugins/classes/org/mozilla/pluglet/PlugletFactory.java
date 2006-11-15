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

    /**
     * This interface includes the functions to create an instance of <code>Pluglet</code>,   
     * and initialize the <code>PlugletFactory</code> instance and shut it down when 
     * no longer required.
     */

public interface PlugletFactory {
    /**
     * Creates a new <code>Pluglet</code> instance based on a MIME type. This
     * allows different implementations to be created depending on
     * the specified MIME type.<p>
     * While normally there will be only one <code>PlugletFactory</code> 
     * implementation and one instance of it, 
     * there can be multiple implementations of <code>Pluglet</code> and instances of them. 
     * Given a MIME type, it is the responsibility of 
     * the <code>createPluglet</code> method to create an instance 
     * of the  implementation of <code>Pluglet</code> 
     * for that MIME type. (Note: A single implementation of the <code>Pluglet</code> 
     * interface could handle more than one MIME type; there may also be 
     * separate implementations of the <code>Pluglet</code> interface for MIME types. 
     * This is up to the developer implementing the <code>Pluglet</code> interface.)
     * <p> 
     * @param <code>mimeType</code> This is the MIME type for which a new <code>Pluglet</code>
     * instance is to be created.
     * @return Returns a new <code>Pluglet</code> instance based on the specified
     * MIME type passed to the method.
     */
    public Pluglet createPluglet(String mimeType);
    /**
     * Initializes the <code>PlugletFactory</code> instance and is called before any new 
     * <code>Pluglet</code> instances are created.<p>
     * @param manager This is an instance of <code>PlugletManager</code> that is passed
     * to this method.
     */
    public void initialize(String plugletJarFileName, PlugletManager manager);
    /**
     * Called when the browser is done with a <code>PlugletFactory</code> instance. Normally
     * there is only one <code>PlugletFactory</code> instance. 
     */
    public void shutdown();
}





