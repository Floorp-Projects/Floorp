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

import java.lang.reflect.*;
import java.io.*;

public class FunctionObject extends BaseFunction
{

    static final long serialVersionUID = -4074285335521944312L;

    /**
     * Create a JavaScript function object from a Java method.
     *
     * <p>The <code>member</code> argument must be either a java.lang.reflect.Method
     * or a java.lang.reflect.Constructor and must match one of two forms.<p>
     *
     * The first form is a member with zero or more parameters
     * of the following types: Object, String, boolean, Scriptable,
     * int, or double. The Long type is not supported
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
        ClassCache cache = ClassCache.get(scope);
        if (methodOrConstructor instanceof Constructor) {
            member = new MemberBox((Constructor) methodOrConstructor, cache);
            isStatic = true; // well, doesn't take a 'this'
        } else {
            member = new MemberBox((Method) methodOrConstructor, cache);
            isStatic = member.isStatic();
        }
        String methodName = member.getName();
        this.functionName = name;
        Class[] types = member.argTypes;
        int arity = types.length;
        if (arity == 4 && (types[1].isArray() || types[2].isArray())) {
            // Either variable args or an error.
            if (types[1].isArray()) {
                if (!isStatic ||
                    types[0] != ScriptRuntime.ContextClass ||
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
                    types[0] != ScriptRuntime.ContextClass ||
                    types[1] != ScriptRuntime.ScriptableClass ||
                    types[2].getComponentType() != ScriptRuntime.ObjectClass ||
                    types[3] != ScriptRuntime.FunctionClass)
                {
                    throw Context.reportRuntimeError1(
                        "msg.varargs.fun", methodName);
                }
                parmsLength = VARARGS_METHOD;
            }
        } else {
            parmsLength = arity;
            if (arity > 0) {
                typeTags = new byte[arity];
                for (int i = 0; i != arity; ++i) {
                    int tag = getTypeTag(types[i]);
                    if (tag == JAVA_UNSUPPORTED_TYPE) {
                        throw Context.reportRuntimeError2(
                            "msg.bad.parms", types[i].getName(), methodName);
                    }
                    typeTags[i] = (byte)tag;
                }
            }
        }

        if (member.isMethod()) {
            Method method = member.method();
            Class returnType = method.getReturnType();
            if (returnType == Void.TYPE) {
                hasVoidReturn = true;
            } else {
                int returnTypeTag = getTypeTag(returnType);
                if (returnTypeTag == JAVA_UNSUPPORTED_TYPE) {
                    throw Context.reportRuntimeError2(
                        "msg.bad.method.return",
                        returnType.getName(), methodName);
                }
            }
            member.prepareInvokerOptimization();
        } else {
            Class ctorType = member.getDeclaringClass();
            if (!ScriptRuntime.ScriptableClass.isAssignableFrom(ctorType)) {
                throw Context.reportRuntimeError1(
                    "msg.bad.ctor.return", ctorType.getName());
            }
        }

        ScriptRuntime.setFunctionProtoAndParent(scope, this);
    }

    /**
     * @return One of <tt>JAVA_*_TYPE</tt> constants to indicate desired type
     *         or {@link #JAVA_UNSUPPORTED_TYPE} if the convertion is not
     *         possible
     */
    public static int getTypeTag(Class type)
    {
        if (type == ScriptRuntime.StringClass)
            return JAVA_STRING_TYPE;
        if (type == ScriptRuntime.IntegerClass || type == Integer.TYPE)
            return JAVA_INT_TYPE;
        if (type == ScriptRuntime.BooleanClass || type == Boolean.TYPE)
            return JAVA_BOOLEAN_TYPE;
        if (type == ScriptRuntime.DoubleClass || type == Double.TYPE)
            return JAVA_DOUBLE_TYPE;
        if (ScriptRuntime.ScriptableClass.isAssignableFrom(type))
            return JAVA_SCRIPTABLE_TYPE;
        if (type == ScriptRuntime.ObjectClass)
            return JAVA_OBJECT_TYPE;

        // Note that the long type is not supported; see the javadoc for
        // the constructor for this class

        return JAVA_UNSUPPORTED_TYPE;
    }

