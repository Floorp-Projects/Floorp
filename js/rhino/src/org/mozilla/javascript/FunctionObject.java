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

// API class

package org.mozilla.javascript;

import java.util.Hashtable;
import java.util.Vector;
import java.lang.reflect.*;

public class FunctionObject extends NativeFunction {

    /**
     * Create a JavaScript function object from a Java method.
     *
     * <p>The <code>member</code> argument must be either a java.lang.reflect.Method
     * or a java.lang.reflect.Constructor and must match one of two forms.<p>
     *
     * The first form is a member with zero or more parameters
     * of the following types: Object, String, boolean, Scriptable,
     * byte, short, int, long, float, or double. If the member is
     * a Method, the return value must be void or one of the types
     * allowed for parameters.<p>
     *
     * The runtime will perform appropriate conversions based
     * upon the type of the parameter. A parameter type of
     * Object specifies that no conversions are to be done. A parameter
     * of type String will use Context.toString to convert arguments.
     * Similarly, parameters of type double, boolean, and Scriptable
     * will cause Context.toNumber, Context.toBoolean, and
     * Context.toObject, respectively, to be called.<p>
     *
     * If the method is not static, the Java 'this' value will
     * correspond to the JavaScript 'this' value. Any attempt
     * to call the function with a 'this' value that is not
     * of the right Java type will result in an error.<p>
     *
     * The second form is the variable arguments (or "varargs")
     * form. If the FunctionObject will be used as a constructor,
     * the member must have the following parameters
     * <pre>
     *      (Context cx, Scriptable thisObj, Object[] args,
     *       Function funObj) </pre>
     * and if it is a Method, be static and return an Object result.<p>
     *
     * Otherwise, if the FunctionObject will not be used to define a
     * constructor, the member must be a static Method with parameters
     * <pre>
     *      (Context cx, Object[] args, Function ctorObj,
     *       boolean inNewExpr)</pre>
     * and an Object result.<p>
     *
     * When the function varargs form is called as part of a function call,
     * the <code>args</code> parameter contains the
     * arguments, with <code>thisObj</code>
     * set to the JavaScript 'this' value. <code>funObj</code>
     * is the function object for the invoked function.<p>
     *
     * When the constructor varargs form is called or invoked while evaluating
     * a <code>new</code> expression, <code>args</code> contains the
     * arguments, <code>ctorObj</code> refers to this FunctionObject, and
     * <code>inNewExpr</code> is true if and only if  a <code>new</code>
     * expression caused the call. This supports defining a function that
     * has different behavior when called as a constructor than when
     * invoked as a normal function call. (For example, the Boolean
     * constructor, when called as a function,
     * will convert to boolean rather than creating a new object.)<p>
     *
     * @param name the name of the function
     * @param methodOrConstructor a java.lang.reflect.Method or a java.lang.reflect.Constructor
     *                            that defines the object
     * @param scope enclosing scope of function
     * @see org.mozilla.javascript.Scriptable
     */
    public FunctionObject(String name, Member methodOrConstructor,
                          Scriptable scope)
    {
        String methodName;
        if (methodOrConstructor instanceof Constructor) {
            ctor = (Constructor) methodOrConstructor;
            isStatic = true; // well, doesn't take a 'this'
            types = ctor.getParameterTypes();
            methodName = ctor.getName();
        } else {
            method = (Method) methodOrConstructor;
            isStatic = Modifier.isStatic(method.getModifiers());
            types = method.getParameterTypes();
            methodName = method.getName();
        }
        String myNames[] = { name };
        super.names = myNames;
        int length;
        if (types.length == 4 && (types[1].isArray() || types[2].isArray())) {
            // Either variable args or an error.
            if (types[1].isArray()) {
                if (!isStatic ||
                    types[0] != Context.class ||
                    types[1].getComponentType() != ScriptRuntime.ObjectClass ||
                    types[2] != ScriptRuntime.FunctionClass ||
                    types[3] != Boolean.TYPE)
                {
                    String message = Context.getMessage("msg.varargs.ctor",
                                                        null);
                    throw Context.reportRuntimeError(message);
                }
            } else {
                if (!isStatic ||
                    types[0] != Context.class ||
                    types[1] != ScriptRuntime.ScriptableClass ||
                    types[2].getComponentType() != ScriptRuntime.ObjectClass ||
                    types[3] != ScriptRuntime.FunctionClass)
                {
                    String message = Context.getMessage("msg.varargs.fun",
                                                        null);
                    throw Context.reportRuntimeError(message);
                }
            }
            // XXX check return type
            parmsLength = types[1].isArray() ? VARARGS_CTOR : VARARGS_METHOD;
            length = 1;
        } else {
            parmsLength = (short) types.length;
            boolean hasConversions = false;
            for (int i=0; i < parmsLength; i++) {
                Class type = types[i];
                if (type == ScriptRuntime.ObjectClass) {
                    // may not need conversions
                } else if (type == ScriptRuntime.StringClass || 
                           type == ScriptRuntime.BooleanClass ||
                           ScriptRuntime.NumberClass.isAssignableFrom(type) ||
                           Scriptable.class.isAssignableFrom(type))
                {
                    hasConversions = true;
                } else if (type == Boolean.TYPE) {
                    hasConversions = true;
                    types[i] = ScriptRuntime.BooleanClass;
                } else if (type == Byte.TYPE) {
                    hasConversions = true;
                    types[i] = ScriptRuntime.ByteClass;
                } else if (type == Short.TYPE) {
                    hasConversions = true;
                    types[i] = ScriptRuntime.ShortClass;
                } else if (type == Integer.TYPE) {
                    hasConversions = true;
                    types[i] = ScriptRuntime.IntegerClass;
                } else if (type == Float.TYPE) {
                    hasConversions = true;
                    types[i] = ScriptRuntime.FloatClass;
                } else if (type == Double.TYPE) {
                    hasConversions = true;
                    types[i] = ScriptRuntime.DoubleClass;
                } else {
                    Object[] errArgs = { methodName };
                    throw Context.reportRuntimeError(
                        Context.getMessage("msg.bad.parms", errArgs));
                }
            }
            if (!hasConversions)
                types = null;
            length = parmsLength;
        }

        // Initialize length property
        lengthPropertyValue = (short) length;

        hasVoidReturn = method != null && method.getReturnType() == Void.TYPE;
        this.argCount = (short) length;

        setParentScope(scope);
        setPrototype(getFunctionPrototype(scope));
    }

