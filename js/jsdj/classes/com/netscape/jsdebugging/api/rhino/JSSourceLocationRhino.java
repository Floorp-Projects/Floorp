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

package com.netscape.jsdebugging.api.rhino;

import com.netscape.jsdebugging.api.*;

public class JSSourceLocationRhino implements JSSourceLocation
{
    public JSSourceLocationRhino( JSPCRhino pc, 
                                  com.netscape.javascript.debug.ISourceLocation sl )
    {
        _pc = pc;
        _sl = sl;
    }
    public int getLine()        {return _sl.getLine();}
    public String getURL()      {return _sl.getURL();}
    public PC getPC()           {return _pc;}
    public String toString()    {return _sl.toString();}

    public boolean equals(Object obj)
    {
        try
        {
            JSSourceLocationRhino other = (JSSourceLocationRhino) obj;
            return getLine() == other.getLine() &&
                   getURL().equals(other.getURL());
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }
    public int hashCode() {return getURL().hashCode() + 17*getLine();}

    private JSPCRhino                         _pc;
    private com.netscape.javascript.debug.ISourceLocation _sl;
}
