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
 * Frank Mitchell
 * Mike Shaver
 * Kemal Bayram
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

import java.io.*;
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

public class NativeJavaObject implements Scriptable, Wrapper, Externalizable {

    public NativeJavaObject() { }

    public NativeJavaObject(Scriptable scope, Object javaObject,
                            Class staticType)
    {
        this.parent = scope;
        this.javaObject = javaObject;
        this.staticType = staticType;
        initMembers();
    }

    protected void initMembers() {
        Class dynamicType;
        if (javaObject != null) {
            dynamicType = javaObject.getClass();
        } else {
            dynamicType = staticType;
        }
        members = JavaMembers.lookupClass(parent, dynamicType, staticType);
        fieldAndMethods
            = members.getFieldAndMethodsObjects(this, javaObject, false);
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
            if (result != null) {
                return result;
            }
        }
        // TODO: passing 'this' as the scope is bogus since it has
        //  no parent scope
        return members.get(this, name, javaObject, false);
    }

    public Object get(int index, Scriptable start) {
        throw members.reportMemberNotFound(Integer.toString(index));
    }

    public void put(String name, Scriptable start, Object value) {
        // We could be asked to modify the value of a property in the
        // prototype. Since we can't add a property to a Java object,
        // we modify it in the prototype rather than copy it down.
        if (prototype == null || members.has(name, false))
            members.put(this, name, javaObject, value, false);
        else
            prototype.put(name, prototype, value);
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
        if (prototype == null && javaObject instanceof String) {
            return ScriptableObject.getClassPrototype(parent, "String");
        }
        return prototype;
    }

    /**
     * Sets the prototype of the object.
     */
    public void setPrototype(Scriptable m) {
        prototype = m;
    }

    /**
     * Returns the parent (enclosing) scope of the object.
     */
    public Scriptable getParentScope() {
        return parent;
    }

    /**
     * Sets the parent (enclosing) scope of the object.
     */
    public void setParentScope(Scriptable m) {
        parent = m;
    }

    public Object[] getIds() {
        return members.getIds(false);
    }

