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

package com.netscape.jsdebugging.api.rhino;

public class ExecResultRhino
    implements com.netscape.jsdebugging.api.ExecResult
{
    ExecResultRhino( String str )
    {
        _str = str;
    }

    public String   getResult()             {return _str;}
    public boolean  getErrorOccured()       {return false;}
    public String   getErrorMessage()       {return null;}
    public String   getErrorFilename()      {return null;}
    public int      getErrorLineNumber()    {return 0;}
    public String   getErrorLineBuffer()    {return null;}
    public int      getErrorTokenOffset()   {return 0;}

    public com.netscape.jsdebugging.api.Value getResultValue() 
                                        {return null;}    // XXX implement this

    private String _str;
}    


