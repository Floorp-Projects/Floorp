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
import java.util.Enumeration;

/**
 * This class reflects non-Array Java objects into the JavaScript environment.  It
 * reflect fields directly, and uses NativeJavaMethod objects to reflect (possibly
 * overloaded) methods.<p>
 *
 * @author Mike Shaver
 * @see NativeJavaArray
 * @see NativeJavaPackage
 * @see NativeJavaClass
 */

public class NativeJavaObject implements Scriptable, Wrapper {
    
    public NativeJavaObject(Object javaObject, JavaMembers members) {
        this.javaObject = javaObject;
        this.members = members;
    }
    
    public NativeJavaObject(Scriptable scope, Object javaObject, 
                            Class staticType) 
    {
        this.javaObject = javaObject;
        Class dynamicType = javaObject != null ? javaObject.getClass()
            : staticType;
        members = JavaMembers.lookupClass(scope, dynamicType, staticType);
        fieldAndMethods = members.getFieldAndMethodsObjects(javaObject, false);
    }
    
    public boolean has(String name, Scriptable start) {
        return members.has(name, false);
    }
        
    public boolean has(int index, Scriptable start) {
        return false;
    }
        
    public Object get(String name, Scriptable start) {
        if (fieldAndMethods != null) {
            Object result = fieldAndMethods.get(name);
            if (result != null)
                return result;
        }
        // TODO: passing 'this' as the scope is bogus since it has 
        //  no parent scope
        return members.get(this, name, javaObject, false);
    }

    public Object get(int index, Scriptable start) {
        throw members.reportMemberNotFound(Integer.toString(index));
    }
    
    public void put(String name, Scriptable start, Object value) {
        members.put(name, javaObject, value, false);
    }

    public void put(int index, Scriptable start, Object value) {
        throw members.reportMemberNotFound(Integer.toString(index));
    }

    public boolean hasInstance(Scriptable value) {
        // This is an instance of a Java class, so always return false
        return false;
    }
    
    public void delete(String name) {
    }
    
    public void delete(int index) {
    }
    
    public Scriptable getPrototype() {
        return null;
    }

    public void setPrototype(Scriptable prototype) {
    }

    public Scriptable getParentScope() {
        return null;
    }

    public void setParentScope(Scriptable parent) {
    }

    public Object[] getIds() {
        return members.getIds(false);
    }
    
    public static Object wrap(Scriptable scope, Object obj, Class staticType) 
    {
        if (obj == null)
            return obj;
        Class cls = obj.getClass();
        if (staticType != null && staticType.isPrimitive()) {
            if (staticType == Void.TYPE)
                return Undefined.instance;
            if (staticType == Character.TYPE)
                return new Integer((int) ((Character) obj).charValue());
            return obj;
        }
        if (cls.isArray())
            return NativeJavaArray.wrap(scope, obj);
        if (obj instanceof Scriptable)
            return obj;
        if (Context.useJSObject && jsObjectClass != null && 
            staticType != jsObjectClass && jsObjectClass.isInstance(obj)) 
            {
                try {
                    return jsObjectGetScriptable.invoke(obj, ScriptRuntime.emptyArgs);
                }
                catch (InvocationTargetException e) {
                    // Just abandon conversion from JSObject
                }
                catch (IllegalAccessException e) {
                    // Just abandon conversion from JSObject
                }
            }
        return new NativeJavaObject(scope, obj, staticType);
    }

    public Object unwrap() {
        return javaObject;
    }

    public String getClassName() {
        return "JavaObject";
    }

    Function getConverter(String converterName) {
        Object converterFunction = get(converterName, this);
        if (converterFunction instanceof Function) {
            return (Function) converterFunction;
        }
        return null;
    }

    Object callConverter(Function converterFunction)
        throws JavaScriptException
    {
        Function f = (Function) converterFunction;
        return f.call(Context.getContext(), f.getParentScope(),
                      this, new Object[0]);
    }

