/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.api.corba;

public class ExecResultCorba
    implements com.netscape.jsdebugging.api.ExecResult
{
    ExecResultCorba( com.netscape.jsdebugging.remote.corba.IExecResult result )
    {
        _result = result;
    }

    public String   getResult()             {return _result.result;}
    public boolean  getErrorOccured()       {return _result.errorOccured;}
    public String   getErrorMessage()       {return _result.errorMessage;}
    public String   getErrorFilename()      {return _result.errorFilename;}
    public int      getErrorLineNumber()    {return _result.errorLineNumber;}
    public String   getErrorLineBuffer()    {return _result.errorLineBuffer;}
    public int      getErrorTokenOffset()   {return _result.errorTokenOffset;}

    public com.netscape.jsdebugging.api.Value getResultValue() 
                                        {return null;}    // XXX implement this

    private com.netscape.jsdebugging.remote.corba.IExecResult _result;
}    


