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
 * This class implements the closure object.
 *
 * @author Norris Boyd
 */
public class NativeClosure extends ScriptableObject implements Function {

    public NativeClosure() {
    }

    public NativeClosure(Context cx, Scriptable scope, NativeFunction f) {
        setPrototype(f);
        setParentScope(scope);
        String name = f.names != null ? f.names[0] : "";
        if (name != null && name.length() > 0) {
            scope.put(name, scope, scope.getParentScope() == null
                                   ? (Object) f : this);
        }
    }

    public String getClassName() {
        return "Closure";
    }

    public Object getDefaultValue(Class typeHint) {
        if (typeHint == ScriptRuntime.FunctionClass)
            return prototype;
        return super.getDefaultValue(typeHint);
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        Function proto = checkProto();
        return proto.call(cx, getParentScope(), thisObj, args);
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        Function proto = checkProto();
        return proto.construct(cx, getParentScope(), args);
    }

    public static Object js_Closure(Context cx, Object[] args,
                                    Function ctorObj, boolean inNewExpr)
    {
        Object[] msgArgs = { "Closure" };
        throw Context.reportRuntimeError(
            Context.getMessage("msg.cant.call.indirect", msgArgs));
    }

    public static Object newClosureSpecial(Context cx, Scriptable varObj,
                                           Object[] args, Function ctorObj)
    {
        ScriptRuntime.checkDeprecated(cx, "Closure");
        
        NativeFunction f = args.length > 0 &&
                           args[0] instanceof NativeFunction
                           ? (NativeFunction) args[0]
                           : null;
        NativeClosure result = f != null
                               ? new NativeClosure(cx, varObj, f)
                               : new NativeClosure();
        Scriptable scope = getTopLevelScope(ctorObj);
        result.setPrototype(args.length == 0
                            ? getObjectPrototype(scope)
                            : ScriptRuntime.toObject(scope, args[0]));
        result.setParentScope(varObj);
        return result;
    }

    private Function checkProto() {
        Scriptable proto = getPrototype();
        if (!(proto instanceof Function)) {
            throw Context.reportRuntimeError(Context.getMessage
                      ("msg.closure.proto", null));
        }
        return (Function) proto;
    }
}

