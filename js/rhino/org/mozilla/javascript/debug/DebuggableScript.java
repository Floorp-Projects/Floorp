/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

// API class

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;

import java.util.Enumeration;

/**
 * This interface exposes debugging information from executable 
 * code (either functions or top-level scripts).
 */
public interface DebuggableScript {
  
    /**
     * Returns true if this is a function, false if it is a script.
     */
    public boolean isFunction();
    
    /**
     * Get the Scriptable object (Function or Script) that is 
     * described by this DebuggableScript object.
     */
    public Scriptable getScriptable();

    /**
     * Get the name of the source (usually filename or URL)
     * of the script.
     */
    public String getSourceName();
    
    /**
     * Get an enumeration containing the line numbers that 
     * can have breakpoints placed on them.
     * XXX - array?
     */
    public Enumeration getLineNumbers();
    
    /**
     * Place a breakpoint at the given line.
     * @return true if the breakpoint was successfully set.
     */
    public boolean placeBreakpoint(int line);
    
    /**
     * Remove a breakpoint from the given line.
     * @return true if there was a breakpoint at the given line.
     */
    public boolean removeBreakpoint(int line);
}
