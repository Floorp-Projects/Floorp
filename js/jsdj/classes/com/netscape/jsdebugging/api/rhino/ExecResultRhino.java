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