    /**
     * Override ScriptableObject's has, get, and set in order to define
     * the "length" property of the function. <p>
     *
     * We could also have defined the property using ScriptableObject's
     * defineProperty method, but that would have consumed a slot in every
     * FunctionObject. Most FunctionObjects typically don't have any
     * properties anyway, so having the "length" property would cause us
     * to allocate an array of slots. <p>
     *
     * In particular, this method will return true for 
     * <code>name.equals("length")</code>
     * and will delegate to the superclass for all other
     * values of <code>name</code>.
     */
    public boolean has(String name, Scriptable start) {
        return name.equals("length") || super.has(name, start);
    }

    /**
     * Override ScriptableObject's has, get, and set in order to define
     * the "length" property of the function. <p>
     *
     * In particular, this method will return the value defined by
     * the method used to construct the object (number of parameters
     * of the method, or 1 if the method is a "varargs" form), unless
     * setLength has been called with a new value.
     *
     * @see org.mozilla.javascript.FunctionObject#setLength
     */
    public Object get(String name, Scriptable start) {
        if (name.equals("length"))
            return new Integer(lengthPropertyValue);
        return super.get(name, start);
    }

    /**
     * Override ScriptableObject's has, get, and set in order to define
     * the "length" property of the function. <p>
     *
     * In particular, this method will ignore all attempts to set the
     * "length" property and forward all other requests to ScriptableObject.
     *
     * @see org.mozilla.javascript.FunctionObject#setLength
     */
    public void put(String name, Scriptable start, Object value) {
        if (!name.equals("length"))
            super.put(name, start, value);
    }

