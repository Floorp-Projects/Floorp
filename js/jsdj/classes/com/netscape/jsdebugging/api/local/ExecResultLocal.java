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

package com.netscape.jsdebugging.api.local;

public class ExecResultLocal
    implements com.netscape.jsdebugging.api.ExecResult
{
    ExecResultLocal(String str)     {_str = str;}
    ExecResultLocal(ValueLocal val) {_val = val;}
    ExecResultLocal(String errorMessage,
                    String errorFilename,
                    int    errorLineNumber,
                    String errorLineBuffer,
                    int    errorTokenOffset)

    {
        _errorOccured    = true;
        _errorMessage    = errorMessage;
        _errorFilename   = errorFilename;
        _errorLineNumber = errorLineNumber;
        _errorLineBuffer = errorLineBuffer;
        _errorTokenOffset= errorTokenOffset;
    }

    public String   getResult()             {return _str;}
    public boolean  getErrorOccured()       {return _errorOccured;}
    public String   getErrorMessage()       {return _errorMessage;}
    public String   getErrorFilename()      {return _errorFilename;}
    public int      getErrorLineNumber()    {return _errorLineNumber;}
    public String   getErrorLineBuffer()    {return _errorLineBuffer;}
    public int      getErrorTokenOffset()   {return _errorTokenOffset;}

    public com.netscape.jsdebugging.api.Value  getResultValue() {return _val;}

    private String _str;
    private ValueLocal _val;

    private boolean _errorOccured;
    private String  _errorMessage;
    private String  _errorFilename;
    private int     _errorLineNumber;
    private String  _errorLineBuffer;
    private int     _errorTokenOffset;
}    


