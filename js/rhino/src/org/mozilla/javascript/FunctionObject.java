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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Igor Bukanov
 * David C. Navas
 * Ted Neward
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

// API class

package org.mozilla.javascript;

import java.util.Vector;
import java.lang.reflect.Constructor;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.InvocationTargetException;

public class FunctionObject extends NativeFunction {

    static final long serialVersionUID = -4074285335521944312L;

    /**
     * Create a JavaScript function object from a Java method.
     *
     * <p>The <code>member</code> argument must be either a java.lang.reflect.Method
     * or a java.lang.reflect.Constructor and must match one of two forms.<p>
     *
     * The first form is a member with zero or more parameters
     * of the following types: Object, String, boolean, Scriptable,
     * byte, short, int, float, or double. The Long type is not supported
     * because the double representation of a long (which is the 
     * EMCA-mandated storage type for Numbers) may lose precision.
     * If the member is a Method, the return value must be void or one 
     * of the types allowed for parameters.<p>
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
     *      (Context cx, Object[] args, Function ctorObj,
     *       boolean inNewExpr)</pre>
     * and if it is a Method, be static and return an Object result.<p>
     *
     * Otherwise, if the FunctionObject will <i>not</i> be used to define a
     * constructor, the member must be a static Method with parameters
     *      (Context cx, Scriptable thisObj, Object[] args,
     *       Function funObj) </pre>
     * <pre>
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
        this.functionName = name;
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
                    throw Context.reportRuntimeError1(
                        "msg.varargs.ctor", methodName);
                }
                parmsLength = VARARGS_CTOR;
            } else {
                if (!isStatic ||
                    types[0] != Context.class ||
                    types[1] != ScriptRuntime.ScriptableClass ||
                    types[2].getComponentType() != ScriptRuntime.ObjectClass ||
                    types[3] != ScriptRuntime.FunctionClass)
                {
                    throw Context.reportRuntimeError1(
                        "msg.varargs.fun", methodName);
                }
                parmsLength = VARARGS_METHOD;
            }
            // XXX check return type
            length = 1;
        } else {
            parmsLength = (short) types.length;
            for (int i=0; i < parmsLength; i++) {
                Class type = types[i];
                if (type != ScriptRuntime.ObjectClass &&
                    type != ScriptRuntime.StringClass &&
                    type != ScriptRuntime.BooleanClass &&
                    !ScriptRuntime.NumberClass.isAssignableFrom(type) &&
                    !Scriptable.class.isAssignableFrom(type) &&
                    type != Boolean.TYPE &&
                    type != Byte.TYPE &&
                    type != Short.TYPE &&
                    type != Integer.TYPE &&
                    type != Float.TYPE && 
                    type != Double.TYPE)
                {
                    // Note that long is not supported.
                    throw Context.reportRuntimeError1("msg.bad.parms", 
                                                      methodName);
                }
            }
            length = parmsLength;
        }

        // Initialize length property
        lengthPropertyValue = (short) length;

        hasVoidReturn = method != null && method.getReturnType() == Void.TYPE;
        this.argCount = (short) length;

        setParentScope(scope);
        setPrototype(getFunctionPrototype(scope));
        Context cx = Context.getCurrentContext();
        useDynamicScope = cx != null && 
                          cx.hasCompileFunctionsWithDynamicScope();
    }

    /**
     * Return the value defined by  the method used to construct the object
     * (number of parameters of the method, or 1 if the method is a "varargs"
     * form), unless setLength has been called with a new value.
     * Overrides getLength in BaseFunction.
     *
     * @see org.mozilla.javascript.FunctionObject#setLength
     * @see org.mozilla.javascript.BaseFunction#getLength
     */
    public int getLength() {
        return lengthPropertyValue;
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
        return findMethods(getMethodList(clazz), name);
    }
    