    Object callConverter(String converterName)
        throws JavaScriptException
    {
        Function converter = getConverter(converterName);
        if (converter == null) {
            Object[] errArgs = { converterName, javaObject.getClass().getName() };
            throw Context.reportRuntimeError(
                                             Context.getMessage("msg.java.conversion.implicit_method",
                                                                errArgs));
        }
        return callConverter(converter);
    }

    public Object getDefaultValue(Class hint) {
        if (hint == null || hint == ScriptRuntime.StringClass)
            return javaObject.toString();
        try {
            if (hint == ScriptRuntime.BooleanClass)
                return callConverter("booleanValue");
            if (hint == ScriptRuntime.NumberClass) {
                return callConverter("doubleValue");
            }
            // fall through to error message
        } catch (JavaScriptException jse) {
            // fall through to error message
        }
        throw Context.reportRuntimeError(
                                         Context.getMessage("msg.default.value", null));
    }


    /**
     * Determine whether we can/should convert between the given type and the
     * desired one.  This should be superceded by a conversion-cost calculation
     * function, but for now I'll hide behind precedent.
     */
    public static boolean canConvert(Object fromObj, Class to) {
        int weight = NativeJavaObject.getConversionWeight(fromObj, to);

        return (weight != CONVERSION_NONE);
    }

    static final int JSTYPE_UNDEFINED   = 0; // undefined type
    static final int JSTYPE_NULL        = 1; // null
    static final int JSTYPE_BOOLEAN     = 2; // boolean
    static final int JSTYPE_NUMBER      = 3; // number
    static final int JSTYPE_STRING      = 4; // string
    static final int JSTYPE_JAVA_CLASS  = 5; // JavaClass
    static final int JSTYPE_JAVA_OBJECT = 6; // JavaObject
    static final int JSTYPE_JAVA_ARRAY  = 7; // JavaArray
    static final int JSTYPE_OBJECT      = 8; // Scriptable

    public static final byte CONVERSION_TRIVIAL      = 1;
    public static final byte CONVERSION_NONTRIVIAL   = 0;
    public static final byte CONVERSION_NONE         = 99;

