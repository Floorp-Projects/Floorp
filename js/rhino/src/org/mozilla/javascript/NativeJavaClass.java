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

import java.lang.reflect.*;
import java.util.Hashtable;

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

    public NativeJavaClass(Scriptable scope, Class cl) {
        super(cl, JavaMembers.lookupClass(scope, cl, cl));
        fieldAndMethods = members.getFieldAndMethodsObjects(javaObject, false);
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
        
        if (fieldAndMethods != null) {
            result = fieldAndMethods.get(name);
            if (result != null)
                return result;
        }
        
        if (members.has(name, true)) {
            result = members.get(this, name, javaObject, true);
        } else {
            // experimental:  look for nested classes by appending $name to current class' name.
            try {
                String nestedName = getClassObject().getName() + '$' + name;
                Class nestedClass = Class.forName(nestedName);
                Scriptable nestedValue = wrap(ScriptableObject.getTopLevelScope(this), nestedClass);
                nestedValue.setParentScope(this);
                result = nestedValue;
            } catch (ClassNotFoundException ex) {
                throw members.reportMemberNotFound(name);
            }
        }
		
     	return result;
    }

    public void put(String name, Scriptable start, Object value) {
        members.put(name, javaObject, value, true);
    }

    public Object[] getIds() {
        return members.getIds(true);
    }
    
    public Class getClassObject() { 
        return (Class) super.unwrap();
    }

    // XXX ??
    public static NativeJavaClass wrap(Scriptable scope, Class cls) {
        return new NativeJavaClass(scope, cls);
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
        throws JavaScriptException
    {
        return construct(cx, scope, args);
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
    	Class classObject = getClassObject();
        Scriptable topLevel = ScriptableObject.getTopLevelScope(this);
    	int modifiers = classObject.getModifiers();
    	if (! (Modifier.isInterface(modifiers) || 
               Modifier.isAbstract(modifiers))) 
        {
	        for (int i = 0; i < args.length; i++) {
	            if (args[i] instanceof Wrapper)
	                args[i] = ((Wrapper)args[i]).unwrap();
	        }

	        Constructor[] ctors = members.getConstructors();
	        Member member = NativeJavaMethod.findFunction(ctors, args);
	        Constructor ctor = (Constructor) member;
	        if (ctor == null) {
	            String sig = NativeJavaMethod.signature(args);
	            Object errArgs[] = { getClassObject().getName(), sig };
	            throw Context.reportRuntimeError(Context.getMessage(
	                "msg.no.java.ctor", errArgs));
	        }

	        // Found the constructor, so try invoking it.
	        Class[] paramTypes = ctor.getParameterTypes();
	        for (int i = 0; i < args.length; i++) {
	            args[i] = NativeJavaObject.coerceType(paramTypes[i], args[i]);
	        }
	        try {
                    /* we need to force this to be wrapped, because construct _has_
                     * to return a scriptable */
                    return (Scriptable) NativeJavaObject.wrap(
                                            topLevel, ctor.newInstance(args),
                                            getClassObject());

	        } catch (InstantiationException instEx) {
	            Object[] errArgs = { instEx.getMessage(), 
	                                 getClassObject().getName() };
	            throw Context.reportRuntimeError(Context.getMessage
	                                             ("msg.cant.instantiate",
	                                              errArgs));
	        } catch (IllegalArgumentException argEx) {
	            String signature = NativeJavaMethod.signature(args);
	            String ctorString = ctor.toString();
	            Object[] errArgs = { argEx.getMessage(),ctorString,signature };
	            throw Context.reportRuntimeError(Context.getMessage
	                                             ("msg.bad.ctor.sig",
	                                             errArgs));
	        } catch (InvocationTargetException e) {
	            throw JavaScriptException.wrapException(scope, e);
	        } catch (IllegalAccessException accessEx) {
	            Object[] errArgs = { accessEx.getMessage() };
	            throw Context.reportRuntimeError(Context.getMessage
	                                             ("msg.java.internal.private", errArgs));
	        }
	    } else {
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
                    msg = ex.getMessage();
                }
	        Object[] errArgs = { msg, getClassObject().getName() };
	        throw Context.reportRuntimeError(Context.getMessage
	                                         ("msg.cant.instantiate",
	                                          errArgs));
	    }
    }

    public String toString() {
        return "[JavaClass " + getClassObject().getName() + "]";
    }

    /**
     * Determines if prototype is a wrapped Java object and performs
     * a Java "instanceof"
     */
    public boolean hasInstance(Scriptable value) {

        if (value instanceof NativeJavaObject) {
            Object instance = ((NativeJavaObject)value).unwrap();

            return getClassObject().isInstance(instance);
        }

        // value wasn't something we understand
        return false;
    }

    public Scriptable getParentScope() {
        return parent;
    }

    public void setParentScope(Scriptable scope) {
        // beard:
        // need at least the top-most scope, so JavaMembers.reflectMethod()
        // will work. this fixes a bug where a static field of a class would get
        // reflected by JavaMembers.reflect() in the scope of a NativeJavaClass,
        // and calls to ScriptableObject.getFunctionPrototype() would return
        // null because there was no top-level scope with "Function" defined.
        // question: should the package hierarchy form the scope chain or is top-level sufficient?
        parent = scope;
    }
    
    private Hashtable fieldAndMethods;

    // beard: need a scope for finding top-level prototypes.
    private Scriptable parent;
}
