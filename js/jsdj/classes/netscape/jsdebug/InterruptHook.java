/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package netscape.jsdebug;

/**
* InterruptHook must be subclassed to respond when interrupts occur
*
* @author  John Bandhauer
* @author  Nick Thompson
* @version 1.0
* @since   1.0
*/
public class InterruptHook extends Hook {

    /**
     * Override this method to respond when interrupts occur
     * @param debug the state of this thread
     * @param pc the pc of the instruction about to execute
     */
    public void aboutToExecute(ThreadStateBase debug, PC pc) {
        // defaults to no action
    }
}
