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
 * This class implements the "arguments" object.
 *
 * See ECMA 10.1.8
 *
 * @see org.mozilla.javascript.NativeCall
 * @author Norris Boyd
 */
class Arguments extends ScriptableObject {

    public Arguments(NativeCall activation) {
        this.activation = activation;

        Scriptable parent = activation.getParentScope();
        setParentScope(parent);
        setPrototype(ScriptableObject.getObjectPrototype(parent));

        args = activation.getOriginalArguments();
        int length = args.length;
        Object callee = activation.funObj;

        defineProperty("length", new Integer(length),
                       ScriptableObject.DONTENUM);
        defineProperty("callee", callee, ScriptableObject.DONTENUM);

        hasCaller = activation.funObj.version <= Context.VERSION_1_3;
    }

    public String getClassName() {
        return "Arguments";
    }

    public boolean has(String name, Scriptable start) {
        return (hasCaller && name.equals("caller")) || super.has(name, start);
    }

    public boolean has(int index, Scriptable start) {
        Object[] args = activation.getOriginalArguments();
        return (0 <= index && index < args.length) || super.has(index, start);
    }

    public Object get(String name, Scriptable start) {
        if (hasCaller && name.equals("caller")) {
            NativeCall caller = activation.caller;
            if (caller == null || caller.originalArgs == null)
                return null;
            return caller.get("arguments", caller);
        }
        return super.get(name, start);
    }

    public Object get(int index, Scriptable start) {
        if (0 <= index && index < args.length) {
            NativeFunction f = activation.funObj;
            if (index < f.argCount)
                return activation.get(f.names[index+1], activation);
            return args[index];
        }
        return super.get(index, start);
    }

    public void put(String name, Scriptable start, Object value) {
        if (name.equals("caller"))
            hasCaller = false;
        super.put(name, start, value);
    }

    public void put(int index, Scriptable start, Object value) {
        if (0 <= index && index < args.length) {
            NativeFunction f = activation.funObj;
            if (index < f.argCount)
                activation.put(f.names[index+1], activation, value);
            else
                args[index] = value;
            return;
        }
        super.put(index, start, value);
    }

    public void delete(String name) {
        if (name.equals("caller"))
            hasCaller = false;
        super.delete(name);
    }

    private NativeCall activation;
    private Object[] args;
    private boolean hasCaller;
}
