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

import java.lang.reflect.Array;

/**
 * This class reflects Java arrays into the JavaScript environment.
 *
 * @author Mike Shaver
 * @see NativeJavaClass
 * @see NativeJavaObject
 * @see NativeJavaPackage
 */

public class NativeJavaArray extends NativeJavaObject {

    public String getClassName() {
        return "JavaArray";
    }

    public static NativeJavaArray wrap(Scriptable scope, Object array) {
        return new NativeJavaArray(scope, array);
    }

    public Object unwrap() {
        return array;
    }

    public NativeJavaArray(Scriptable scope, Object array) {
        super(scope, null, ScriptRuntime.ObjectClass);
        Class cl = array.getClass();
        if (!cl.isArray()) {
            throw new RuntimeException("Array expected");
        }
        this.array = array;
        this.length = Array.getLength(array);
        this.cls = cl.getComponentType();
    }

    public boolean has(String id, Scriptable start) {
        return id.equals("length") || super.has(id, start);
    }

    public boolean has(int index, Scriptable start) {
        return 0 <= index && index < length;
    }

    public Object get(String id, Scriptable start) {
        if (id.equals("length"))
            return new Integer(length);
        return super.get(id, start);
    }

    public Object get(int index, Scriptable start) {
        if (0 <= index && index < length)
            return NativeJavaObject.wrap(this, Array.get(array, index), cls);
        return Undefined.instance;
    }

    public void put(String id, Scriptable start, Object value) {
        // Ignore assignments to "length"--it's readonly.
        if (!id.equals("length"))
            super.put(id, start, value);
    }
    
    public void put(int index, Scriptable start, Object value) {
        if (0 <= index && index < length) {
            if (value instanceof Wrapper)
                value = ((Wrapper)value).unwrap();
            Array.set(array, index, NativeJavaObject.coerceType(cls, value));
            return;
        }
        super.put(index, start, value);
    }

    public Object getDefaultValue(Class hint) {
        if (hint == null || hint == ScriptRuntime.StringClass) 
            return array.toString();
        if (hint == ScriptRuntime.BooleanClass)
            return Boolean.TRUE;
        if (hint == ScriptRuntime.NumberClass)
            return ScriptRuntime.NaNobj;
        return this;
    }
    
    public Object[] getIds() {
        Object[] result = new Object[length];
        int i = length;
        while (--i >= 0)
            result[i] = new Integer(i);
        return result;
    }

    public boolean hasInstance(Scriptable value) {
        if (!(value instanceof NativeJavaObject))
            return false;
        Object instance = ((NativeJavaObject)value).unwrap();
        return cls.isInstance(instance);
    }

    Object array;
    int length;
    Class cls;
}
