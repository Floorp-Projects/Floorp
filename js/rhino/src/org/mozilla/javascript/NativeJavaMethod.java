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

/**
 * This class reflects Java methods into the JavaScript environment.  It
 * handles overloading of methods, and method/field name conflicts.
 * All NativeJavaMethods behave as JSRef `bound' methods, in that they
 * always operate on the object underlying the original NativeJavaObject
 * parent regardless of any reparenting that may occur.
 *
 * @author Mike Shaver
 * @see NativeJavaArray
 * @see NativeJavaPackage
 * @see NativeJavaClass
 */

public class NativeJavaMethod extends BaseFunction
{

    public NativeJavaMethod(Method[] methods)
    {
        this.methods = methods;
        this.functionName = methods[0].getName();
    }

    public NativeJavaMethod(Method method, String name)
    {
        this.methods = new Method[1];
        this.methods[0] = method;
        this.functionName = name;
    }

    public void add(Method method)
    {
        if (functionName == null) {
            functionName = method.getName();
        } else if (!functionName.equals(method.getName())) {
            throw new RuntimeException("internal method name mismatch");
        }
        // XXX a more intelligent growth algorithm would be nice
        int len = methods == null ? 0 : methods.length;
        Method[] newMeths = new Method[len + 1];
        for (int i = 0; i < len; i++)
            newMeths[i] = methods[i];
        newMeths[len] = method;
        methods = newMeths;
    }

    private static String scriptSignature(Object value)
    {
        if (value == null) {
            return "null";
        } else if (value instanceof Boolean) {
            return "boolean";
        } else if (value instanceof String) {
            return "string";
        } else if (value instanceof Number) {
            return "number";
        } else if (value instanceof Scriptable) {
            if (value instanceof Undefined) {
                return "undefined";
            } else if (value instanceof Wrapper) {
                Object wrapped = ((Wrapper)value).unwrap();
                return wrapped.getClass().getName();
            } else if (value instanceof Function) {
                return "function";
            } else {
                return "object";
            }
        } else {
            return javaSignature(value.getClass());
        }
    }

    static String scriptSignature(Object[] values)
    {
        StringBuffer sig = new StringBuffer();
        for (int i = 0; i < values.length; i++) {
            if (i != 0)
                sig.append(',');
            sig.append(scriptSignature(values[i]));
        }
        return sig.toString();
    }

    static String javaSignature(Class type)
    {
        if (type == null) {
            return "null";
        } else if (!type.isArray()) {
            return type.getName();
        } else {
            int arrayDimension = 0;
            do {
                ++arrayDimension;
                type = type.getComponentType();
            } while (type.isArray());
            String name = type.getName();
            String suffix = "[]";
            if (arrayDimension == 1) {
                return name.concat(suffix);
            } else {
                int length = name.length() + arrayDimension * suffix.length();
                StringBuffer sb = new StringBuffer(length);
                sb.append(name);
                while (arrayDimension != 0) {
                    --arrayDimension;
                    sb.append(suffix);
                }
                return sb.toString();
            }
        }
    }

    static String memberSignature(Member member)
    {
        String name;
        Class[] paramTypes;
        if (member instanceof Method) {
            Method method = (Method)member;
            name = method.getName();
            paramTypes = method.getParameterTypes();
        } else {
            Constructor ctor = (Constructor)member;
            name = "";
            paramTypes = ctor.getParameterTypes();
        }
        StringBuffer sb = new StringBuffer();
        sb.append(name);
        sb.append('(');
        for (int i = 0, N = paramTypes.length; i != N; ++i) {
            if (i != 0) {
                sb.append(',');
            }
            sb.append(javaSignature(paramTypes[i]));
        }
        sb.append(')');
        return sb.toString();
    }

    public String decompile(Context cx, int indent, boolean justbody)
    {
        StringBuffer sb = new StringBuffer();
        if (!justbody) {
            sb.append("function ");
            sb.append(getFunctionName());
            sb.append("() {");
        }
        sb.append("/*\n");
        toString(sb);
        sb.append(justbody ? "*/\n" : "*/}\n");
        return sb.toString();
    }

    public String toString()
    {
        StringBuffer sb = new StringBuffer();
        toString(sb);
        return sb.toString();
    }

