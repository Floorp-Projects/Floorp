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
 * Frank Mitchell
 * Mike Shaver
 * Kurt Westerfeld
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

import java.lang.reflect.*;
import java.util.Hashtable;
import java.io.*;

/**
 * This class reflects Java classes into the JavaScript environment, mainly
 * for constructors and static members.  We lazily reflect properties,
 * and currently do not guarantee that a single j.l.Class is only
 * reflected once into the JS environment, although we should.
 * The only known case where multiple reflections
 * are possible occurs when a j.l.Class is wrapped as part of a
 * method return or property access, rather than by walking the
 * Packages/java tree.
 *
 * @author Mike Shaver
 * @see NativeJavaArray
 * @see NativeJavaObject
 * @see NativeJavaPackage
 */

public class NativeJavaClass extends NativeJavaObject implements Function {

    public NativeJavaClass() {
    }

    public NativeJavaClass(Scriptable scope, Class cl) {
        this.parent = scope;
        this.javaObject = cl;
        initMembers();
    }

    protected void initMembers() {
        Class cl = (Class)javaObject;
        members = JavaMembers.lookupClass(parent, cl, cl);
        staticFieldAndMethods
            = members.getFieldAndMethodsObjects(this, cl, true);
    }

    public String getClassName() {
        return "JavaClass";
    }

    public boolean has(String name, Scriptable start) {
        return members.has(name, true);
    }

    public Object get(String name, Scriptable start) {
        // When used as a constructor, ScriptRuntime.newObject() asks
        // for our prototype to create an object of the correct type.
        // We don't really care what the object is, since we're returning
        // one constructed out of whole cloth, so we return null.

        if (name.equals("prototype"))
            return null;

        Object result = Scriptable.NOT_FOUND;

        if (staticFieldAndMethods != null) {
            result = staticFieldAndMethods.get(name);
            if (result != null)
                return result;
        }

        if (members.has(name, true)) {
            result = members.get(this, name, javaObject, true);
        } else {
            // experimental:  look for nested classes by appending $name to
            // current class' name.
            Class nestedClass = findNestedClass(getClassObject(), name);
            if (nestedClass == null) {
                throw members.reportMemberNotFound(name);
            }
            NativeJavaClass nestedValue = new NativeJavaClass
                (ScriptableObject.getTopLevelScope(this), nestedClass);
            nestedValue.setParentScope(this);
            result = nestedValue;
        }

        return result;
    }

    public void put(String name, Scriptable start, Object value) {
        members.put(this, name, javaObject, value, true);
    }

    public Object[] getIds() {
        return members.getIds(true);
    }

    public Class getClassObject() {
        return (Class) super.unwrap();
    }

    public Object getDefaultValue(Class hint) {
        if (hint == null || hint == ScriptRuntime.StringClass)
            return this.toString();
        if (hint == ScriptRuntime.BooleanClass)
            return Boolean.TRUE;
        if (hint == ScriptRuntime.NumberClass)
            return ScriptRuntime.NaNobj;
        return this;
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        // If it looks like a "cast" of an object to this class type,
        // walk the prototype chain to see if there's a wrapper of a
        // object that's an instanceof this class.
        if (args.length == 1 && args[0] instanceof Scriptable) {
            Class c = getClassObject();
            Scriptable p = (Scriptable) args[0];
            do {
                if (p instanceof Wrapper) {
                    Object o = ((Wrapper) p).unwrap();
                    if (c.isInstance(o))
                        return p;
                }
                p = p.getPrototype();
            } while (p != null);
        }
        return construct(cx, scope, args);
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
    {
        Class classObject = getClassObject();
        int modifiers = classObject.getModifiers();
        if (! (Modifier.isInterface(modifiers) ||
               Modifier.isAbstract(modifiers)))
        {
            MemberBox[] ctors = members.ctors;
            int index = NativeJavaMethod.findFunction(cx, ctors, args);
            if (index < 0) {
                String sig = NativeJavaMethod.scriptSignature(args);
                throw Context.reportRuntimeError2(
                    "msg.no.java.ctor", classObject.getName(), sig);
            }

            // Found the constructor, so try invoking it.
            return constructSpecific(cx, scope, args, ctors[index]);
        } else {
            Scriptable topLevel = ScriptableObject.getTopLevelScope(this);
            String msg = "";
            try {
                // trying to construct an interface; use JavaAdapter to
                // construct a new class on the fly that implements this
                // interface.
                Object v = topLevel.get("JavaAdapter", topLevel);
                if (v != NOT_FOUND) {
                    Function f = (Function) v;
                    Object[] adapterArgs = { this, args[0] };
                    return (Scriptable) f.construct(cx, topLevel,
                                                    adapterArgs);
                }
            } catch (Exception ex) {
                // fall through to error
                String m = ex.getMessage();
                if (m != null)
                    msg = m;
            }
            throw Context.reportRuntimeError2(
                "msg.cant.instantiate", msg, classObject.getName());
        }
    }

    static Scriptable constructSpecific(Context cx, Scriptable scope,
                                        Object[] args, MemberBox ctor)
    {
        Scriptable topLevel = ScriptableObject.getTopLevelScope(scope);
        Class classObject = ctor.getDeclaringClass();
        Class[] argTypes = ctor.argTypes;

        Object[] origArgs = args;
        for (int i = 0; i < args.length; i++) {
            Object arg = args[i];
            Object x = NativeJavaObject.coerceType(argTypes[i], arg, true);
            if (x != arg) {
                if (args == origArgs) {
                    args = (Object[])origArgs.clone();
                }
                args[i] = x;
            }
        }
        Object instance = ctor.newInstance(args);
        // we need to force this to be wrapped, because construct _has_
        // to return a scriptable
        return cx.getWrapFactory().wrapNewObject(cx, topLevel, instance);
    }

    public String toString() {
        return "[JavaClass " + getClassObject().getName() + "]";
    }

    /**
     * Determines if prototype is a wrapped Java object and performs
     * a Java "instanceof".
     * Exception: if value is an instance of NativeJavaClass, it isn't
     * considered an instance of the Java class; this forestalls any
     * name conflicts between java.lang.Class's methods and the
     * static methods exposed by a JavaNativeClass.
     */
    public boolean hasInstance(Scriptable value) {

        if (value instanceof Wrapper &&
            !(value instanceof NativeJavaClass)) {
            Object instance = ((Wrapper)value).unwrap();

            return getClassObject().isInstance(instance);
        }

        // value wasn't something we understand
        return false;
    }

    private static Class findNestedClass(Class parentClass, String name) {
        String nestedClassName = parentClass.getName() + '$' + name;
        ClassLoader loader = parentClass.getClassLoader();
        if (loader == null) {
            // ALERT: if loader is null, nested class should be loaded
            // via system class loader which can be different from the
            // loader that brought Rhino classes that Class.forName() would
            // use, but ClassLoader.getSystemClassLoader() is Java 2 only
            return Kit.classOrNull(nestedClassName);
        } else {
            return Kit.classOrNull(loader, nestedClassName);
        }
    }

    private Hashtable staticFieldAndMethods;
}
