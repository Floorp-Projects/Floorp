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

public interface Pluglet {
    /**
     * Creates a new pluglet instance, based on a MIME type. This
     * allows different impelementations to be created depending on
     * the specified MIME type.
     */
    public PlugletInstance createPlugletInstance(String mimeType);
    /**
     * Initializes the pluglet and will be called before any new instances are
     * created.
     */
    public void initialize(PlugletManager manager);
    /**
     * Called when the browser is done with the pluglet factory, or when
     * the pluglet is disabled by the user.
     */
    public void shutdown();
}