    private void toString(StringBuffer sb)
    {
        for (int i = 0, N = methods.length; i != N; ++i) {
            Method method = methods[i];
            sb.append(javaSignature(method.getReturnType()));
            sb.append(' ');
            sb.append(memberSignature(method));
            sb.append('\n');
        }
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        // Find a method that matches the types given.
        if (methods.length == 0) {
            throw new RuntimeException("No methods defined for call");
        }
        int index = findFunction(methods, args);
        if (index < 0) {
            Class c = methods[0].getDeclaringClass();
            String sig = c.getName() + '.' + functionName + '(' +
                         scriptSignature(args) + ')';
            throw Context.reportRuntimeError1("msg.java.no_such_method", sig);
        }

        Method meth = methods[index];
        // OPT: already retrieved in findFunction, so we should inline that
        // OPT: or pass it back somehow
        Class paramTypes[] = meth.getParameterTypes();

        // First, we marshall the args.
        Object[] origArgs = args;
        for (int i = 0; i < args.length; i++) {
            Object arg = args[i];
            Object coerced = NativeJavaObject.coerceType(paramTypes[i], arg,
                                                         true);
            if (coerced != arg) {
                if (origArgs == args) {
                    args = (Object[])args.clone();
                }
                args[i] = coerced;
            }
        }
        Object javaObject;
        if (Modifier.isStatic(meth.getModifiers())) {
            javaObject = null;  // don't need an object
        } else {
            Scriptable o = thisObj;
            while (!(o instanceof Wrapper)) {
                o = o.getPrototype();
                if (o == null) {
                    throw Context.reportRuntimeError1(
                        "msg.nonjava.method", functionName);
                }
            }
            javaObject = ((Wrapper) o).unwrap();
        }
        if (debug) {
            printDebug("Calling ", meth, args);
        }

        Object retval;
        try {
            try {
                retval = meth.invoke(javaObject, args);
            } catch (IllegalAccessException e) {
                retval = retryIllegalAccessInvoke(meth, javaObject, args, e);
            }
        } catch (IllegalAccessException accessEx) {
            throw Context.reportRuntimeError(
                "While attempting to call \"" + meth.getName() +
                "\" in class \"" + meth.getDeclaringClass().getName() +
                "\" receieved " + accessEx.toString());
        } catch (Exception ex) {
            throw ScriptRuntime.throwAsUncheckedException(ex);
        }
        Class staticType = meth.getReturnType();

        if (debug) {
            Class actualType = (retval == null) ? null
                                                : retval.getClass();
            System.err.println(" ----- Returned " + retval +
                               " actual = " + actualType +
                               " expect = " + staticType);
        }

        Object wrapped = cx.getWrapFactory().wrap(cx, scope,
                                                  retval, staticType);
        if (debug) {
            Class actualType = (wrapped == null) ? null
                                                 : wrapped.getClass();
            System.err.println(" ----- Wrapped as " + wrapped +
                               " class = " + actualType);
        }

        if (wrapped == Undefined.instance)
            return wrapped;
        if (wrapped == null && staticType == Void.TYPE)
            return Undefined.instance;
        return wrapped;
    }

    static Object retryIllegalAccessInvoke(Method method, Object obj,
                                           Object[] args,
                                           IllegalAccessException illegalAccess)
        throws IllegalAccessException, InvocationTargetException
    {
        if (Modifier.isPublic(method.getModifiers())) {
            String name = method.getName();
            Class[] parms = method.getParameterTypes();
            Class c = method.getDeclaringClass();
            Class[] intfs = c.getInterfaces();
            for (int i=0; i < intfs.length; i++) {
                c = intfs[i];
                try {
                    Method m = c.getMethod(name, parms);
                    return m.invoke(obj, args);
                } catch (NoSuchMethodException ex) {
                    continue;
                } catch (IllegalAccessException ex) {
                    continue;
                }
            }
        }
        /**
         * Due to a bug in Sun's VM, public methods in private
         * classes are not accessible by default (Sun Bug #4071593).
         * We have to explicitly set the method accessible
         * via method.setAccessible(true) but we have to use
         * reflection because the setAccessible() in Method is
         * not available under jdk 1.1. We wait until a failure
         * to retry to avoid the overhead of this call on cases
         * that don't require it.
         */
        if (method_setAccessible != null) {
            Object[] args_wrapper = { Boolean.TRUE };
            try {
                method_setAccessible.invoke(method, args_wrapper);
            }
            catch (IllegalAccessException ex) { }
            catch (IllegalArgumentException ex) { }
            catch (InvocationTargetException ex) { }
            return method.invoke(obj, args);
        }
        throw illegalAccess;
    }