/**
@deprecated Use {@link Context#getWrapFactory()} together with calling {@link
WrapFactory#wrap(Context cx, Scriptable scope, Object obj, Class)}
*/
    public static Object wrap(Scriptable scope, Object obj, Class staticType) {

        Context cx = Context.getContext();
        return cx.getWrapFactory().wrap(cx, scope, obj, staticType);
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
                      this, ScriptRuntime.emptyArgs);
    }

    Object callConverter(String converterName)
        throws JavaScriptException
    {
        Function converter = getConverter(converterName);
        if (converter == null) {
            return javaObject.toString();
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
        throw Context.reportRuntimeError0("msg.default.value");
    }


    /**
     * Determine whether we can/should convert between the given type and the
     * desired one.  This should be superceded by a conversion-cost calculation
     * function, but for now I'll hide behind precedent.
     */
    public static boolean canConvert(Object fromObj, Class to) {
        int weight = NativeJavaObject.getConversionWeight(fromObj, to);

        return (weight < CONVERSION_NONE);
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
            if (to == ScriptRuntime.StringClass ||
                to == ScriptRuntime.ObjectClass) {
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
            else if (to == ScriptRuntime.BooleanClass) {
                result = 2;
            }
            else if (to == ScriptRuntime.ObjectClass) {
                result = 3;
            }
            else if (to == ScriptRuntime.StringClass) {
                result = 4;
            }
            break;

        case JSTYPE_NUMBER:
            if (to.isPrimitive()) {
                if (to == Double.TYPE) {
                    result = 1;
                }
                else if (to != Boolean.TYPE) {
                    result = 1 + NativeJavaObject.getSizeRank(to);
                }
            }
            else {
                if (to == ScriptRuntime.StringClass) {
                    // native numbers are #1-8
                    result = 9;
                }
                else if (to == ScriptRuntime.ObjectClass) {
                    result = 10;
                }
                else if (ScriptRuntime.NumberClass.isAssignableFrom(to)) {
                    // "double" is #1
                    result = 2;
                }
            }
            break;

        case JSTYPE_STRING:
            if (to == ScriptRuntime.StringClass) {
                result = 1;
            }
            else if (to == ScriptRuntime.ObjectClass ||
                     to == ScriptRuntime.SerializableClass ||
                     to == ScriptRuntime.ComparableClass)
            {
                result = 2;
            }
            else if (to.isPrimitive() && to != Boolean.TYPE) {
                if (to == Character.TYPE) {
                    result = 3;
                }
                else {
                    result = 4;
                }
            }
            break;

        case JSTYPE_JAVA_CLASS:
            if (to == ScriptRuntime.ClassClass) {
                result = 1;
            }
            else if (to == ScriptRuntime.ObjectClass) {
                result = 3;
            }
            else if (to == ScriptRuntime.StringClass) {
                result = 4;
            }
            break;

        case JSTYPE_JAVA_OBJECT:
        case JSTYPE_JAVA_ARRAY:
            if (to == ScriptRuntime.StringClass) {
                result = 2;
            }
            else if (to.isPrimitive() && to != Boolean.TYPE) {
                result =
                    (fromCode == JSTYPE_JAVA_ARRAY) ?
                    CONVERSION_NONTRIVIAL :
                    2 + NativeJavaObject.getSizeRank(to);
            }
            else {
                Object javaObj = fromObj;
                if (javaObj instanceof Wrapper) {
                    javaObj = ((Wrapper)javaObj).unwrap();
                }
                if (to.isInstance(javaObj)) {
                    result = CONVERSION_NONTRIVIAL;
                }
            }
            break;

        case JSTYPE_OBJECT:
            // Other objects takes #1-#3 spots
            if (fromObj instanceof NativeArray && to.isArray()) {
                // This is a native array conversion to a java array
                // Array conversions are all equal, and preferable to object
                // and string conversion, per LC3.
                result = 1;
            }
            else if (to == ScriptRuntime.ObjectClass) {
                result = 2;
            }
            else if (to == ScriptRuntime.StringClass) {
                result = 3;
            }
            else if (to.isPrimitive() || to != Boolean.TYPE) {
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
        else if (aType == Boolean.TYPE) {
            return CONVERSION_NONE;
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

            if (valueClass == ScriptRuntime.StringClass) {
                return JSTYPE_STRING;
            }
            else if (valueClass == ScriptRuntime.BooleanClass) {
                return JSTYPE_BOOLEAN;
            }
            else if (value instanceof Number) {
                return JSTYPE_NUMBER;
            }
            else if (valueClass == ScriptRuntime.ClassClass) {
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
     * Not intended for public use. Callers should use the
     * public API Context.toType.
     * @see org.mozilla.javascript.Context#toType.
     * @deprecated as of 1.5 Release 4
     */
    public static Object coerceType(Class type, Object value) {
        return coerceType(type, value, true);
    }

    /**
     * Type-munging for field setting and method invocation.
     * Conforms to LC3 specification
     */
    static Object coerceType(Class type, Object value,
                             boolean useErrorHandler)
    {
        if (value != null && value.getClass() == type) {
            return value;
        }

        switch (NativeJavaObject.getJSTypeCode(value)) {

        case JSTYPE_NULL:
            // raise error if type.isPrimitive()
            if (type.isPrimitive()) {
                reportConversionError(value, type, !useErrorHandler);
            }
            return null;

        case JSTYPE_UNDEFINED:
            if (type == ScriptRuntime.StringClass ||
                type == ScriptRuntime.ObjectClass) {
                return "undefined";
            }
            else {
                reportConversionError("undefined", type, !useErrorHandler);
            }
            break;

        case JSTYPE_BOOLEAN:
            // Under LC3, only JS Booleans can be coerced into a Boolean value
            if (type == Boolean.TYPE ||
                type == ScriptRuntime.BooleanClass ||
                type == ScriptRuntime.ObjectClass) {
                return value;
            }
            else if (type == ScriptRuntime.StringClass) {
                return value.toString();
            }
            else {
                reportConversionError(value, type, !useErrorHandler);
            }
            break;

        case JSTYPE_NUMBER:
            if (type == ScriptRuntime.StringClass) {
                return ScriptRuntime.toString(value);
            }
            else if (type == ScriptRuntime.ObjectClass) {
                return coerceToNumber(Double.TYPE, value, useErrorHandler);
            }
            else if ((type.isPrimitive() && type != Boolean.TYPE) ||
                     ScriptRuntime.NumberClass.isAssignableFrom(type)) {
                return coerceToNumber(type, value, useErrorHandler);
            }
            else {
                reportConversionError(value, type, !useErrorHandler);
            }
            break;

        case JSTYPE_STRING:
            if (type == ScriptRuntime.StringClass ||
                type == ScriptRuntime.ObjectClass ||
                type == ScriptRuntime.SerializableClass ||
                type == ScriptRuntime.ComparableClass) {
                return value;
            }
            else if (type == Character.TYPE ||
                     type == ScriptRuntime.CharacterClass) {
                // Special case for converting a single char string to a
                // character
                // Placed here because it applies *only* to JS strings,
                // not other JS objects converted to strings
                if (((String)value).length() == 1) {
                    return new Character(((String)value).charAt(0));
                }
                else {
                    return coerceToNumber(type, value, useErrorHandler);
                }
            }
            else if ((type.isPrimitive() && type != Boolean.TYPE) ||
                     ScriptRuntime.NumberClass.isAssignableFrom(type)) {
                return coerceToNumber(type, value, useErrorHandler);
            }
            else {
                reportConversionError(value, type, !useErrorHandler);
            }
            break;

        case JSTYPE_JAVA_CLASS:
            if (value instanceof Wrapper) {
                value = ((Wrapper)value).unwrap();
            }

            if (type == ScriptRuntime.ClassClass ||
                type == ScriptRuntime.ObjectClass) {
                return value;
            }
            else if (type == ScriptRuntime.StringClass) {
                return value.toString();
            }
            else {
                reportConversionError(value, type, !useErrorHandler);
            }
            break;

        case JSTYPE_JAVA_OBJECT:
        case JSTYPE_JAVA_ARRAY:
            if (type.isPrimitive()) {
                if (type == Boolean.TYPE) {
                    reportConversionError(value, type, !useErrorHandler);
                }
                return coerceToNumber(type, value, useErrorHandler);
            }
            else {
                if (value instanceof Wrapper) {
                    value = ((Wrapper)value).unwrap();
                }
                if (type == ScriptRuntime.StringClass) {
                    return value.toString();
                }
                else {
                    if (type.isInstance(value)) {
                        return value;
                    }
                    else {
                        reportConversionError(value, type, !useErrorHandler);
                    }
                }
            }
            break;

        case JSTYPE_OBJECT:
            if (type == ScriptRuntime.StringClass) {
                return ScriptRuntime.toString(value);
            }
            else if (type.isPrimitive()) {
                if (type == Boolean.TYPE) {
                    reportConversionError(value, type, !useErrorHandler);
                }
                return coerceToNumber(type, value, useErrorHandler);
            }
            else if (type.isInstance(value)) {
                return value;
            }
            else if (type.isArray() && value instanceof NativeArray) {
                // Make a new java array, and coerce the JS array components
                // to the target (component) type.
                NativeArray array = (NativeArray) value;
                long length = array.getLength();
                Class arrayType = type.getComponentType();
                Object Result = Array.newInstance(arrayType, (int)length);
                for (int i = 0 ; i < length ; ++i) {
                    try  {
                        Array.set(Result, i, coerceType(arrayType,
                                                        array.get(i, array),
                                                        useErrorHandler));
                    }
                    catch (EvaluatorException ee) {
                        reportConversionError(value, type, !useErrorHandler);
                    }
                }

                return Result;
            }
            else if (value instanceof Wrapper) {
                value = ((Wrapper)value).unwrap();
                if (type.isInstance(value))
                    return value;
                reportConversionError(value, type, !useErrorHandler);
            }
            else {
                reportConversionError(value, type, !useErrorHandler);
            }
            break;
        }

        return value;
    }

    static Object coerceToNumber(Class type, Object value, boolean useErrorHandler) {
        Class valueClass = value.getClass();

        // Character
        if (type == Character.TYPE || type == ScriptRuntime.CharacterClass) {
            if (valueClass == ScriptRuntime.CharacterClass) {
                return value;
            }
            return new Character((char)toInteger(value,
                                                 ScriptRuntime.CharacterClass,
                                                 (double)Character.MIN_VALUE,
                                                 (double)Character.MAX_VALUE,
                                                 useErrorHandler));
        }

        // Double, Float
        if (type == ScriptRuntime.ObjectClass ||
            type == ScriptRuntime.DoubleClass || type == Double.TYPE) {
            return valueClass == ScriptRuntime.DoubleClass
                ? value
                : new Double(toDouble(value, useErrorHandler));
        }

        if (type == ScriptRuntime.FloatClass || type == Float.TYPE) {
            if (valueClass == ScriptRuntime.FloatClass) {
                return value;
            }
            else {
                double number = toDouble(value, useErrorHandler);
                if (Double.isInfinite(number) || Double.isNaN(number)
                    || number == 0.0) {
                    return new Float((float)number);
                }
                else {
                    double absNumber = Math.abs(number);
                    if (absNumber < (double)Float.MIN_VALUE) {
                        return new Float((number > 0.0) ? +0.0 : -0.0);
                    }
                    else if (absNumber > (double)Float.MAX_VALUE) {
                        return new Float((number > 0.0) ?
                                         Float.POSITIVE_INFINITY :
                                         Float.NEGATIVE_INFINITY);
                    }
                    else {
                        return new Float((float)number);
                    }
                }
            }
        }

        // Integer, Long, Short, Byte
        if (type == ScriptRuntime.IntegerClass || type == Integer.TYPE) {
            if (valueClass == ScriptRuntime.IntegerClass) {
                return value;
            }
            else {
                return new Integer((int)toInteger(value,
                                                  ScriptRuntime.IntegerClass,
                                                  (double)Integer.MIN_VALUE,
                                                  (double)Integer.MAX_VALUE,
                                                  useErrorHandler));
            }
        }

        if (type == ScriptRuntime.LongClass || type == Long.TYPE) {
            if (valueClass == ScriptRuntime.LongClass) {
                return value;
            }
            else {
                /* Long values cannot be expressed exactly in doubles.
                 * We thus use the largest and smallest double value that
                 * has a value expressible as a long value. We build these
                 * numerical values from their hexidecimal representations
                 * to avoid any problems caused by attempting to parse a
                 * decimal representation.
                 */
                final double max = Double.longBitsToDouble(0x43dfffffffffffffL);
                final double min = Double.longBitsToDouble(0xc3e0000000000000L);
                return new Long(toInteger(value,
                                          ScriptRuntime.LongClass,
                                          min,
                                          max,
                                          useErrorHandler));
            }
        }

        if (type == ScriptRuntime.ShortClass || type == Short.TYPE) {
            if (valueClass == ScriptRuntime.ShortClass) {
                return value;
            }
            else {
                return new Short((short)toInteger(value,
                                                  ScriptRuntime.ShortClass,
                                                  (double)Short.MIN_VALUE,
                                                  (double)Short.MAX_VALUE,
                                                  useErrorHandler));
            }
        }

        if (type == ScriptRuntime.ByteClass || type == Byte.TYPE) {
            if (valueClass == ScriptRuntime.ByteClass) {
                return value;
            }
            else {
                return new Byte((byte)toInteger(value,
                                                ScriptRuntime.ByteClass,
                                                (double)Byte.MIN_VALUE,
                                                (double)Byte.MAX_VALUE,
                                                useErrorHandler));
            }
        }

        return new Double(toDouble(value, useErrorHandler));
    }


    static double toDouble(Object value, boolean useErrorHandler) {
        if (value instanceof Number) {
            return ((Number)value).doubleValue();
        }
        else if (value instanceof String) {
            return ScriptRuntime.toNumber((String)value);
        }
        else if (value instanceof Scriptable) {
            if (value instanceof Wrapper) {
                // XXX: optimize tail-recursion?
                return toDouble(((Wrapper)value).unwrap(), useErrorHandler);
            }
            else {
                return ScriptRuntime.toNumber(value);
            }
        }
        else {
            Method meth;
            try {
                meth = value.getClass().getMethod("doubleValue", null);
            }
            catch (NoSuchMethodException e) {
                meth = null;
            }
            catch (SecurityException e) {
                meth = null;
            }
            if (meth != null) {
                try {
                    return ((Number)meth.invoke(value, null)).doubleValue();
                }
                catch (IllegalAccessException e) {
                    // XXX: ignore, or error message?
                    reportConversionError(value, Double.TYPE, !useErrorHandler);
                }
                catch (InvocationTargetException e) {
                    // XXX: ignore, or error message?
                    reportConversionError(value, Double.TYPE, !useErrorHandler);
                }
            }
            return ScriptRuntime.toNumber(value.toString());
        }
    }

    static long toInteger(Object value, Class type, double min, double max,
                          boolean useErrorHandler)
    {
        double d = toDouble(value, useErrorHandler);

        if (Double.isInfinite(d) || Double.isNaN(d)) {
            // Convert to string first, for more readable message
            reportConversionError(ScriptRuntime.toString(value), type,
                                  !useErrorHandler);
        }

        if (d > 0.0) {
            d = Math.floor(d);
        }
        else {
            d = Math.ceil(d);
        }

        if (d < min || d > max) {
            // Convert to string first, for more readable message
            reportConversionError(ScriptRuntime.toString(value), type,
                                  !useErrorHandler);
        }
        return (long)d;
    }

    static void reportConversionError(Object value, Class type,
                                      boolean throwIllegalArg)
    {
        if (throwIllegalArg) {
            throw new IllegalArgumentException("Cannot convert " + value +
                                               " to type " + type);
        }
        throw Context.reportRuntimeError2
            ("msg.conversion.not.allowed",
             value.toString(), NativeJavaMethod.javaSignature(type));
    }

    public void writeExternal(ObjectOutput out)
        throws IOException
    {
        out.writeObject(prototype);
        out.writeObject(parent);

        if (javaObject != null) {
            Class joClass = javaObject.getClass();
            if (joClass.getName().startsWith("adapter")) {

                out.writeBoolean(true);
                out.writeObject(joClass.getSuperclass().getName());

                Class[] interfaces = joClass.getInterfaces();
                String[] interfaceNames = new String[interfaces.length];

                for (int i=0; i < interfaces.length; i++)
                    interfaceNames[i] = interfaces[i].getName();

                out.writeObject(interfaceNames);

                try {
                    Object delegee
                        = joClass.getField("delegee").get(javaObject);
                    Object self
                        = joClass.getField("self").get(javaObject);
                    out.writeObject(delegee);
                    out.writeObject(self);
                } catch (IllegalAccessException e) {
                } catch (NoSuchFieldException e) {
                }
            } else {
                out.writeBoolean(false);
                out.writeObject(javaObject);
            }
        } else {
            out.writeBoolean(false);
            out.writeObject(javaObject);
        }

        if (staticType != null) {
            out.writeObject(staticType.getClass().getName());
        } else {
            out.writeObject(null);
        }
    }

    public void readExternal(ObjectInput in)
        throws IOException, ClassNotFoundException
    {
        prototype = (Scriptable)in.readObject();
        parent = (Scriptable)in.readObject();

        if (in.readBoolean()) {
            Class superclass = Class.forName((String)in.readObject());

            String[] interfaceNames = (String[])in.readObject();
            Class[] interfaces = new Class[interfaceNames.length];

            for (int i=0; i < interfaceNames.length; i++)
                interfaces[i] = Class.forName(interfaceNames[i]);

            javaObject = JavaAdapter.createAdapterClass(superclass, interfaces,
                (Scriptable)in.readObject(), (Scriptable)in.readObject());
        } else {
            javaObject = in.readObject();
        }

        String className = (String)in.readObject();
        if (className != null) {
            staticType = Class.forName(className);
        } else {
            staticType = null;
        }

        initMembers();
    }

    /**
     * The prototype of this object.
     */
    protected Scriptable prototype;

    /**
     * The parent scope of this object.
     */
    protected Scriptable parent;

    protected Object javaObject;

    protected Class staticType;
    protected JavaMembers members;
    private Hashtable fieldAndMethods;

}
