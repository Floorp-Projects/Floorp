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

public class NativeJavaMethod extends NativeFunction implements Function {

    public NativeJavaMethod() {
        this.functionName = null;
    }

    public NativeJavaMethod(Method[] methods) {
        this.methods = methods;
        this.functionName = methods[0].getName();
    }

    public NativeJavaMethod(Method method, String name) {
        this.methods = new Method[1];
        this.methods[0] = method;
        this.functionName = name;
    }

    public void add(Method method) {
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

    static String scriptSignature(Object value) {
        if (value == null) {
            return "null";
        }
        else {
            Class type = value.getClass();
            if (type == ScriptRuntime.UndefinedClass)
                return "undefined";
            if (type == ScriptRuntime.BooleanClass)
                return "boolean";
            if (type == ScriptRuntime.StringClass)
                return "string";
            if (ScriptRuntime.NumberClass.isAssignableFrom(type))
                return "number";
            if (value instanceof Wrapper) {
                return ((Wrapper)value).unwrap().getClass().getName();
            }
            if (value instanceof Scriptable) {
                if (value instanceof Function)
                    return "function";
                return "object";
            }
            return javaSignature(type);
        }
    }

    static String scriptSignature(Object[] values) {
        StringBuffer sig = new StringBuffer();
        for (int i = 0; i < values.length; i++) {
            if (i != 0)
                sig.append(',');
            sig.append(scriptSignature(values[i]));
        }
        return sig.toString();
    }

    static String javaSignature(Class type) {
        if (type == null) {
            return "null";
        }
        else if (type.isArray()) {
            return javaSignature(type.getComponentType()) + "[]";
        }
        return type.getName();
    }

    static String javaSignature(Class[] types) {
        StringBuffer sig = new StringBuffer();
        for (int i = 0; i < types.length; i++) {
            if (i != 0)
                sig.append(',');
            sig.append(javaSignature(types[i]));
        }
        return sig.toString();
    }

    static String signature(Member member) {
        Class paramTypes[];

        if (member instanceof Method) {
            paramTypes = ((Method) member).getParameterTypes();
            return member.getName() + "(" + javaSignature(paramTypes) + ")";
        }
        else {
            paramTypes = ((Constructor) member).getParameterTypes();
            return "(" + javaSignature(paramTypes) + ")";
        }
    }
    
    public String toString() {
        StringBuffer sb = new StringBuffer();
        for (int i=0; i < methods.length; i++) {
            sb.append(javaSignature(methods[i].getReturnType()));
            sb.append(' ');
            sb.append(signature(methods[i]));
            sb.append('\n');
        }
        return sb.toString();
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        // Find a method that matches the types given.
        if (methods.length == 0) {
            throw new RuntimeException("No methods defined for call");
        }

        Method meth = (Method) findFunction(methods, args);
        if (meth == null) {
            Class c = methods[0].getDeclaringClass();
            String sig = c.getName() + "." + functionName + "(" +
                         scriptSignature(args) + ")";
            throw Context.reportRuntimeError1("msg.java.no_such_method", sig);
        }

        // OPT: already retrieved in findFunction, so we should inline that
        // OPT: or pass it back somehow
        Class paramTypes[] = meth.getParameterTypes();

        // First, we marshall the args.
        for (int i = 0; i < args.length; i++) {
            args[i] = NativeJavaObject.coerceType(paramTypes[i], args[i]);
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
        try {
            if (debug) {
                printDebug("Calling ", meth, args);
            }

            Object retval = meth.invoke(javaObject, args);
            Class staticType = meth.getReturnType();

            if (debug) {
                Class actualType = (retval == null) ? null : retval.getClass();
                System.err.println(" ----- Returned " + retval + 
                                   " actual = " + actualType +
                                   " expect = " + staticType);
            }

            Object wrapped = NativeJavaObject.wrap(scope, retval, staticType);

            if (debug) {
                Class actualType = (wrapped == null) ? null : wrapped.getClass();
                System.err.println(" ----- Wrapped as " + wrapped + 
                                   " class = " + actualType);
            }

            if (wrapped == Undefined.instance)
                return wrapped;
            if (wrapped == null && staticType == Void.TYPE)
                return Undefined.instance;
            return wrapped;
        } catch (IllegalAccessException accessEx) {
            throw Context.reportRuntimeError(accessEx.getMessage());
        } catch (InvocationTargetException e) {
            throw JavaScriptException.wrapException(scope, e);
        }
    }

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
                        if (debug) printDebug("Rejecting (args can't convert) ", 
                                              member, args);
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
                    if (preference == PREFERENCE_EQUAL &&
                        Modifier.isStatic(bestFit.getModifiers()) &&
                        bestFit.getDeclaringClass().isAssignableFrom(
                            member.getDeclaringClass()))                                          
                    {
                        // On some JVMs, Class.getMethods will return all
                        // static methods of the class heirarchy, even if
                        // a derived class's parameters match exactly.
                        // We want to call the dervied class's method.
                        if (debug) printDebug("Rejecting (overridden static)",
                                              member, args);
                        bestFit = member;
                        bestFitTypes = paramTypes;
                    } else {
                        if (debug) printDebug("Rejecting ", member, args);
                    }
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
                    NativeJavaMethod.scriptSignature(args),
                    buf.toString()
                };
                errMsg = 
                    Context.getMessage("msg.constructor.ambiguous", errArgs);
            }
            else {
                Object errArgs[] = { 
                    bestFit.getDeclaringClass().getName(), 
                    bestFit.getName(), 
                    NativeJavaMethod.scriptSignature(args),
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
                                      Class[] sig1, Class[] sig2) 
    {
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

    Method[] getMethods() {
        return methods; 
    }

    private static final boolean debug = false;

    private static void printDebug(String msg, Member member, Object[] args) {
        if (debug) {
            System.err.println(" ----- " + msg + 
                               member.getDeclaringClass().getName() +
                               "." + signature(member) +
                               " for arguments (" + scriptSignature(args) + ")");
        }
    }

    Method methods[];
}

