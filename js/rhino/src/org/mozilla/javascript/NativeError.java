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
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
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

/**
 *
 * The class of error objects
 *
 *  ECMA 15.11
 */
public class NativeError extends ScriptableObject {

    public NativeError() {
    }
    
    public static Object jsConstructor(Context cx, Object[] args, 
                                       Function funObj, boolean inNewExpr)
    {
        NativeError result = new NativeError();
        if (args.length >= 1) 
            result.put("message", result, cx.toString(args[0]));
        result.setPrototype(getClassPrototype(funObj, "Error"));
        return result;
    }
    
    public String getClassName() { 
        return "Error"; 
    }

    public String toString() {
        return getName() + ": " + getMessage();
    }
    
    public String jsFunction_toString() {
        return toString();
    }
    
    public String getName() {
        return ScriptRuntime.toString(
                ScriptRuntime.getProp(this, "name", this));
    }
    
    public String getMessage() {
        return ScriptRuntime.toString(
                ScriptRuntime.getProp(this, "message", this));
    }    
    
    public static void finishInit(Scriptable scope, FunctionObject ctor,
                                  Scriptable proto)
        throws PropertyException
    {
        ((ScriptableObject) proto).defineProperty("message", "", 
                                                  ScriptableObject.EMPTY);
        ((ScriptableObject) proto).defineProperty("name", "Error", 
                                                  ScriptableObject.EMPTY);
    }

}