    static Method[] findMethods(Method[] methods, String name) {
        // Usually we're just looking for a single method, so optimize
        // for that case.
        Vector v = null;
        Method first = null;
        for (int i=0; i < methods.length; i++) {
            if (methods[i] == null)
                continue;
            if (methods[i].getName().equals(name)) {
                if (first == null) {
                    first = methods[i];
                } else {
                    if (v == null) {
                        v = new Vector(5);
                        v.addElement(first);
                    }
                    v.addElement(methods[i]);
                }
            }
        }
        if (v == null) {
            if (first == null)
                return null;
            Method[] single = { first };
            return single;
        }
        Method[] result = new Method[v.size()];
        v.copyInto(result);
        return result;
    }

    static Method[] getMethodList(Class clazz) {
        Method[] cached = methodsCache; // get once to avoid synchronization
        if (cached != null && cached[0].getDeclaringClass() == clazz)
            return cached;
        Method[] methods = null;
        try {
            // getDeclaredMethods may be rejected by the security manager
            // but getMethods is more expensive
            if (!sawSecurityException) 
                methods = clazz.getDeclaredMethods();
        } catch (SecurityException e) {
            // If we get an exception once, give up on getDeclaredMethods
            sawSecurityException = true;
        }
        if (methods == null) {
            methods = clazz.getMethods();
        }
        int count = 0;
        for (int i=0; i < methods.length; i++) {
            if (sawSecurityException 
                ? methods[i].getDeclaringClass() != clazz
                : !Modifier.isPublic(methods[i].getModifiers()))
            {
                methods[i] = null;
            } else {
                count++;
            }
        }
        Method[] result = new Method[count];
        int j=0;
        for (int i=0; i < methods.length; i++) {
            if (methods[i] != null)
                result[j++] = methods[i];
        }
        if (result.length > 0 && Context.isCachingEnabled)
            methodsCache = result;
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
        setImmunePrototypeProperty(prototype);

        prototype.setParentScope(this);
        
        final int attr = ScriptableObject.DONTENUM  |
                         ScriptableObject.PERMANENT |
                         ScriptableObject.READONLY;
        defineProperty(prototype, "constructor", this, attr);

        String name = prototype.getClassName();
        defineProperty(scope, name, this, ScriptableObject.DONTENUM);

        setParentScope(scope);
    }

    static public Object convertArg(Scriptable scope,
                                    Object arg, Class desired)
    {
        if (desired == ScriptRuntime.StringClass) 
            return ScriptRuntime.toString(arg);
        if (desired == ScriptRuntime.IntegerClass || 
            desired == Integer.TYPE)
        {
            return new Integer(ScriptRuntime.toInt32(arg));
        }
        if (desired == ScriptRuntime.BooleanClass || 
            desired == Boolean.TYPE)
        {
            return ScriptRuntime.toBoolean(arg) ? Boolean.TRUE 
                                                : Boolean.FALSE;
        }
        if (desired == ScriptRuntime.DoubleClass || 
            desired == Double.TYPE)
        {
            return new Double(ScriptRuntime.toNumber(arg));
        }
        if (desired == ScriptRuntime.ScriptableClass)
            return ScriptRuntime.toObject(scope, arg);
        if (desired == ScriptRuntime.ObjectClass)
            return arg;
        
        // Note that the long type is not supported; see the javadoc for
        // the constructor for this class
        throw Context.reportRuntimeError1
            ("msg.cant.convert", desired.getName());
    }

