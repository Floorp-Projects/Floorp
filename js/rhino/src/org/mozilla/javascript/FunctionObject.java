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

import java.util.Hashtable;
import java.util.Vector;
import java.lang.reflect.*;
import java.io.IOException;
import org.mozilla.classfile.*;

public class FunctionObject extends NativeFunction {

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
            // Ugly: allow variable-arg constructors that need access to the 
            // scope to get it from the Context. Cleanest solution would be
            // to modify the varargs form, but that would require users with 
            // the old form to change their code.
            cx.ctorScope = scope;
            Object result = callVarargs(cx, thisObj, args, false);
            cx.ctorScope = null;
            return result;
        }
        if (!isStatic) {
            // OPT: cache "clazz"?
            Class clazz = method != null ? method.getDeclaringClass()
                                         : ctor.getDeclaringClass();
            while (!clazz.isInstance(thisObj)) {
                thisObj = thisObj.getPrototype();
                if (thisObj == null || !useDynamicScope) {
                    // Couldn't find an object to call this on.
                    String msg = Context.getMessage1
                        ("msg.incompat.call", names[0]);
                    throw NativeGlobal.constructError(cx, "TypeError", msg, scope);
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
                // Ugly: allow variable-arg constructors that need access to the 
                // scope to get it from the Context. Cleanest solution would be
                // to modify the varargs form, but that would require users with 
                // the old form to change their code.
                cx.ctorScope = scope;
                result = (Scriptable) callVarargs(cx, null, args, true);
                cx.ctorScope = null;
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
    
    public abstract static class Invoker {
        static int classNumber = 0;

        public static Invoker createInvoker(Method method, Class[] types,
                                            JavaAdapter.DefiningClassLoader classLoader)
        {
            String className;
            Invoker result = null;
            
            //synchronized(invokers) { // is "++" atomic?
                className = "inv" + ++classNumber;
            //}
            
            ClassFileWriter cfw = new ClassFileWriter(className, 
                                    "org.mozilla.javascript.FunctionObject$Invoker", "");
            cfw.setFlags((short)(ClassFileWriter.ACC_PUBLIC | 
                                 ClassFileWriter.ACC_FINAL));
            
            // Add our instantiator!
            cfw.startMethod("<init>", "()V", ClassFileWriter.ACC_PUBLIC);
            cfw.add(ByteCode.ALOAD_0);
            cfw.add(ByteCode.INVOKESPECIAL,
                    "org.mozilla.javascript.FunctionObject$Invoker",
                    "<init>", "()", "V");
            cfw.add(ByteCode.RETURN);
            cfw.stopMethod((short)1, null); // one argument -- this???

            // Add the invoke() method call
            cfw.startMethod("invoke", 
                            "(Ljava/lang/Object;[Ljava/lang/Object;)"+
                             "Ljava/lang/Object;",
                            (short)(ClassFileWriter.ACC_PUBLIC | 
                                    ClassFileWriter.ACC_FINAL));
            
            // If we return a primitive type, then do something special!
            String declaringClassName = method.getDeclaringClass().getName
                ().replace('.', '/');
            Class returnType = method.getReturnType();
            String invokeSpecial = null;
            String invokeSpecialType = null;
            boolean returnsVoid = false;
            boolean returnsBoolean = false;
            
            if (returnType.isPrimitive()) {
                if (returnType == Boolean.TYPE) {
                    returnsBoolean = true;
                    invokeSpecialType = "(Z)";
                } else if (returnType == Void.TYPE) {
                    returnsVoid = true;
                    invokeSpecialType = "(V)";
                } else if (returnType == Integer.TYPE) {
                    cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Integer");
                    cfw.add(ByteCode.DUP);
                    invokeSpecialType = "(I)";
                } else if (returnType == Long.TYPE) {
                    cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Long");
                    cfw.add(ByteCode.DUP);
                    invokeSpecialType = "(J)";
                } else if (returnType == Short.TYPE) {
                    cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Short");
                    cfw.add(ByteCode.DUP);
                    invokeSpecialType = "(S)";
                } else if (returnType == Float.TYPE) {
                    cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Float");
                    cfw.add(ByteCode.DUP);
                    invokeSpecialType = "(F)";
                } else if (returnType == Double.TYPE) {
                    cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Double");
                    cfw.add(ByteCode.DUP);
                    invokeSpecialType = "(D)";
                } else if (returnType == Byte.TYPE) {
                    cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Byte");
                    cfw.add(ByteCode.DUP);
                    invokeSpecialType = "(B)";
                } else if (returnType == Character.TYPE) {
                    cfw.add(ByteCode.NEW, invokeSpecial 
                                = "java/lang/Character");
                    cfw.add(ByteCode.DUP);
                    invokeSpecialType = "(C)";
                }
            }
            
            // handle setup of call to virtual function (if calling non-static)
            if (!java.lang.reflect.Modifier.isStatic(method.getModifiers())) {
                cfw.add(ByteCode.ALOAD_1);
                cfw.add(ByteCode.CHECKCAST, declaringClassName);
            }
            
            // Handle parameters!
            StringBuffer params = new StringBuffer(2 + ((types!=null)?(20 *
                                    types.length):0));
            
            params.append("(");
            if (types != null) {
                for(int i = 0; i < types.length; i++) {
                    Class type = types[i];

                    cfw.add(ByteCode.ALOAD_2);
                    
                    if (i <= 5) {
                        cfw.add((byte) (ByteCode.ICONST_0 + i));
                    } else if (i <= Byte.MAX_VALUE) {
                        cfw.add(ByteCode.BIPUSH, i);
                    } else if (i <= Short.MAX_VALUE) {
                        cfw.add(ByteCode.SIPUSH, i);
                    } else {
                        cfw.addLoadConstant((int)i);
                    }
                    
                    cfw.add(ByteCode.AALOAD);
                    
                    if (type.isPrimitive()) {
                        // Convert enclosed type back to primitive.
                        
                        if (type == Boolean.TYPE) {
                            cfw.add(ByteCode.CHECKCAST, "java/lang/Boolean");
                            cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Boolean", 
                                    "booleanValue", "()", "Z");
                            params.append("Z");
                        } else if (type == Integer.TYPE) {
                            cfw.add(ByteCode.CHECKCAST, "java/lang/Number");
                            cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Number", 
                                    "intValue", "()", "I");
                            params.append("I");
                        } else if (type == Short.TYPE) {
                            cfw.add(ByteCode.CHECKCAST, "java/lang/Number");
                            cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Number", 
                                    "shortValue", "()", "S");
                            params.append("S");
                        } else if (type == Character.TYPE) {
                            cfw.add(ByteCode.CHECKCAST, "java/lang/Character");
                            cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Character", 
                                    "charValue", "()", "C");
                            params.append("C");
                        } else if (type == Double.TYPE) {
                            cfw.add(ByteCode.CHECKCAST, "java/lang/Number");
                            cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Number", 
                                    "doubleValue", "()", "D");
                            params.append("D");
                        } else if (type == Float.TYPE) {
                            cfw.add(ByteCode.CHECKCAST, "java/lang/Number");
                            cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Number", 
                                    "floatValue", "()", "F");
                            params.append("F");
                        } else if (type == Byte.TYPE) {
                            cfw.add(ByteCode.CHECKCAST, "java/lang/Byte");
                            cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Byte", 
                                    "byteValue", "()", "B");
                            params.append("B");
                        }
                    } else {
                        String typeName = type.getName().replace('.', '/');
                        cfw.add(ByteCode.CHECKCAST, typeName);
                        
                        if (!type.isArray()) {
                            params.append('L');
                        }
                        params.append(typeName);
                        
                        if (!type.isArray()) {
                            params.append(';');
                        }
                    }
                }
            }
            params.append(")");
            
            // Call actual function!
            if (!java.lang.reflect.Modifier.isStatic(method.getModifiers())) {
                cfw.add(ByteCode.INVOKEVIRTUAL, declaringClassName, 
                        method.getName(), params.toString(),
                        (invokeSpecialType!=null?invokeSpecialType.substring(1,2)
                                                 :returnType.isArray()?
                                                  returnType.getName().replace('.','/')
                                                  :"L".concat
                    (returnType.getName().replace('.', '/').concat(";"))));
            } else {
                cfw.add(ByteCode.INVOKESTATIC, declaringClassName, 
                        method.getName(), params.toString(),
                        (invokeSpecialType!=null?invokeSpecialType.substring(1,2)
                                                 :returnType.isArray()?
                                                  returnType.getName().replace('.','/')
                                                  :"L".concat
                    (returnType.getName().replace('.', '/').concat(";"))));
            }
            
            // Handle return value
            if (returnsVoid) {
                cfw.add(ByteCode.ACONST_NULL);
                cfw.add(ByteCode.ARETURN);
            } else if (returnsBoolean) {
                // HACK
                //check to see if true;
                // '7' is the number of bytes of the ifeq<branch> plus getstatic<TRUE> plus areturn instructions
                cfw.add(ByteCode.IFEQ, 7);
                cfw.add(ByteCode.GETSTATIC,
                        "java/lang/Boolean",
                        "TRUE",
                        "Ljava/lang/Boolean;");
                cfw.add(ByteCode.ARETURN);
                cfw.add(ByteCode.GETSTATIC,
                        "java/lang/Boolean",
                        "FALSE",
                        "Ljava/lang/Boolean;");
                cfw.add(ByteCode.ARETURN);
            } else if (invokeSpecial != null) {
                cfw.add(ByteCode.INVOKESPECIAL,
                        invokeSpecial,
                        "<init>", invokeSpecialType, "V");
                cfw.add(ByteCode.ARETURN);
            } else {
                cfw.add(ByteCode.ARETURN);
            }
            cfw.stopMethod((short)3, null); // three arguments, including the this pointer???
            
            // Add class to our classloader.
            java.io.ByteArrayOutputStream bos = 
                new java.io.ByteArrayOutputStream(550);
            
            try {
                cfw.write(bos);
            }
            catch (IOException ioe) {
                throw new RuntimeException("unexpected IOException" + ioe.toString());
            }
            
            try {
                byte[] bytes = bos.toByteArray();
                classLoader.defineClass(className, bytes);
                Class clazz = classLoader.loadClass(className, true);
                result = (Invoker)clazz.newInstance();
                
                if (false) {
                    System.out.println("Generated method delegate for: " + method.getName() 
                                       + " on " + method.getDeclaringClass().getName() + " :: " + params.toString() 
                                       + " :: " + types);
                }
            } catch (ClassNotFoundException e) {
                throw new RuntimeException("unexpected " + e.toString());
            } catch (InstantiationException e) {
                throw new RuntimeException("unexpected " + e.toString());
            } catch (IllegalAccessException e) {
                throw new RuntimeException("unexpected " + e.toString());
            }

            return result;
        }
        
        public abstract Object invoke(Object that, Object [] args);
    }
    
    private final Object doInvoke(Object thisObj, Object[] args) 
        throws IllegalAccessException, InvocationTargetException
    {
        if (classLoader != null) {
            if (invoker == null) {
                invoker = (Invoker) invokersCache.get(method);
                if (invoker == null) {
                    invoker = Invoker.createInvoker(method, types, classLoader);
                    invokersCache.put(method, invoker);
                }
            }
            return invoker.invoke(thisObj, args);
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
            classLoader = null;
            invokersCache = null;
        } else if (classLoader == null) {
            classLoader = JavaAdapter.createDefiningClassLoader();
            invokersCache = new Hashtable();
        }
    }

    private static final short VARARGS_METHOD = -1;
    private static final short VARARGS_CTOR =   -2;
    
    private static boolean sawSecurityException;

    static Method[] methodsCache;
    static Hashtable invokersCache = new Hashtable();
    static JavaAdapter.DefiningClassLoader classLoader 
        = JavaAdapter.createDefiningClassLoader();

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
