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

package org.mozilla.javascript;

public class InterpretedScript extends NativeScript {

    InterpretedScript(InterpreterData theData, Context cx)
    {
        itsData = theData;
        names = new String[itsData.itsVariableTable.size() + 1];
        names[0] = "";
        for (int i = 0; i < itsData.itsVariableTable.size(); i++)
            names[i + 1] = itsData.itsVariableTable.getName(i);
        nestedFunctions = itsData.itsNestedFunctions;
        version = (short)cx.getLanguageVersion();   
    }
    
    public Object exec(Context cx, Scriptable scope)
        throws JavaScriptException
    {
        return call(cx, scope, scope, null);    
    }

    public Object call(Context cx, Scriptable scope, 
                       Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        scope = ScriptRuntime.initScript(cx, scope, this, thisObj);
        itsData.itsCX = cx;
        itsData.itsScope = scope;
        itsData.itsThisObj = thisObj;
        itsData.itsInArgs = args;
        return Interpreter.interpret(itsData);    
    }

    InterpreterData itsData;
   
}

