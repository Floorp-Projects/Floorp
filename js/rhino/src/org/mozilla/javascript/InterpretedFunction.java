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

package org.mozilla.javascript;

class InterpretedFunction extends NativeFunction {
    
    InterpretedFunction(InterpreterData theData, Context cx)
    {
        itsData = theData;

// probably too much copying going on from theData to the InterpretedFunction object
// should pass them as parameters - unless we need them in the data block anyway?

        names = new String[itsData.itsVariableTable.size() + 1];
        names[0] = itsData.itsName;
        for (int i = 0; i < itsData.itsVariableTable.size(); i++)
            names[i + 1] = itsData.itsVariableTable.getName(i);
        argCount = (short)itsData.itsVariableTable.getParameterCount();
        source = itsData.itsSource;
        nestedFunctions = itsData.itsNestedFunctions;
        version = (short)cx.getLanguageVersion();   
    }
    
    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {            
        if (itsData.itsNeedsActivation)
            scope = ScriptRuntime.initVarObj(cx, scope, this, thisObj, args);
        itsData.itsCX = cx;
        itsData.itsScope = scope;
        itsData.itsThisObj = thisObj;
        itsData.itsInArgs = args;
        return Interpreter.interpret(itsData);    
    }

    InterpreterData itsData;
}
    