    /**
     * Derive a ranking based on how "natural" the conversion is.
     * The special value CONVERSION_NONE means no conversion is possible, 
     * and CONVERSION_NONTRIVIAL signals that more type conformance testing 
     * is required.
     * Based on 
     * <a href="http://www.mozilla.org/js/liveconnect/lc3_method_overloading.html">
     * "preferred method conversions" from Live Connect 3</a>
     */
    public static int getConversionWeight(Object fromObj, Class to) {
        int fromCode = NativeJavaObject.getJSTypeCode(fromObj);

        int result = CONVERSION_NONE;

        switch (fromCode) {

        case JSTYPE_UNDEFINED:
            if (to == String.class || to == Object.class) {
                result = 1;
            }
            break;

        case JSTYPE_NULL:
            if (!to.isPrimitive()) {
                result = 1;
            }
            break;

        case JSTYPE_BOOLEAN:
            // "boolean" is #1
            if (to == Boolean.TYPE) {
                result = 1;
            }
            else if (to == Boolean.class) {
                result = 2;
            }
            else if (to == Object.class) {
                result = 3;
            }
            else if (to == String.class) {
                result = 4;
            }
            break;

        case JSTYPE_NUMBER:
            if (to.isPrimitive()) {
                if (to == Double.TYPE) {
                    result = 1;
                }
                else if (to == Boolean.TYPE) {
                    result = CONVERSION_NONE;
                }
                else {
                    result = 1 + NativeJavaObject.getSizeRank(to);
                }
            }
            else {
                if (to == String.class) {
                    // native numbers are #1-8
                    result = 9;
                }
                else if (to == Object.class) {
                    result = 10;
                }
                else if (Number.class.isAssignableFrom(to)) {
                    // "double" is #1
                    result = 2;
                }
            }
            break;

        case JSTYPE_STRING:
            if (to == String.class) {
                result = 1;
            }
            else if (to == Object.class) {
                result = 2;
            }
            else if (to.isPrimitive()) {
                if (to == Character.TYPE) {
                    result = 3;
                }
                else {
                    result = 4;
                }
            }
            break;

        case JSTYPE_JAVA_CLASS:
            if (to == Class.class) {
                result = 1;
            }
            if (Context.useJSObject && jsObjectClass != null && 
                jsObjectClass.isAssignableFrom(to)) {
                result = 2;
            }
            else if (to == Object.class) {
                result = 3;
            }
            else if (to == String.class) {
                result = 4;
            }
            break;

        case JSTYPE_JAVA_OBJECT:
        case JSTYPE_JAVA_ARRAY:
            if (to == String.class) {
                result = 2;
            }
            else if (to.isPrimitive()) {
                result = 
                    (fromCode == JSTYPE_JAVA_ARRAY) ?
                    CONVERSION_NONTRIVIAL :
                    2 + NativeJavaObject.getSizeRank(to);
            }
            else {
                Object javaObj = fromObj;
                if (javaObj instanceof NativeJavaObject) {
                    javaObj = ((NativeJavaObject)javaObj).unwrap();
                }
                if (to.isInstance(javaObj)) {
                    result = CONVERSION_NONTRIVIAL;
                }
            }
            break;

        case JSTYPE_OBJECT:
            // Other objects takes #1-#3 spots
            if (Context.useJSObject && jsObjectClass != null && 
                jsObjectClass.isAssignableFrom(to)) {
                result = 1;
            }
            else if (to == Object.class) {
                result = 2;
            }
            else if (to == String.class) {
                result = 3;
            }
            else if (to.isPrimitive()) {
                result = 3 + NativeJavaObject.getSizeRank(to);
            }
            break;
        }

        return result;
    
    }

    static int getSizeRank(Class aType) {
        if (aType == Double.TYPE) {
            return 1;
        }
        else if (aType == Float.TYPE) {
            return 2;
        }
        else if (aType == Long.TYPE) {
            return 3;
        }
        else if (aType == Integer.TYPE) {
            return 4;
        }
        else if (aType == Short.TYPE) {
            return 5;
        }
        else if (aType == Character.TYPE) {
            return 6;
        }
        else if (aType == Byte.TYPE) {
            return 7;
        }
        else {
            return 8;
        }
    }

    static int getJSTypeCode(Object value) {
        if (value == null) {
            return JSTYPE_NULL;
        }
        else if (value == Undefined.instance) {
            return JSTYPE_UNDEFINED;
        }
        else if (value instanceof Scriptable) {
            if (value instanceof NativeJavaClass) {
                return JSTYPE_JAVA_CLASS;
            }
            else if (value instanceof NativeJavaArray) {
                return JSTYPE_JAVA_ARRAY;
            }
            else if (value instanceof NativeJavaObject) {
                return JSTYPE_JAVA_OBJECT;
            }
            else {
                return JSTYPE_OBJECT;
            }
        }
        else {
            Class valueClass = value.getClass();

            if (valueClass == String.class) {
                return JSTYPE_STRING;
            }
            else if (valueClass == Boolean.class) {
                return JSTYPE_BOOLEAN;
            }
            else if (value instanceof Number) {
                return JSTYPE_NUMBER;
            }
            else if (valueClass == Class.class) {
                return JSTYPE_JAVA_CLASS;
            }
            else if (valueClass.isArray()) {
                return JSTYPE_JAVA_ARRAY;
            }
            else {
                return JSTYPE_JAVA_OBJECT;
            }
        }
    }

