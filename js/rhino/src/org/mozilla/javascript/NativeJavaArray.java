/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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
 * Igor Bukanov
 * Frank Mitchell
 * Mike Shaver
 * Kemal Bayram
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

import java.lang.reflect.Array;
import java.io.*;

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
        Object result = super.get(id, start);
        if (result == NOT_FOUND &&
            !ScriptableObject.hasProperty(getPrototype(), id))
        {
            throw Context.reportRuntimeError2(
                "msg.java.member.not.found", array.getClass().getName(), id);
        }
        return result;
    }

    public Object get(int index, Scriptable start) {
        if (0 <= index && index < length) {
            Context cx = Context.getContext();
            Object obj = Array.get(array, index);
            return cx.getWrapFactory().wrap(cx, this, obj, cls);
        }
        return Undefined.instance;
    }

    public void put(String id, Scriptable start, Object value) {
        // Ignore assignments to "length"--it's readonly.
        if (!id.equals("length"))
            super.put(id, start, value);
    }

    public void put(int index, Scriptable start, Object value) {
        if (0 <= index && index < length) {
            Array.set(array, index, NativeJavaObject.coerceType(cls, value,
                                                                true));
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
        if (!(value instanceof Wrapper))
            return false;
        Object instance = ((Wrapper)value).unwrap();
        return cls.isInstance(instance);
    }

    public Scriptable getPrototype() {
        if (prototype == null) {
            prototype =
                ScriptableObject.getClassPrototype(this.getParentScope(),
                                                   "Array");
        }
        return prototype;
    }

    Object array;
    int length;
    Class cls;
    Scriptable prototype;
}
