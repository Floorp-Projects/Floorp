/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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
