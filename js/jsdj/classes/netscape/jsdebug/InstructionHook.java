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
* InstructionHook must be subclassed to respond to hooks
* at a particular program instruction.
*
* @author  John Bandhauer
* @author  Nick Thompson
* @version 1.0
* @since   1.0
*/
public class InstructionHook extends Hook {
    private PC pc;

    /**
     * Create an InstructionHook at the given PC value.
     */
    public InstructionHook(PC pc) {
        this.pc = pc;
    }

    /**
     * Get the instruction that the hook is set at.
     */
    public PC getPC() {
        return pc;
    }

    /**
     * Override this method to respond just before a thread
     * reaches a particular instruction.
     */
    /* jband - 03/31/97 - I made this public to allow chaining */
    public void aboutToExecute(ThreadStateBase debug) {
        // defaults to no action
    }
}