    /**
     * Set the value of the "length" property.
     *
     * <p>Changing the value of the "length" property of a FunctionObject only
     * affects the value retrieved from get() and does not affect the way
     * the method itself is called. <p>
     *
     * The "length" property will be defined by default as the number
     * of parameters of the method used to construct the FunctionObject,
     * unless the method is a "varargs" form, in which case the "length"
     * property will be defined to 1.
     *
     * @param length the new length
     */
    public void setLength(short length) {
        lengthPropertyValue = length;
    }

    // TODO: Make not public
    /**
     * Finds methods of a given name in a given class.
     *
     * <p>Searches <code>clazz</code> for methods with name
     * <code>name</code>. Maintains a cache so that multiple
     * lookups on the same class are cheap.
     *
     * @param clazz the class to search
     * @param name the name of the methods to find
     * @return an array of the found methods, or null if no methods
     *         by that name were found.
     * @see java.lang.Class#getMethods
     */
    public static Method[] findMethods(Class clazz, String name) {
        Vector v = new Vector(5);
        Method[] methods = clazz.getMethods();
        for (int i=0; i < methods.length; i++) {
            if (methods[i].getDeclaringClass() == clazz &&
                methods[i].getName().equals(name)){
                v.addElement(methods[i]);
            }
        }
        if (v.size() == 0) {
            return null;
        }
        Method[] result = new Method[v.size()];
        v.copyInto(result);
        return result;
    }

    /**
     * Define this function as a JavaScript constructor.
     * <p>
     * Sets up the "prototype" and "constructor" properties. Also
     * calls setParent and setPrototype with appropriate values.
     * Then adds the function object as a property of the given scope, using
     *      <code>prototype.getClassName()</code>
     * as the name of the property.
     *
     * @param scope the scope in which to define the constructor (typically
     *              the global object)
     * @param prototype the prototype object
     * @see org.mozilla.javascript.Scriptable#setParentScope
     * @see org.mozilla.javascript.Scriptable#setPrototype
     * @see org.mozilla.javascript.Scriptable#getClassName
     */
    public void addAsConstructor(Scriptable scope, Scriptable prototype) {
        setParentScope(scope);
        setPrototype(getFunctionPrototype(scope));
        prototype.setParentScope(this);
        final int attr = ScriptableObject.DONTENUM  |
                         ScriptableObject.PERMANENT |
                         ScriptableObject.READONLY;
        defineProperty("prototype", prototype, attr);
        String name = prototype.getClassName();
        if (!name.equals("With")) {
            // A "With" object would delegate these calls to the prototype:
            // not the right thing to do here!
            if (prototype instanceof ScriptableObject) {
                ((ScriptableObject) prototype).defineProperty("constructor",
                                                              this, attr);
            } else {
                prototype.put("constructor", prototype, this);
            }
        }
        if (scope instanceof ScriptableObject) {
            ((ScriptableObject) scope).defineProperty(name, this,
                ScriptableObject.DONTENUM);
        } else {
            scope.put(name, scope, this);
        }
        setParentScope(scope);
    }

    /**
     * Performs conversions on argument types if needed and
     * invokes the underlying Java method or constructor.
     * <p>
     * Implements Function.call.
     *
     * @see org.mozilla.javascript.Function#call
     * @exception JavaScriptException if the underlying Java method or constructor
     *            threw an exception
     */
    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        if (parmsLength < 0)
            return callVarargs(cx, thisObj, args, false);

