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

import java.lang.reflect.Method;

/**
 * This class implements the object lookup required for the
 * <code>with</code> statement.
 * It simply delegates every action to its prototype except
 * for operations on its parent.
 */
public class NativeWith implements Scriptable {

    public static void init(Scriptable scope) {
        NativeWith w = new NativeWith();
        w.setPrototype(ScriptableObject.getObjectPrototype(scope));
        Method[] m = FunctionObject.findMethods(NativeWith.class, "With");
        FunctionObject f = new FunctionObject("With", m[0], scope);
        f.addAsConstructor(scope, w);
    }

    public NativeWith() {
    }

    public NativeWith(Scriptable parent, Scriptable prototype) {
        this.parent = parent;
        this.prototype = prototype;
    }

    public String getClassName() {
        return "With";
    }

    public boolean has(String id, Scriptable start) {
        if (start == this)
            start = prototype;
        return prototype.has(id, start);
    }

    public boolean has(int index, Scriptable start) {
        if (start == this)
            start = prototype;
        return prototype.has(index, start);
    }

    public Object get(String id, Scriptable start) {
        if (start == this)
            start = prototype;
        return prototype.get(id, start);
    }

    public Object get(int index, Scriptable start) {
        if (start == this)
            start = prototype;
        return prototype.get(index, start);
    }

    public void put(String id, Scriptable start, Object value) {
        if (start == this)
            start = prototype;
        prototype.put(id, start, value);
    }

    public void put(int index, Scriptable start, Object value) {
        if (start == this)
            start = prototype;
        prototype.put(index, start, value);
    }

    public void delete(String id) {
        prototype.delete(id);
    }

    public void delete(int index) {
        prototype.delete(index);
    }

    public Scriptable getPrototype() {
        return prototype;
    }

    public void setPrototype(Scriptable prototype) {
        this.prototype = prototype;
    }

    public Scriptable getParentScope() {
        return parent;
    }

    public void setParentScope(Scriptable parent) {
        this.parent = parent;
    }

    public Object[] getIds() {
        return prototype.getIds();
    }

    public Object getDefaultValue(Class typeHint) {
        return prototype.getDefaultValue(typeHint);
    }

    public boolean hasInstance(Scriptable value) {
        return prototype.hasInstance(value);
    }

    public static Object With(Context cx, Object[] args, Function ctorObj,
                              boolean inNewExpr)
    {
        Object[] msgArgs = { "With" };
        throw Context.reportRuntimeError(
            Context.getMessage("msg.cant.call.indirect", msgArgs));
    }

    public static Object newWithSpecial(Context cx, Object[] args, 
                                        Function ctorObj, boolean inNewExpr)
    {
        if (!inNewExpr) {
            Object[] errArgs = { "With" };
            throw Context.reportRuntimeError(Context.getMessage
                                             ("msg.only.from.new", errArgs));
        }
        
        ScriptRuntime.checkDeprecated(cx, "With");
        
        Scriptable scope = ScriptableObject.getTopLevelScope(ctorObj);
        NativeWith thisObj = new NativeWith();
        thisObj.setPrototype(args.length == 0
                             ? ScriptableObject.getClassPrototype(scope,
								  "Object")
                             : ScriptRuntime.toObject(scope, args[0]));
        thisObj.setParentScope(scope);
        return thisObj;
    }
    
    private Scriptable prototype;
    private Scriptable parent;
    private Scriptable constructor;
}