    public static Object convertArg(Context cx, Scriptable scope,
                                    Object arg, int typeTag)
    {
        switch (typeTag) {
          case JAVA_STRING_TYPE:
              if (arg instanceof String)
                return arg;
            return ScriptRuntime.toString(arg);
          case JAVA_INT_TYPE:
              if (arg instanceof Integer)
                return arg;
            return new Integer(ScriptRuntime.toInt32(arg));
          case JAVA_BOOLEAN_TYPE:
              if (arg instanceof Boolean)
                return arg;
            return ScriptRuntime.toBoolean(arg) ? Boolean.TRUE
                                                : Boolean.FALSE;
          case JAVA_DOUBLE_TYPE:
            if (arg instanceof Double)
                return arg;
            return new Double(ScriptRuntime.toNumber(arg));
          case JAVA_SCRIPTABLE_TYPE:
            if (arg instanceof Scriptable)
                return arg;
            return ScriptRuntime.toObject(cx, scope, arg);
          case JAVA_OBJECT_TYPE:
            return arg;
          default:
            throw new IllegalArgumentException();
        }
    }

    /**
     * Return the value defined by  the method used to construct the object
     * (number of parameters of the method, or 1 if the method is a "varargs"
     * form).
     */
    public int getArity() {
        return parmsLength < 0 ? 1 : parmsLength;
    }

    /**
     * Return the same value as {@link #getArity()}.
     */
    public int getLength() {
        return getArity();
    }

    /**
     * Get Java method or constructor this function represent.
     */
    public Member getMethodOrConstructor()
    {
        if (member.isMethod()) {
            return member.method();
        } else {
            return member.ctor();
        }
    }

    static Method findSingleMethod(Method[] methods, String name)
    {
        Method found = null;
        for (int i = 0, N = methods.length; i != N; ++i) {
            Method method = methods[i];
            if (method != null && name.equals(method.getName())) {
                if (found != null) {
                    throw Context.reportRuntimeError2(
                        "msg.no.overload", name,
                        method.getDeclaringClass().getName());
                }
                found = method;
            }
        }
        return found;
    }

    static Method[] getMethodList(Class clazz) {
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
        ScriptRuntime.setFunctionProtoAndParent(scope, this);
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

    /**
     * @deprecated Use {@link #getTypeTag(Class)}
     * and {@link #convertArg(Context, Scriptable, Object, int)}
     * for type convertion.
     */
    public static Object convertArg(Context cx, Scriptable scope,
                                    Object arg, Class desired)
    {
        int tag = getTypeTag(desired);
        if (tag == JAVA_UNSUPPORTED_TYPE) {
            throw Context.reportRuntimeError1
                ("msg.cant.convert", desired.getName());
        }
        return convertArg(cx, scope, arg, tag);
    }

    /**
     * Performs conversions on argument types if needed and
     * invokes the underlying Java method or constructor.
     * <p>
     * Implements Function.call.
     *
     * @see org.mozilla.javascript.Function#call(
     *          Context, Scriptable, Scriptable, Object[])
     */
    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        if (parmsLength < 0) {
            return callVarargs(cx, thisObj, args);
        }
        if (!isStatic) {
            Class clazz = member.getDeclaringClass();
            if (!clazz.isInstance(thisObj)) {
                boolean compatible = false;
                if (thisObj == scope) {
                    Scriptable parentScope = getParentScope();
                    if (scope != parentScope) {
                        // Call with dynamic scope for standalone function,
                        // use parentScope as thisObj
                        compatible = clazz.isInstance(parentScope);
                        if (compatible) {
                            thisObj = parentScope;
                        }
                    }
                }
                if (!compatible) {
                    // Couldn't find an object to call this on.
                    throw ScriptRuntime.typeError1("msg.incompat.call",
                                                   functionName);
                }
            }
        }

        Object[] invokeArgs;
        if (parmsLength == args.length) {
            // Do not allocate new argument array if java arguments are
            // the same as the original js ones.
            invokeArgs = args;
            for (int i = 0; i != parmsLength; ++i) {
                Object arg = args[i];
                Object converted = convertArg(cx, scope, arg, typeTags[i]);
                if (arg != converted) {
                    if (invokeArgs == args) {
                        invokeArgs = (Object[])args.clone();
                    }
                    invokeArgs[i] = converted;
                }
            }
        } else if (parmsLength == 0) {
            invokeArgs = ScriptRuntime.emptyArgs;
        } else {
            invokeArgs = new Object[parmsLength];
            for (int i = 0; i != parmsLength; ++i) {
                Object arg = (i < args.length)
                             ? args[i]
                             : Undefined.instance;
                invokeArgs[i] = convertArg(cx, scope, arg, typeTags[i]);
            }
        }

        Object result;
        if (member.isMethod()) {
            result = member.invoke(thisObj, invokeArgs);
        } else {
            result = member.newInstance(invokeArgs);
        }

        return hasVoidReturn ? Undefined.instance : result;
    }

