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

public class NativeJavaMethod extends NativeFunction implements Function {

    public NativeJavaMethod() {
        names = new String[1];
    }

    public NativeJavaMethod(Method[] methods) {
        this.methods = methods;
        names = new String[1];
        names[0] = methods[0].getName();
    }

    public NativeJavaMethod(Method method, String name) {
        this.methods = new Method[1];
        this.methods[0] = method;
        names = new String[1];
        names[0] = name;
    }

    public void add(Method method) {
        if (names[0] == null) {
            names[0] = method.getName();
        } else if (!names[0].equals(method.getName())) {
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

    static String signature(Class type) {
        if (type == null)
            return "null";
        if (type == ScriptRuntime.BooleanClass)
            return "boolean";
        if (type == ScriptRuntime.ByteClass)
            return "byte";
        if (type == ScriptRuntime.ShortClass)
            return "short";
        if (type == ScriptRuntime.IntegerClass)
            return "int";
        if (type == ScriptRuntime.LongClass)
            return "long";
        if (type == ScriptRuntime.FloatClass)
            return "float";
        if (type == ScriptRuntime.DoubleClass)
            return "double";
        if (type == ScriptRuntime.StringClass)
            return "string";
        if (ScriptRuntime.ScriptableClass.isAssignableFrom(type)) {
            if (ScriptRuntime.FunctionClass.isAssignableFrom(type))
                return "function";
            if (type == ScriptRuntime.UndefinedClass)
                return "undefined";
            return "object";
        }
        if (type.isArray()) {
            return signature(type.getComponentType()) + "[]";
        }
        return type.getName();
    }

    static String signature(Object[] values) {
        StringBuffer sig = new StringBuffer();
        for (int i = 0; i < values.length; i++) {
            if (i != 0)
                sig.append(',');
            sig.append(values[i] == null ? "null"
                                         : signature(values[i].getClass()));
        }
        return sig.toString();
    }

    static String signature(Class[] types) {
        StringBuffer sig = new StringBuffer();
        for (int i = 0; i < types.length; i++) {
            if (i != 0)
                sig.append(',');
            sig.append(signature(types[i]));
        }
        return sig.toString();
    }

    static String signature(Member member) {
        Class paramTypes[];

        if (member instanceof Method) {
            paramTypes = ((Method) member).getParameterTypes();
            return member.getName() + "(" + signature(paramTypes) + ")";
        }
        else {
            paramTypes = ((Constructor) member).getParameterTypes();
            return "(" + signature(paramTypes) + ")";
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

        // Eliminate useless args[0] and unwrap if required
        for (int i = 0; i < args.length; i++)
            if (args[i] instanceof Wrapper)
                args[i] = ((Wrapper)args[i]).unwrap();

        Method meth = (Method) findFunction(methods, args);
        if (meth == null) {
            Class c = methods[0].getDeclaringClass();
            String sig = c.getName() + "." + names[0] + "(" +
                         signature(args) + ")";
            Object errArgs[] = { sig };
            throw Context.reportRuntimeError(
                Context.getMessage("msg.java.no_such_method", errArgs));
        }

        // OPT: already retrieved in findFunction, so we should inline that
        // OPT: or pass it back somehow
        Class paramTypes[] = meth.getParameterTypes();

        // First, we marshall the args.
        for (int i = 0; i < args.length; i++) {
            args[i] = NativeJavaObject.coerceType(paramTypes[i], args[i]);
        }
        Object javaObject;
        try {
            javaObject = ((NativeJavaObject) thisObj).unwrap();        
        }
        catch (ClassCastException e) {
            if (Modifier.isStatic(meth.getModifiers())) {
                javaObject = null;  // don't need it anyway
            } else {
                Object errArgs[] = { names[0] };
                throw Context.reportRuntimeError(
                    Context.getMessage("msg.nonjava.method", errArgs));
            }
        }
        try {
            Object retval = meth.invoke(javaObject, args);
            Class staticType = meth.getReturnType();
            Object wrapped = NativeJavaObject.wrap(scope, retval, staticType);
            // XXX set prototype && parent
            if (wrapped == Undefined.instance)
                return wrapped;
            if (wrapped == null && staticType == Void.TYPE)
                return Undefined.instance;
            if (retval != wrapped && wrapped instanceof Scriptable) {
                Scriptable s = (Scriptable)wrapped;
                if (s.getPrototype() == null)
                    s.setPrototype(parent.getPrototype());
                if (s.getParentScope() == null)
                    s.setParentScope(parent.getParentScope());
            }
            return wrapped;
        } catch (IllegalAccessException accessEx) {
            throw Context.reportRuntimeError(accessEx.getMessage());
        } catch (InvocationTargetException e) {
            throw JavaScriptException.wrapException(scope, e);
        }
    }

    /*
    public Object getDefaultValue(Class hint) {
        return this;
    }
    */
    
    /** 
     * Find the correct function to call given the set of methods
     * or constructors and the arguments.
     * If no function can be found to call, return null.
     */
    static Member findFunction(Member[] methodsOrCtors, Object[] args) {
        if (methodsOrCtors.length == 0)
            return null;
        boolean hasMethods = methodsOrCtors[0] instanceof Method;
        if (Context.useJSObject && 
            NativeJavaObject.jsObjectClass != null) 
        {
            try {
                for (int i = 0; i < args.length; i++) {
                    if (NativeJavaObject.jsObjectClass.isInstance(args[i]))
                        args[i] = NativeJavaObject.jsObjectGetScriptable.invoke(
                                    args[i], ScriptRuntime.emptyArgs);
                }
            }
            catch (InvocationTargetException e) {
                // Just abandon conversion from JSObject
            }
            catch (IllegalAccessException e) {
                // Just abandon conversion from JSObject
            }
        }

        Member  bestFit = null;
        Class[] bestFitTypes = null;

        java.util.Vector ambiguousMethods = null;

        for (int i = 0; i < methodsOrCtors.length; i++) {
            Member member = methodsOrCtors[i];
            Class paramTypes[] = hasMethods
                                 ? ((Method) member).getParameterTypes()
                                 : ((Constructor) member).getParameterTypes();
            if (paramTypes.length != args.length) {
                continue;
            }
            if (bestFitTypes == null) {
                int j;
                for (j = 0; j < paramTypes.length; j++) {
                    if (!NativeJavaObject.canConvert(args[j], paramTypes[j])) {
                        if (debug) printDebug("Rejecting ", member, args);
                        break;
                    }
                }
                if (j == paramTypes.length) {
                    if (debug) printDebug("Found ", member, args);
                    bestFit = member;
                    bestFitTypes = paramTypes;
                }
            }
            else {
                int preference = 
                    NativeJavaMethod.preferSignature(args, 
                                                     paramTypes, 
                                                     bestFitTypes);
                if (preference == PREFERENCE_AMBIGUOUS) {
                    if (debug) printDebug("Deferring ", member, args);
                    // add to "ambiguity list"
                    if (ambiguousMethods == null)
                        ambiguousMethods = new java.util.Vector();
                    ambiguousMethods.addElement(member);
                }
                else if (preference == PREFERENCE_FIRST_ARG) {
                    if (debug) printDebug("Substituting ", member, args);
                    bestFit = member;
                    bestFitTypes = paramTypes;
                }
                else {
                    if (debug) printDebug("Rejecting ", member, args);
                }
            }
        }
        
        if (ambiguousMethods == null)
            return bestFit;

        // Compare ambiguous methods with best fit, in case 
        // the current best fit removes the ambiguities.
        for (int i = ambiguousMethods.size() - 1; i >= 0 ; i--) {
            Member member = (Member)ambiguousMethods.elementAt(i);
            Class paramTypes[] = hasMethods
                                 ? ((Method) member).getParameterTypes()
                                 : ((Constructor) member).getParameterTypes();
            int preference = 
                NativeJavaMethod.preferSignature(args, 
                                                 paramTypes, 
                                                 bestFitTypes);

            if (preference == PREFERENCE_FIRST_ARG) {
                if (debug) printDebug("Substituting ", member, args);
                bestFit = member;
                bestFitTypes = paramTypes;
                ambiguousMethods.removeElementAt(i);
            }
            else if (preference == PREFERENCE_SECOND_ARG) {
                if (debug) printDebug("Rejecting ", member, args);
                ambiguousMethods.removeElementAt(i);
            }
            else {
                if (debug) printDebug("UNRESOLVED: ", member, args);
            }
        }

        if (ambiguousMethods.size() > 0) {
            // PENDING: report remaining ambiguity
            StringBuffer buf = new StringBuffer();
            boolean isCtor = (bestFit instanceof Constructor);

            ambiguousMethods.addElement(bestFit);

            for (int i = 0; i < ambiguousMethods.size(); i++) {
                if (i != 0) {
                    buf.append(", ");
                }
                Member member = (Member)ambiguousMethods.elementAt(i);
                if (!isCtor) {
                    Class rtnType = ((Method)member).getReturnType();
                    buf.append(rtnType);
                    buf.append(' ');
                }
                buf.append(NativeJavaMethod.signature(member));
            }

            String errMsg;
            if (isCtor) {
                Object errArgs[] = { 
                    bestFit.getName(), 
                    NativeJavaMethod.signature(args),
                    buf.toString()
                };
                errMsg = 
                    Context.getMessage("msg.constructor.ambiguous", errArgs);
            }
            else {
                Object errArgs[] = { 
                    bestFit.getDeclaringClass().getName(), 
                    bestFit.getName(), 
                    NativeJavaMethod.signature(args),
                    buf.toString()
                };
                errMsg = Context.getMessage("msg.method.ambiguous", errArgs);
            }

            throw 
                Context.reportRuntimeError(errMsg);
        }

        return bestFit;
    }
    
    /** Types are equal */
    static final int PREFERENCE_EQUAL      = 0;
    static final int PREFERENCE_FIRST_ARG  = 1;
    static final int PREFERENCE_SECOND_ARG = 2;
    /** No clear "easy" conversion */
    static final int PREFERENCE_AMBIGUOUS  = 3;

    /**
     * Determine which of two signatures is the closer fit.
     * Returns one of PREFERENCE_EQUAL, PREFERENCE_FIRST_ARG, 
     * PREFERENCE_SECOND_ARG, or PREFERENCE_AMBIGUOUS.
     */
    public static int preferSignature(Object[] args, 
                                      Class[] sig1, Class[] sig2) {

        int preference = 0;

        for (int j = 0; j < args.length; j++) {
            Class type1 = sig1[j];
            Class type2 = sig2[j];

            if (type1 == type2) {
                continue;
            }

            preference |=
                NativeJavaMethod.preferConversion(args[j], 
                                                  type1,
                                                  type2);

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
    public static int preferConversion(Object fromObj, 
                                       Class toClass1, Class toClass2) {

        int rank1  = 
            NativeJavaObject.getConversionWeight(fromObj, toClass1);
        int rank2 = 
            NativeJavaObject.getConversionWeight(fromObj, toClass2);

        if (rank1 == NativeJavaObject.CONVERSION_NONTRIVIAL && 
            rank2 == NativeJavaObject.CONVERSION_NONTRIVIAL) {

            if (toClass1.isAssignableFrom(toClass2)) {
                return PREFERENCE_FIRST_ARG;
            }
            else if (toClass2.isAssignableFrom(toClass1)) {
                return PREFERENCE_SECOND_ARG;
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

    Method[] getMethods() {
        return methods; 
    }

    private static final boolean debug = false;

    private static void printDebug(String msg, Member member, Object[] args) {
        if (debug) {
            System.err.println(" ----- " + msg + 
                               member.getDeclaringClass().getName() +
                               "." + signature(member) +
                               " for arguments (" + signature(args) + ")");
        }
    }

    Method methods[];
}

