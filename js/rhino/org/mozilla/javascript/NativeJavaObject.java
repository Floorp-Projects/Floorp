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
        if (fromObj == null)
            return true;
        // XXX nontrivial conversions?
        return getConversionWeight(fromObj, to) == CONVERSION_TRIVIAL;
    }

    public static final int CONVERSION_NONE         = 0;
    public static final int CONVERSION_TRIVIAL      = 1;
    public static final int CONVERSION_NONTRIVIAL   = 2;

    public static int getConversionWeight(Object fromObj, Class to) {
        Class from = fromObj.getClass();

        // if to is a primitive, from must be assignableFrom
        // the wrapper class.
        if (to.isPrimitive()) {
            if (ScriptRuntime.NumberClass.isAssignableFrom(from))
                return CONVERSION_TRIVIAL;
            if (to == Boolean.TYPE)
                return from == ScriptRuntime.BooleanClass
                       ? CONVERSION_TRIVIAL
                       : CONVERSION_NONE;
            // Allow String to convert to Character if length() == 1
            if (to == Character.TYPE) {
                if (from.isAssignableFrom(ScriptRuntime.CharacterClass) ||
                    (from == ScriptRuntime.StringClass &&
                     ((String) fromObj).length() == 1))
                {
                    return CONVERSION_TRIVIAL;
                }
                return CONVERSION_NONE;
            }
            return CONVERSION_NONTRIVIAL;
        }
        if (to == ScriptRuntime.CharacterClass) {
            if (from.isAssignableFrom(ScriptRuntime.CharacterClass) ||
                (from == ScriptRuntime.StringClass &&
                ((String) fromObj).length() == 1))
            {
                return CONVERSION_TRIVIAL;
            }
            return CONVERSION_NONE;
        }
        if (to == ScriptRuntime.StringClass) {
            return CONVERSION_TRIVIAL;
        }
        if (Context.useJSObject && jsObjectClass != null && 
            jsObjectClass.isAssignableFrom(to) && 
            Scriptable.class.isAssignableFrom(from))
        {
            // Convert a Scriptable to a JSObject.
            return CONVERSION_TRIVIAL;
        }
        return to.isAssignableFrom(from) ? CONVERSION_TRIVIAL : CONVERSION_NONE;
    }

    /**
     * Type-munging for field setting and method invocation.
     * This is traditionally the least fun part of getting LiveConnect right.
     * I hope we've got good tests.... =/<p>
     *
     * Assumptions:<br>
     * <ul>
     * <li> we should convert fields very aggressively. e.g: a String sent to an
     *      int slot gets toInteger()ed and then assigned.  Invalid strings
     *      become NaN and then 0.
     * <li> we only get called when we <b>must</b> make this coercion.  Don't
     *      use this unless you already know that the type conversion is really
     *      what you want.
     * </ul>
     */
    public static Object coerceType(Class type, Object value) {
        // Don't coerce null to a string (or other object)
        if (value == null)
            return null;
            
        // For final classes we can compare valueClass to a class object
        // rather than using instanceof
        Class valueClass = value.getClass();
        
        // Is value already of the correct type?
        if (valueClass == type)
            return value;

        // String
        if (type == ScriptRuntime.StringClass)
            return ScriptRuntime.toString(value);

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