    /**
     * Find the index of the correct function to call given the set of methods
     * or constructors and the arguments.
     * If no function can be found to call, return -1.
     */
    static int findFunction(Member[] methodsOrCtors, Object[] args)
    {
        if (methodsOrCtors.length == 0) {
            return -1;
        } else if (methodsOrCtors.length == 1) {
            Member member = methodsOrCtors[0];
            Class paramTypes[] = (member instanceof Method)
                                 ? ((Method) member).getParameterTypes()
                                 : ((Constructor) member).getParameterTypes();
            int plength = paramTypes.length;
            if (plength != args.length) {
                return -1;
            }
            for (int j = 0; j != plength; ++j) {
                if (!NativeJavaObject.canConvert(args[j], paramTypes[j])) {
                    if (debug) printDebug("Rejecting (args can't convert) ",
                                          member, args);
                    return -1;
                }
            }
            if (debug) printDebug("Found ", member, args);
            return 0;
        }

        boolean hasMethods = methodsOrCtors[0] instanceof Method;

        int bestFit = -1;
        Class[] bestFitTypes = null;

        int[] ambiguousMethods = null;
        int ambiguousMethodCount = 0;

        for (int i = 0; i < methodsOrCtors.length; i++) {
            Member member = methodsOrCtors[i];
            Class paramTypes[] = hasMethods
                                 ? ((Method) member).getParameterTypes()
                                 : ((Constructor) member).getParameterTypes();
            if (paramTypes.length != args.length) {
                continue;
            }
            if (bestFit < 0) {
                int j;
                for (j = 0; j < paramTypes.length; j++) {
                    if (!NativeJavaObject.canConvert(args[j], paramTypes[j])) {
                        if (debug) printDebug("Rejecting (args can't convert) ",
                                              member, args);
                        break;
                    }
                }
                if (j == paramTypes.length) {
                    if (debug) printDebug("Found ", member, args);
                    bestFit = i;
                    bestFitTypes = paramTypes;
                }
            }
            else {
                int preference = preferSignature(args, paramTypes,
                                                 bestFitTypes);
                if (preference == PREFERENCE_AMBIGUOUS) {
                    if (debug) printDebug("Deferring ", member, args);
                    // add to "ambiguity list"
                    if (ambiguousMethods == null)
                        ambiguousMethods = new int[methodsOrCtors.length];
                    ambiguousMethods[ambiguousMethodCount++] = i;
                } else if (preference == PREFERENCE_FIRST_ARG) {
                    if (debug) printDebug("Substituting ", member, args);
                    bestFit = i;
                    bestFitTypes = paramTypes;
                } else if (preference == PREFERENCE_SECOND_ARG) {
                    if (debug) printDebug("Rejecting ", member, args);
                } else {
                    if (preference != PREFERENCE_EQUAL) Context.codeBug();
                    Member best = methodsOrCtors[bestFit];
                    if (Modifier.isStatic(best.getModifiers())
                        && best.getDeclaringClass().isAssignableFrom(
                               member.getDeclaringClass()))
                    {
                        // On some JVMs, Class.getMethods will return all
                        // static methods of the class heirarchy, even if
                        // a derived class's parameters match exactly.
                        // We want to call the dervied class's method.
                        if (debug) printDebug(
                            "Substituting (overridden static)", member, args);
                        bestFit = i;
                        bestFitTypes = paramTypes;
                    } else {
                        if (debug) printDebug(
                            "Ignoring same signature member ", member, args);
                    }
                }
            }
        }

        if (ambiguousMethodCount == 0)
            return bestFit;

        // Compare ambiguous methods with best fit, in case
        // the current best fit removes the ambiguities.
        int removedCount = 0;
        for (int k = 0; k != ambiguousMethodCount; ++k) {
            int i = ambiguousMethods[k];
            Member member = methodsOrCtors[i];
            Class paramTypes[] = hasMethods
                                 ? ((Method) member).getParameterTypes()
                                 : ((Constructor) member).getParameterTypes();
            int preference = preferSignature(args, paramTypes,
                                             bestFitTypes);

            if (preference == PREFERENCE_FIRST_ARG) {
                if (debug) printDebug("Substituting ", member, args);
                bestFit = i;
                bestFitTypes = paramTypes;
                ambiguousMethods[k] = -1;
                ++removedCount;
            }
            else if (preference == PREFERENCE_SECOND_ARG) {
                if (debug) printDebug("Rejecting ", member, args);
                ambiguousMethods[k] = -1;
                ++removedCount;
            }
            else {
                if (debug) printDebug("UNRESOLVED: ", member, args);
            }
        }

        if (removedCount == ambiguousMethodCount) {
            return bestFit;
        }

        // PENDING: report remaining ambiguity
        StringBuffer buf = new StringBuffer();

        ambiguousMethods[ambiguousMethodCount++] = bestFit;
        boolean first = true;
        for (int k = 0; k < ambiguousMethodCount; k++) {
            int i = ambiguousMethods[k];
            if (i < 0) { continue; }
            if (!first) {
                buf.append(", ");
            }
            Member member = methodsOrCtors[i];
            if (hasMethods) {
                Method method = (Method)member;
                Class rtnType = method.getReturnType();
                buf.append(rtnType);
                buf.append(' ');
            }
            buf.append(memberSignature(member));
            first = false;
        }

        Member best = methodsOrCtors[bestFit];

        if (!hasMethods) {
            throw Context.reportRuntimeError3(
                "msg.constructor.ambiguous",
                best.getName(), scriptSignature(args), buf.toString());
        } else {
            throw Context.reportRuntimeError4(
                "msg.method.ambiguous", best.getDeclaringClass().getName(),
                best.getName(), scriptSignature(args), buf.toString());
        }
    }