    /**
     * Performs conversions on argument types if needed and
     * invokes the underlying Java method or constructor.
     * <p>
     * Implements Function.call.
     *
     * @see org.mozilla.javascript.Function#call
     * @exception JavaScriptException if the underlying Java method or 
     *            constructor threw an exception
     */
    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        if (parmsLength < 0) {
            return callVarargs(cx, thisObj, args, false);
        }
        if (!isStatic) {
            // OPT: cache "clazz"?
            Class clazz = method != null ? method.getDeclaringClass()
                                         : ctor.getDeclaringClass();
            while (!clazz.isInstance(thisObj)) {
                thisObj = thisObj.getPrototype();
                if (thisObj == null || !useDynamicScope) {
                    // Couldn't find an object to call this on.
                    throw NativeGlobal.typeError1
                        ("msg.incompat.call", functionName, scope);
                }
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
                arg = convertArg(this, arg, types[i]);
            }
            invokeArgs[i] = arg;
        }
        try {
            Object result = method == null ? ctor.newInstance(invokeArgs)
                                           : doInvoke(thisObj, invokeArgs);
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
            Scriptable result;
            if (method != null) {
                result = (Scriptable) callVarargs(cx, null, args, true);
            } else {
                result = (Scriptable) call(cx, scope, null, args);
            }

            if (result.getPrototype() == null)
                result.setPrototype(getClassPrototype());
            if (result.getParentScope() == null) {
                Scriptable parent = getParentScope();
                if (result != parent)
                    result.setParentScope(parent);
            }

            return result;
        } else if (method != null && !isStatic) {
            Scriptable result;
            try {
                result = (Scriptable) method.getDeclaringClass().newInstance();
            } catch (IllegalAccessException e) {
                throw WrappedException.wrapException(e);
            } catch (InstantiationException e) {
                throw WrappedException.wrapException(e);
            }

            result.setPrototype(getClassPrototype());
            result.setParentScope(getParentScope());

            Object val = call(cx, scope, result, args);
            if (val != null && val != Undefined.instance &&
                val instanceof Scriptable)
            {
                return (Scriptable) val;
            }
            return result;
        }

        return super.construct(cx, scope, args);
    }
        
    private final Object doInvoke(Object thisObj, Object[] args) 
        throws IllegalAccessException, InvocationTargetException
    {
        Invoker master = invokerMaster;
        if (master != null) {
            if (invoker == null) {
                invoker = master.createInvoker(method, types);
            }
            try {
                return invoker.invoke(thisObj, args);
            } catch (Exception e) {
                throw new InvocationTargetException(e);
            }
        } 
        return method.invoke(thisObj, args);
    }

    private Object callVarargs(Context cx, Scriptable thisObj, Object[] args,
                               boolean inNewExpr)
        throws JavaScriptException
    {
        try {
            if (parmsLength == VARARGS_METHOD) {
                Object[] invokeArgs = { cx, thisObj, args, this };
                Object result = doInvoke(null, invokeArgs);
                return hasVoidReturn ? Undefined.instance : result;
            } else {
                Boolean b = inNewExpr ? Boolean.TRUE : Boolean.FALSE;
                Object[] invokeArgs = { cx, args, this, b };
                return (method == null)
                       ? ctor.newInstance(invokeArgs)
                       : doInvoke(null, invokeArgs);
            }
        }
        catch (InvocationTargetException e) {
            Throwable target = e.getTargetException();
            if (target instanceof EvaluatorException)
                throw (EvaluatorException) target;
            if (target instanceof EcmaError)
                throw (EcmaError) target;
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
    
    boolean isVarArgsMethod() { 
        return parmsLength == VARARGS_METHOD;
    }

    boolean isVarArgsConstructor() { 
        return parmsLength == VARARGS_CTOR;
    }
    
    static void setCachingEnabled(boolean enabled) {
        if (!enabled) {
            methodsCache = null;
            invokerMaster = null;
        } else if (invokerMaster == null) {
            invokerMaster = newInvokerMaster();
        }
    }

    /** Get default master implementation or null if not available */
    private static Invoker newInvokerMaster() {
        try {
            Class cl = ScriptRuntime.loadClassName(INVOKER_MASTER_CLASS);
            return (Invoker)cl.newInstance();
        }
        catch (ClassNotFoundException ex) {}
        catch (IllegalAccessException ex) {}
        catch (InstantiationException ex) {}
        catch (SecurityException ex) {}
        return null;
    }

    private static final String 
        INVOKER_MASTER_CLASS = "org.mozilla.javascript.optimizer.InvokerImpl";

    static Invoker invokerMaster = newInvokerMaster();
    
    private static final short VARARGS_METHOD = -1;
    private static final short VARARGS_CTOR =   -2;
    
    private static boolean sawSecurityException;

    static Method[] methodsCache;

    Method method;
    Constructor ctor;
    private Class[] types;
    Invoker invoker;
    private short parmsLength;
    private short lengthPropertyValue;
    private boolean hasVoidReturn;
    private boolean isStatic;
    private boolean useDynamicScope;
}
