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
 * Norris Boyd
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
 * This class implements the activation object.
 *
 * See ECMA 10.1.6
 *
 * @see org.mozilla.javascript.Arguments
 * @author Norris Boyd
 */
public final class NativeCall extends IdScriptable {

    static void init(Context cx, Scriptable scope, boolean sealed) {
        NativeCall obj = new NativeCall();
        obj.prototypeFlag = true;
        obj.addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    NativeCall(Context cx, Scriptable scope, NativeFunction funObj, 
               Scriptable thisObj, Object[] args)
    {
        this.funObj = funObj;
        this.thisObj = thisObj;
        
        setParentScope(scope);
        // leave prototype null
        
        // save current activation
        this.caller = cx.currentActivation;
        cx.currentActivation = this;

        this.originalArgs = (args == null) ? ScriptRuntime.emptyArgs : args;
        
        // initialize values of arguments
        String[] argNames = funObj.argNames;
        if (argNames != null) {
            for (int i=0; i < funObj.argCount; i++) {
                Object val = i < args.length ? args[i] 
                                             : Undefined.instance;
                super.put(argNames[i], this, val);
            }
        }
        
        // initialize "arguments" property
        super.put("arguments", this, new Arguments(this));
    }
    
    private NativeCall() {
    }

    public String getClassName() {
        return "Call";
    }
    
    private static Object jsConstructor(Context cx, Object[] args, 
                                        Function ctorObj, boolean inNewExpr)
    {
        if (!inNewExpr) {
            throw Context.reportRuntimeError1("msg.only.from.new", "Call");
        }
        ScriptRuntime.checkDeprecated(cx, "Call");
        NativeCall result = new NativeCall();
        result.setPrototype(getObjectPrototype(ctorObj));
        return result;
    }
    
    NativeCall getActivation(Function f) {
        NativeCall x = this;
        do {
            if (x.funObj == f)
                return x;
            x = x.caller;
        } while (x != null);
        return null;
    }
        
    public Function getFunctionObject() {
        return funObj;
    }

    public Object[] getOriginalArguments() {
        return originalArgs;
    }
    
    public NativeCall getCaller() {
        return caller;
    }
        
    public Scriptable getThisObj() {
        return thisObj;
    }
    
    public int methodArity(int methodId) {
        if (prototypeFlag) {
            if (methodId == Id_constructor) return 1;
        }
        return super.methodArity(methodId);
    }

    public Object execMethod
        (int methodId, IdFunction f,
         Context cx, Scriptable scope, Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        if (prototypeFlag) {
            if (methodId == Id_constructor) {
                return jsConstructor(cx, args, f, thisObj == null);
            }
        }
        return super.execMethod(methodId, f, cx, scope, thisObj, args);
    }

    protected String getIdName(int id) {
        if (prototypeFlag) {
            if (id == Id_constructor) return "constructor";
        }
        return null;        
    }
    
    protected int mapNameToId(String s) {
        if (!prototypeFlag) { return 0; }
        return s.equals("constructor") ? Id_constructor : 0;
    }

    private static final int
        Id_constructor   = 1,
        MAX_PROTOTYPE_ID = 1;

    NativeCall caller;
    NativeFunction funObj;
    Scriptable thisObj;
    Object[] originalArgs;
    public int debugPC;

    private boolean prototypeFlag;
}
