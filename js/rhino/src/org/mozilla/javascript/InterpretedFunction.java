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
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Roger Lawrence
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

import java.util.*;
import org.mozilla.javascript.debug.DebuggableScript;

class InterpretedFunction extends NativeFunction implements DebuggableScript {
    
    InterpretedFunction(InterpreterData theData, Context cx) {
        itsData = theData;
        init(cx);
    }
    
    void init(Context cx)
    {
// probably too much copying going on from theData to the InterpretedFunction object
// should pass them as parameters - unless we need them in the data block anyway?

        functionName = itsData.itsName;
        int N = itsData.itsVariableTable.size();
        if (N != 0) {
            argNames = new String[N];
            for (int i = 0; i != N; i++) {
                argNames[i] = itsData.itsVariableTable.getName(i);
            }
        }
        argCount = (short)itsData.itsVariableTable.getParameterCount();
        source = itsData.itsSource;
        nestedFunctions = itsData.itsNestedFunctions;
        if (cx != null)
            version = (short)cx.getLanguageVersion();
    }
    
    InterpretedFunction(InterpretedFunction theOther,
                        Scriptable theScope, Context cx)
    {
        itsData = theOther.itsData;
        itsClosure = theScope;
        init(cx);
    }
    
    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {            
        if (itsClosure != null)
            scope = itsClosure;
        else if (!itsData.itsUseDynamicScope)
            scope = getParentScope();
        if (itsData.itsNeedsActivation)
            scope = ScriptRuntime.initVarObj(cx, scope, this, thisObj, args);
        return Interpreter.interpret(cx, scope, thisObj, args, this, itsData);
    }
    
    public boolean isFunction() {
        return true;
    }
    
    public Scriptable getScriptable() {
        return this;
    }
    
    public String getSourceName() {
        return itsData.itsSourceFile;
    }
    
    public Enumeration getLineNumbers() { 
        return itsData.itsLineNumberTable.keys();
    }
    
    public boolean placeBreakpoint(int line) { // XXX throw exn?
        return itsData.placeBreakpoint(line);
    }
    
    public boolean removeBreakpoint(int line) {
        return itsData.removeBreakpoint(line);
    }
    
    InterpreterData itsData;
    Scriptable itsClosure;
}
    
