/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.jsdebug;

/**
* This subclass of PC provides JavaScript-specific information.
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
*/
public final class JSPC extends PC {
    private Script script;
    private int pc;

    public JSPC(Script script, int pc) {
        this.script = script;
        this.pc = pc;
    }

    public Script getScript() {
        return script;
    }

    public int getPC() {
        return pc;
    }

    public boolean isValid()
    {
        return script.isValid();
    }


    /**
     * Get the SourceLocation associated with this PC value.
     * returns null if the source location is unavailable.
     */
    public native SourceLocation getSourceLocation();

    /**
     * Ask whether two program counter values are equal.
     */
    public boolean equals(Object obj) {
        if (this == obj)
            return true;
        if (!(obj instanceof JSPC))
            return false;
        JSPC jspc = (JSPC) obj;
        return (jspc.script == script && jspc.pc == pc);
    }

    public int hashCode() {
        return script.hashCode() + pc;
    }

    public String toString() {
        return "<PC "+script+"+"+pc+">";
    }
}
