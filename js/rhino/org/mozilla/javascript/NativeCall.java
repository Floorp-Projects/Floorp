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

/**
 * This class implements the activation object.
 *
 * See ECMA 10.1.6
 *
 * @see org.mozilla.javascript.Arguments
 * @author Norris Boyd
 */
public final class NativeCall extends ScriptableObject {

    NativeCall(Context cx, Scriptable scope, NativeFunction funObj, 
               Scriptable thisObj, Object[] args)
    {
        this(cx, scope, funObj, thisObj);
        this.originalArgs = args;
        
        // initialize references to nested functions
        NativeFunction[] fns = funObj.nestedFunctions;
        if (fns != null) {
            for (int i=0; i < fns.length; i++) {
                NativeFunction f = fns[i];
                if (f.names != null)
                    super.put(f.names[0], this, f);
            }
        }
        
        // initialize values of arguments
        String[] names = funObj.names;
        if (names != null) {
            for (int i=0; i < funObj.argCount; i++) {
                Object val = i < args.length ? args[i] 
                                             : Undefined.instance;
                super.put(names[i+1], this, val);
            }
        }
        
        // initialize "arguments" property
        super.put("arguments", this, new Arguments(this));
    }
    
    NativeCall(Context cx, Scriptable scope, NativeFunction funObj, 
               Scriptable thisObj)
    {
        this.funObj = funObj;
        this.thisObj = thisObj;
        
        setParentScope(scope);
        // leave prototype null
        
        // save current activation
        this.caller = cx.currentActivation;
        cx.currentActivation = this;
    }
    
    // Needed in order to use this class with ScriptableObject.defineClass
    public NativeCall() {
    }

    public String getClassName() {
        return "Call";
    }
    
    public static Object js_Call(Context cx, Object[] args, Function ctorObj,
                                 boolean inNewExpr)
    {
        if (!inNewExpr) {
            Object[] errArgs = { "Call" };
            throw Context.reportRuntimeError(Context.getMessage
                                             ("msg.only.from.new", errArgs));
        }
        ScriptRuntime.checkDeprecated(cx, "Call");
        NativeCall result = new NativeCall();
        result.setPrototype(getObjectPrototype(ctorObj));
        return result;
    }
    
    NativeCall getActivation(NativeFunction f) {
        NativeCall x = this;
        do {
            if (x.funObj == f)
                return x;
            x = x.caller;
        } while (x != null);
        return null;
    }
        
    public NativeFunction getFunctionObject() {
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
    
    NativeCall caller;
    NativeFunction funObj;
    Scriptable thisObj;
    Object[] originalArgs;
    public int debugPC;
}