    /** Types are equal */
    private static final int PREFERENCE_EQUAL      = 0;
    private static final int PREFERENCE_FIRST_ARG  = 1;
    private static final int PREFERENCE_SECOND_ARG = 2;
    /** No clear "easy" conversion */
    private static final int PREFERENCE_AMBIGUOUS  = 3;

    /**
     * Determine which of two signatures is the closer fit.
     * Returns one of PREFERENCE_EQUAL, PREFERENCE_FIRST_ARG,
     * PREFERENCE_SECOND_ARG, or PREFERENCE_AMBIGUOUS.
     */
    private static int preferSignature(Object[] args,
                                       Class[] sig1, Class[] sig2)
    {
        int preference = 0;

        for (int j = 0; j < args.length; j++) {
            Class type1 = sig1[j];
            Class type2 = sig2[j];

            if (type1 == type2) {
                continue;
            }

            preference |= preferConversion(args[j], type1, type2);

            if (preference == PREFERENCE_AMBIGUOUS) {
                break;
            }
        }
        return preference;
    }


    /**
     * Determine which of two types is the easier conversion.
     * Returns one of PREFERENCE_EQUAL, PREFERENCE_FIRST_ARG,
     * PREFERENCE_SECOND_ARG, or PREFERENCE_AMBIGUOUS.
     */
    private static int preferConversion(Object fromObj,
                                        Class toClass1, Class toClass2)
    {
        int rank1  =
            NativeJavaObject.getConversionWeight(fromObj, toClass1);
        int rank2 =
            NativeJavaObject.getConversionWeight(fromObj, toClass2);

        if (rank1 == NativeJavaObject.CONVERSION_NONTRIVIAL &&
            rank2 == NativeJavaObject.CONVERSION_NONTRIVIAL) {

            if (toClass1.isAssignableFrom(toClass2)) {
                return PREFERENCE_SECOND_ARG;
            }
            else if (toClass2.isAssignableFrom(toClass1)) {
                return PREFERENCE_FIRST_ARG;
            }
        }
        else {
            if (rank1 < rank2) {
                return PREFERENCE_FIRST_ARG;
            }
            else if (rank1 > rank2) {
                return PREFERENCE_SECOND_ARG;
            }
        }
        return PREFERENCE_AMBIGUOUS;
    }

    private static final boolean debug = false;

    private static void printDebug(String msg, Member member, Object[] args)
    {
        if (debug) {
            StringBuffer sb = new StringBuffer();
            sb.append(" ----- ");
            sb.append(msg);
            sb.append(member.getDeclaringClass().getName());
            sb.append('.');
            sb.append(memberSignature(member));
            sb.append(" for arguments (");
            sb.append(scriptSignature(args));
            sb.append(')');
        }
    }

    Method methods[];

    private static Method method_setAccessible = null;

    static {
        try {
            Class MethodClass = Class.forName("java.lang.reflect.Method");
            method_setAccessible = MethodClass.getMethod(
                "setAccessible", new Class[] { Boolean.TYPE });
        } catch (Exception ex) {
            // Assume any exceptions means the method does not exist.
        }
    }

}

