/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.api.corba;

import com.netscape.jsdebugging.api.*;

public class JSSourceLocationCorba implements JSSourceLocation
{
    public JSSourceLocationCorba( JSPCCorba pc, 
                                  com.netscape.jsdebugging.remote.corba.IJSSourceLocation sl )
    {
        _pc = pc;
        _sl = sl;
    }
    public int getLine()        {return _sl.line;}
    public String getURL()      {return _pc.getScript().getURL();}
    public PC getPC()           {return _pc;}
    public String toString()    
    {
        return "<JSSourceLocationCorba at "+getURL()+" line "+getLine()+">";
    }

    public boolean equals(Object obj)
    {
        try
        {
            JSSourceLocationCorba other = (JSSourceLocationCorba) obj;
            return getLine() == other.getLine() &&
                   getURL().equals(other.getURL());
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }
    public int hashCode() {return getURL().hashCode() + 17*getLine();}

    private JSPCCorba                         _pc;
    private com.netscape.jsdebugging.remote.corba.IJSSourceLocation _sl;
}