    /**
     * Type-munging for field setting and method invocation.
     */
    public static Object coerceType(Class type, Object value) {
        // Don't coerce null to a string (or other object)
        if (value == null) {
            return null;
        }

        // For final classes we can compare valueClass to a class object
        // rather than using instanceof
        Class valueClass = value.getClass();
        
        // Is value already of the correct type?
        if (valueClass == type)
            return value;

        // String
        if (type == ScriptRuntime.StringClass)
            return ScriptRuntime.toString(value);
        
        // Boolean
        if (type == Boolean.TYPE || type == ScriptRuntime.BooleanClass)
            return new Boolean(ScriptRuntime.toBoolean(value));

        if (type == Character.TYPE || type == ScriptRuntime.CharacterClass) {
            // Special case for converting a single char string to a character
            if (valueClass == ScriptRuntime.StringClass && ((String) value).length() == 1)
                return new Character(((String) value).charAt(0));
            return new Character((char)ScriptRuntime.toInteger(value));
        }

        // Integer, Long, Char, Byte
        if (type == ScriptRuntime.IntegerClass || type == Integer.TYPE)
            return new Integer((int)ScriptRuntime.toInteger(value));

        if (type == ScriptRuntime.LongClass || type == Long.TYPE)
            return new Long((long)ScriptRuntime.toInteger(value));

        if (type == ScriptRuntime.ByteClass || type == Byte.TYPE)
            return new Byte((byte)ScriptRuntime.toInteger(value));

        if (type == ScriptRuntime.ShortClass || type == Short.TYPE)
            return new Short((short)ScriptRuntime.toInteger(value));

        // Double, Float
        if (type == ScriptRuntime.DoubleClass || type == Double.TYPE) {
            return valueClass == ScriptRuntime.DoubleClass
                ? value
                : new Double(ScriptRuntime.toNumber(value));
        }

        if (type == ScriptRuntime.FloatClass || type == Float.TYPE) {
            return valueClass == ScriptRuntime.FloatClass
                ? value
                : new Float(ScriptRuntime.toNumber(value));
        }

        if (valueClass == ScriptRuntime.DoubleClass)
            return value;
            
        // If JSObject compatibility is enabled, and the method wants it,
        // wrap the Scriptable value in a JSObject.
        if (Context.useJSObject && jsObjectClass != null && 
            value instanceof Scriptable)
            {
                if (Scriptable.class.isAssignableFrom(type))
                    return value;
                try {
                    Object ctorArgs[] = { value };
                    return jsObjectCtor.newInstance(ctorArgs);
                } catch (InstantiationException instEx) {
                    throw new EvaluatorException("error generating JSObject wrapper for " +
                                                 value);
                } catch (IllegalArgumentException argEx) {
                    throw new EvaluatorException("JSObject constructor doesn't want [Scriptable]!");
                } catch (InvocationTargetException e) {
                    throw WrappedException.wrapException(e);
                } catch (IllegalAccessException accessEx) {
                    throw new EvaluatorException("JSObject constructor is protected/private!");
                }
            }
        
        if (ScriptRuntime.NumberClass.isInstance(value))
            return new Double(((Number) value).doubleValue());

        return value;
    }
    
    public static void initJSObject() {
        if (!Context.useJSObject)
            return;
        // if netscape.javascript.JSObject is in the CLASSPATH, enable JSObject
        // compatability wrappers
        jsObjectClass = null;
        try {
            jsObjectClass = Class.forName("netscape.javascript.JSObject");
            Class ctorParms[] = { ScriptRuntime.ScriptableClass };
            jsObjectCtor = jsObjectClass.getConstructor(ctorParms);
            jsObjectGetScriptable = jsObjectClass.getMethod("getScriptable", 
                                                            new Class[0]);
        } catch (ClassNotFoundException classEx) {
            // jsObjectClass already null
        } catch (NoSuchMethodException methEx) {
            // jsObjectClass already null
        }
    }
        
    protected Object javaObject;
    protected JavaMembers members;
    private Hashtable fieldAndMethods;
    static Class jsObjectClass;
    static Constructor jsObjectCtor;
    static Method jsObjectGetScriptable;

}