    /**
     * Return new {@link Scriptable} instance using the default
     * constructor for the class of the underlying Java method.
     * Return null to indicate that the call method should be used to create
     * new objects.
     */
    public Scriptable createObject(Context cx, Scriptable scope) {
        if (member.isCtor() || parmsLength == VARARGS_CTOR) {
            return null;
        }
        Scriptable result;
        try {
            result = (Scriptable) member.getDeclaringClass().newInstance();
        } catch (Exception ex) {
            throw Context.throwAsScriptRuntimeEx(ex);
        }

        result.setPrototype(getClassPrototype());
        result.setParentScope(getParentScope());
        return result;
    }

    private Object callVarargs(Context cx, Scriptable thisObj, Object[] args)
    {
        if (parmsLength == VARARGS_METHOD) {
            Object[] invokeArgs = { cx, thisObj, args, this };
            Object result = member.invoke(null, invokeArgs);
            return hasVoidReturn ? Undefined.instance : result;
        } else {
            boolean inNewExpr = (thisObj == null);
            Boolean b = inNewExpr ? Boolean.TRUE : Boolean.FALSE;
            Object[] invokeArgs = { cx, args, this, b };
            return (member.isCtor())
                   ? member.newInstance(invokeArgs)
                   : member.invoke(null, invokeArgs);
        }
    }

    boolean isVarArgsMethod() {
        return parmsLength == VARARGS_METHOD;
    }

    boolean isVarArgsConstructor() {
        return parmsLength == VARARGS_CTOR;
    }

    private void readObject(ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
        in.defaultReadObject();
        if (parmsLength > 0) {
            Class[] types = member.argTypes;
            typeTags = new byte[parmsLength];
            for (int i = 0; i != parmsLength; ++i) {
                typeTags[i] = (byte)getTypeTag(types[i]);
            }
        }
    }

    private static final short VARARGS_METHOD = -1;
    private static final short VARARGS_CTOR =   -2;

    private static boolean sawSecurityException;

    public static final int JAVA_UNSUPPORTED_TYPE = 0;
    public static final int JAVA_STRING_TYPE      = 1;
    public static final int JAVA_INT_TYPE         = 2;
    public static final int JAVA_BOOLEAN_TYPE     = 3;
    public static final int JAVA_DOUBLE_TYPE      = 4;
    public static final int JAVA_SCRIPTABLE_TYPE  = 5;
    public static final int JAVA_OBJECT_TYPE      = 6;

    MemberBox member;
    transient private byte[] typeTags;
    private int parmsLength;
    private boolean hasVoidReturn;
    private boolean isStatic;
}