        if (!isStatic) {
            // OPT: cache "clazz"?
            Class clazz = method != null ? method.getDeclaringClass()
                                         : ctor.getDeclaringClass();
            if (thisObj == null || !clazz.isInstance(thisObj)) {
                Object[] errArgs = { names[0] };
                throw Context.reportRuntimeError(
                    Context.getMessage("msg.incompat.call", errArgs));
            }
        }
        Object[] invokeArgs;
        int i;
        if (parmsLength == args.length) {
            invokeArgs = args;
            // avoid copy loop if no conversions needed
            i = (types == null) ? parmsLength : 0;
        } else {
            invokeArgs = new Object[parmsLength];
            i = 0;
        }
        for (; i < parmsLength; i++) {
            Object arg = (i < args.length)
                         ? args[i]
                         : Undefined.instance;
            if (types != null) {
                Class desired = types[i];
                if (desired == ScriptRuntime.BooleanClass 
                                            || desired == Boolean.TYPE)
                    arg = ScriptRuntime.toBoolean(arg) ? Boolean.TRUE 
                                                       : Boolean.FALSE;
                else if (desired == ScriptRuntime.StringClass)
                    arg = ScriptRuntime.toString(arg);
                else if (desired == ScriptRuntime.IntegerClass || desired == Integer.TYPE)
                    arg = new Integer(ScriptRuntime.toInt32(arg));
                else if (desired == ScriptRuntime.DoubleClass || desired == Double.TYPE)
                    arg = new Double(ScriptRuntime.toNumber(arg));
                else if (desired == ScriptRuntime.ScriptableClass)
                    arg = ScriptRuntime.toObject(this, arg);
                else {
                    Object[] errArgs = { desired.getName() };
                    throw Context.reportRuntimeError(
                            Context.getMessage("msg.cant.convert", errArgs));
                }
            }
            invokeArgs[i] = arg;
        }
        try {
            Object result = (method != null)
                            ? method.invoke(thisObj, invokeArgs)
                            : ctor.newInstance(invokeArgs);
            return hasVoidReturn ? Undefined.instance : result;
        }
        catch (InvocationTargetException e) {
            throw JavaScriptException.wrapException(scope, e);
        }
        catch (IllegalAccessException e) {
            throw WrappedException.wrapException(e);
        }
        catch (InstantiationException e) {
            throw WrappedException.wrapException(e);
        }
    }

    /**
     * Performs conversions on argument types if needed and
     * invokes the underlying Java method or constructor
     * to create a new Scriptable object.
     * <p>
     * Implements Function.construct.
     *
     * @param cx the current Context for this thread
     * @param scope the scope to execute the function relative to. This
     *              set to the value returned by getParentScope() except
     *              when the function is called from a closure.
     * @param args arguments to the constructor
     * @see org.mozilla.javascript.Function#construct
     * @exception JavaScriptException if the underlying Java method or constructor
     *            threw an exception
     */
    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        if (method == null || parmsLength == VARARGS_CTOR) {
            Scriptable result = method != null
                ? (Scriptable) callVarargs(cx, null, args, true)
                : (Scriptable) call(cx, scope, null, args);

            if (result.getPrototype() == null)
                result.setPrototype(getClassPrototype());
            if (result.getParentScope() == null) {
                Scriptable parent = getParentScope();
                if (result != parent)
                    result.setParentScope(parent);
            }

            return result;
        }

        return super.construct(cx, scope, args);
    }

    private Object callVarargs(Context cx, Scriptable thisObj, Object[] args,
                               boolean inNewExpr)
        throws JavaScriptException
    {
        try {
            if (parmsLength == VARARGS_METHOD) {
                Object[] invokeArgs = { cx, thisObj, args, this };
                Object result = method.invoke(null, invokeArgs);
                return hasVoidReturn ? Undefined.instance : result;
            } else {
                Boolean b = inNewExpr ? Boolean.TRUE : Boolean.FALSE;
                Object[] invokeArgs = { cx, args, this, b };
                return (method == null)
                       ? ctor.newInstance(invokeArgs)
                       : method.invoke(null, invokeArgs);
            }
        }
        catch (InvocationTargetException e) {
            Throwable target = e.getTargetException();
            if (target instanceof EvaluatorException)
                throw (EvaluatorException) target;
            Scriptable scope = thisObj == null ? this : thisObj;
            throw JavaScriptException.wrapException(scope, target);
        }
        catch (IllegalAccessException e) {
            throw WrappedException.wrapException(e);
        }
        catch (InstantiationException e) {
            throw WrappedException.wrapException(e);
        }
    }

    private static final short VARARGS_METHOD = -1;
    private static final short VARARGS_CTOR =   -2;

    Method method;
    Constructor ctor;
    private Class[] types;
    private short parmsLength;
    private short lengthPropertyValue;
    private boolean hasVoidReturn;
    private boolean isStatic;
}
