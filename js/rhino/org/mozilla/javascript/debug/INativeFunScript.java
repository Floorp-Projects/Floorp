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
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;

public interface INativeFunScript
{
    // public interface to the rest of the debugging system

    public int pc2lineno(int pc);
    public int lineno2pc(int lineno);
    public void setTrap(int pc);
    public void clearTrap(int pc);
    public void clearAllTraps();
    public void setInterrupt();
    public void clearInterrupt();

    // the rest are used almost exclusively by NativeDelegate

    public Context get_debug_creatingContext();
    public int     get_debug_stopFlags();
    public int[]   get_debug_trapMap();
    public String  get_debug_linenoMap();
    public int     get_debug_trapCount();
    public int     get_debug_baseLineno();
    public int     get_debug_endLineno();
    public IScript get_debug_script();

    public void    set_debug_creatingContext(Context cx);
    public void    set_debug_stopFlags(int flags);
    public void    set_debug_trapCount(int count);
    public void    set_debug_script(IScript script);
}    
