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

import java.lang.reflect.*;
import java.util.Hashtable;

/**
 * This is the default implementation of the Scriptable interface. This
 * class provides convenient default behavior that makes it easier to
 * define host objects.
 * <p>
 * Various properties and methods of JavaScript objects can be conveniently
 * defined using methods of ScriptableObject.
 * <p>
 * Classes extending ScriptableObject must define the getClassName method.
 *
 * @see org.mozilla.javascript.Scriptable
 * @author Norris Boyd
 */

public abstract class ScriptableObject implements Scriptable {

    /**
     * The empty property attribute.
     *
     * Used by getAttributes() and setAttributes().
     *
     * @see org.mozilla.javascript.ScriptableObject#getAttributes
     * @see org.mozilla.javascript.ScriptableObject#setAttributes
     */
    public static final int EMPTY =     0x00;

    /**
     * Property attribute indicating assignment to this property is ignored.
     *
     * @see org.mozilla.javascript.ScriptableObject#put
     * @see org.mozilla.javascript.ScriptableObject#getAttributes
     * @see org.mozilla.javascript.ScriptableObject#setAttributes
     */
    public static final int READONLY =  0x01;

    /**
     * Property attribute indicating property is not enumerated.
     *
     * Only enumerated properties will be returned by getIds().
     *
     * @see org.mozilla.javascript.ScriptableObject#getIds
     * @see org.mozilla.javascript.ScriptableObject#getAttributes
     * @see org.mozilla.javascript.ScriptableObject#setAttributes
     */
    public static final int DONTENUM =  0x02;

    /**
     * Property attribute indicating property cannot be deleted.
     *
     * @see org.mozilla.javascript.ScriptableObject#delete
     * @see org.mozilla.javascript.ScriptableObject#getAttributes
     * @see org.mozilla.javascript.ScriptableObject#setAttributes
     */
    public static final int PERMANENT = 0x04;

    /**
     * Return the name of the class.
     *
     * This is typically the same name as the constructor.
     * Classes extending ScriptableObject must implement this abstract
     * method.
     */
    public abstract String getClassName();

    /**
     * Returns true if the named property is defined.
     *
     * @param name the name of the property
     * @param start the object in which the lookup began
     * @return true if and only if the property was found in the object
     */
    public boolean has(String name, Scriptable start) {
        return getSlot(name, name.hashCode()) != SLOT_NOT_FOUND;
    }

    /**
     * Returns true if the property index is defined.
     *
     * @param index the numeric index for the property
     * @param start the object in which the lookup began
     * @return true if and only if the property was found in the object
     */
    public boolean has(int index, Scriptable start) {
        return getSlot(null, index) != SLOT_NOT_FOUND;
    }

    /**
     * Returns the value of the named property or NOT_FOUND.
     *
     * If the property was created using defineProperty, the
     * appropriate getter method is called.
     *
     * @param name the name of the property
     * @param start the object in which the lookup began
     * @return the value of the property (may be null), or NOT_FOUND
     */

    public Object get(String name, Scriptable start) {
        int hashCode;
        if (name == lastName) {
            if (lastValue != REMOVED)
                return lastValue;
            hashCode = lastHash;
        } else {
            hashCode = name.hashCode(); 
        }
        int slotIndex = getSlot(name, hashCode);
        if (slotIndex == SLOT_NOT_FOUND)
            return Scriptable.NOT_FOUND;
        Slot slot = slots[slotIndex];
        if ((slot.flags & Slot.HAS_GETTER) != 0) {
            GetterSlot getterSlot = (GetterSlot) slot;
            try {
                if (getterSlot.delegateTo == null) {
                    // Walk the prototype chain to find an appropriate
                    // object to invoke the getter on.
                    Class clazz = getterSlot.getter.getDeclaringClass();
                    while (!clazz.isInstance(start)) {
                        start = start.getPrototype();
                        if (start == null) {
                            start = this;
                            break;
                        }
                    }
                    return getterSlot.getter.invoke(start, ScriptRuntime.emptyArgs);
                }
                Object[] args = { this };
                return getterSlot.getter.invoke(getterSlot.delegateTo, args);
            }
            catch (InvocationTargetException e) {
                throw WrappedException.wrapException(e);
            }
            catch (IllegalAccessException e) {
                throw WrappedException.wrapException(e);
            }
        }
        // Update cache. Assign REMOVED to invalidate the cache while
        // we update it in case there are accesses from another thread.
        lastValue = REMOVED;
        lastName = name;
        lastHash = hashCode;
        lastValue = slot.value;
        return lastValue;
    }

    /**
     * Returns the value of the indexed property or NOT_FOUND.
     *
     * @param index the numeric index for the property
     * @param start the object in which the lookup began
     * @return the value of the property (may be null), or NOT_FOUND
     */
    public Object get(int index, Scriptable start) {
        int slotIndex = getSlot(null, index);
        if (slotIndex == SLOT_NOT_FOUND)
            return Scriptable.NOT_FOUND;
        return slots[slotIndex].value;
    }

    /**
     * Sets the value of the named property, creating it if need be.
     *
     * If the property was created using defineProperty, the
     * appropriate setter method is called. <p>
     *
     * If the property's attributes include READONLY, no action is
     * taken.
     * This method will actually set the property in the start
     * object.
     *
     * @param name the name of the property
     * @param start the object whose property is being set
     * @param value value to set the property to
     */
    public void put(String name, Scriptable start, Object value) {
        int hash = name.hashCode();
        int slotIndex = getSlot(name, hash);
        if (slotIndex == SLOT_NOT_FOUND) {
            if (start != this) {
                start.put(name, start, value);
                return;
            }
            slotIndex = getSlotToSet(name, hash, false);
        }
        Slot slot = slots[slotIndex];
        if ((slot.attributes & ScriptableObject.READONLY) != 0)
            return;
        if ((slot.flags & Slot.HAS_SETTER) != 0) {
            GetterSlot getterSlot = (GetterSlot) slot;
            try {
                if (getterSlot.delegateTo == null) {
                    // Walk the prototype chain to find an appropriate
                    // object to invoke the setter on.
                    Object[] arg = { value };
                    Class clazz = getterSlot.setter.getDeclaringClass();
                    while (!clazz.isInstance(start)) {
                        start = start.getPrototype();
                        if (start == null) {
                            start = this;
                            break;
                        }
                    }
                    getterSlot.setter.invoke(start, arg);
                    return;
                }
                Object[] args = { this, value };
                getterSlot.setter.invoke(getterSlot.delegateTo, args);
                return;
            }
            catch (InvocationTargetException e) {
                throw WrappedException.wrapException(e);
            }
            catch (IllegalAccessException e) {
                throw WrappedException.wrapException(e);
            }
        }
        if (this == start) {
            slot.value = value;
            // It could be the case that name.equals(lastName)
            // even if name != lastName. However, it is more
            // expensive to call String.equals than it is to
            // just invalidate the cache.
            lastValue = name == lastName ? value : REMOVED;
        } else {
            start.put(name, start, value);
        }
    }

    /**
     * Sets the value of the indexed property, creating it if need be.
     *
     * @param index the numeric index for the property
     * @param start the object whose property is being set
     * @param value value to set the property to
     */
    public void put(int index, Scriptable start, Object value) {
        int slotIndex = getSlot(null, index);
        if (slotIndex == SLOT_NOT_FOUND) {
            if (start != this) {
                start.put(index, start, value);
                return;
            }
            slotIndex = getSlotToSet(null, index, false);
        }
        Slot slot = slots[slotIndex];
        if ((slot.attributes & ScriptableObject.READONLY) != 0)
            return;
        if (this == start) {
            slot.value = value;
        } else {
            start.put(index, start, value);
        }
    }

    /**
     * Removes a named property from the object.
     *
     * If the property is not found, or it has the PERMANENT attribute,
     * no action is taken.
     *
     * @param name the name of the property
     */
    public void delete(String name) {
        if (name.equals(lastName))
            lastValue = REMOVED;   // invalidate cache
        removeSlot(name, name.hashCode());
    }

    /**
     * Removes the indexed property from the object.
     *
     * If the property is not found, or it has the PERMANENT attribute,
     * no action is taken.
     *
     * @param index the numeric index for the property
     */
    public void delete(int index) {
        removeSlot(null, index);
    }

    /**
     * Get the attributes of a named property.
     *
     * The property is specified by <code>name</code>
     * as defined for <code>has</code>.<p>
     *
     * @param name the identifier for the property
     * @param start the object in which the lookup began
     * @return the bitset of attributes
     * @exception PropertyException if the named property
     *            is not found
     * @see org.mozilla.javascript.ScriptableObject#has
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#DONTENUM
     * @see org.mozilla.javascript.ScriptableObject#PERMANENT
     * @see org.mozilla.javascript.ScriptableObject#EMPTY
     */
    public int getAttributes(String name, Scriptable start)
        throws PropertyException
    {
        int slotIndex = getSlot(name, name.hashCode());
        if (slotIndex == SLOT_NOT_FOUND) {
            throw new PropertyException(
                Context.getMessage("msg.prop.not.found", null));
        }
        return slots[slotIndex].attributes;
    }

    /**
     * Get the attributes of an indexed property.
     *
     * @param index the numeric index for the property
     * @param start the object in which the lookup began
     * @exception PropertyException if the indexed property
     *            is not found
     * @return the bitset of attributes
     * @see org.mozilla.javascript.ScriptableObject#has
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#DONTENUM
     * @see org.mozilla.javascript.ScriptableObject#PERMANENT
     * @see org.mozilla.javascript.ScriptableObject#EMPTY
     */
    public int getAttributes(int index, Scriptable start)
        throws PropertyException
    {
        int slotIndex = getSlot(null, index);
        if (slotIndex == SLOT_NOT_FOUND) {
            throw new PropertyException(
                Context.getMessage("msg.prop.not.found", null));
        }
        return slots[slotIndex].attributes;
    }

    /**
     * Set the attributes of a named property.
     *
     * The property is specified by <code>name</code>
     * as defined for <code>has</code>.<p>
     *
     * The possible attributes are READONLY, DONTENUM,
     * and PERMANENT. Combinations of attributes
     * are expressed by the bitwise OR of attributes.
     * EMPTY is the state of no attributes set. Any unused
     * bits are reserved for future use.
     *
     * @param name the name of the property
     * @param start the object in which the lookup began
     * @param attributes the bitset of attributes
     * @exception PropertyException if the named property
     *            is not found
     * @see org.mozilla.javascript.Scriptable#has
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#DONTENUM
     * @see org.mozilla.javascript.ScriptableObject#PERMANENT
     * @see org.mozilla.javascript.ScriptableObject#EMPTY
     */
    public void setAttributes(String name, Scriptable start,
                              int attributes)
        throws PropertyException
    {
        final int mask = READONLY | DONTENUM | PERMANENT;
        attributes &= mask; // mask out unused bits
        int slotIndex = getSlot(name, name.hashCode());
        if (slotIndex == SLOT_NOT_FOUND) {
            throw new PropertyException(
                Context.getMessage("msg.prop.not.found", null));
        }
        slots[slotIndex].attributes = (short) attributes;
    }

    /**
     * Set the attributes of an indexed property.
     *
     * @param index the numeric index for the property
     * @param start the object in which the lookup began
     * @param attributes the bitset of attributes
     * @exception PropertyException if the indexed property
     *            is not found
     * @see org.mozilla.javascript.Scriptable#has
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#DONTENUM
     * @see org.mozilla.javascript.ScriptableObject#PERMANENT
     * @see org.mozilla.javascript.ScriptableObject#EMPTY
     */
    public void setAttributes(int index, Scriptable start,
                              int attributes)
        throws PropertyException
    {
        int slotIndex = getSlot(null, index);
        if (slotIndex == SLOT_NOT_FOUND) {
            throw new PropertyException(
                Context.getMessage("msg.prop.not.found", null));
        }
        slots[slotIndex].attributes = (short) attributes;
    }

    /**
     * Returns the prototype of the object.
     */
    public Scriptable getPrototype() {
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

    /**
     * Returns an array of ids for the properties of the object.
     *
     * <p>Any properties with the attribute DONTENUM are not listed. <p>
     *
     * @return an array of java.lang.Objects with an entry for every
     * listed property. Properties accessed via an integer index will 
     * have a corresponding
     * Integer entry in the returned array. Properties accessed by
     * a String will have a String entry in the returned array.
     */
    public Object[] getIds() {
        return getIds(false);
    }
    
    /**
     * Returns an array of ids for the properties of the object.
     *
     * <p>All properties, even those with attribute DONTENUM, are listed. <p>
     *
     * @return an array of java.lang.Objects with an entry for every
     * listed property. Properties accessed via an integer index will 
     * have a corresponding
     * Integer entry in the returned array. Properties accessed by
     * a String will have a String entry in the returned array.
     */
    public Object[] getAllIds() {
        return getIds(true);
    }
    
    /**
     * Implements the [[DefaultValue]] internal method.
     *
     * <p>Note that the toPrimitive conversion is a no-op for
     * every type other than Object, for which [[DefaultValue]]
     * is called. See ECMA 9.1.<p>
     *
     * A <code>hint</code> of null means "no hint".
     *
     * @param typeHint the type hint
     * @return the default value for the object
     *
     * See ECMA 8.6.2.6.
     */
    public Object getDefaultValue(Class typeHint) {
        Object val;
        FlattenedObject f = new FlattenedObject(this);
        Context cx = null;
        try {
            for (int i=0; i < 2; i++) {
                if (typeHint == ScriptRuntime.StringClass ? i == 0 : i == 1) {
                    Function fun = getFunctionProperty(f, "toString");
                    if (fun == null)
                        continue;
                    if (cx == null)
                        cx = Context.getContext();
                    val = fun.call(cx, fun.getParentScope(), f.getObject(),
                                   ScriptRuntime.emptyArgs);
                } else {
                    String hint;
                    if (typeHint == null)
                        hint = "undefined";
                    else if (typeHint == ScriptRuntime.StringClass)
                        hint = "string";
                    else if (typeHint == ScriptRuntime.ScriptableClass)
                        hint = "object";
                    else if (typeHint == ScriptRuntime.FunctionClass)
                        hint = "function";
                    else if (typeHint == ScriptRuntime.BooleanClass || 
                                                typeHint == Boolean.TYPE)
                        hint = "boolean";
                    else if (typeHint == ScriptRuntime.NumberClass ||
                             typeHint == ScriptRuntime.ByteClass || 
                             typeHint == Byte.TYPE ||
                             typeHint == ScriptRuntime.ShortClass || 
                             typeHint == Short.TYPE ||
                             typeHint == ScriptRuntime.IntegerClass || 
                             typeHint == Integer.TYPE ||
                             typeHint == ScriptRuntime.FloatClass || 
                             typeHint == Float.TYPE ||
                             typeHint == ScriptRuntime.DoubleClass || 
                             typeHint == Double.TYPE)
                        hint = "number";
                    else {
                        Object[] args = { typeHint.toString() };
                        throw Context.reportRuntimeError(Context.getMessage
                                                         ("msg.invalid.type", args));
                    }
                    Function fun = getFunctionProperty(f, "valueOf");
                    if (fun == null)
                        continue;
                    Object[] args = { hint };
                    if (cx == null)
                        cx = Context.getContext();
                    val = fun.call(cx, fun.getParentScope(), f.getObject(), args);
                }
                if (val != null && (val == Undefined.instance ||
                                    !(val instanceof Scriptable) ||
                                    typeHint == Scriptable.class ||
                                    typeHint == Function.class))
                {
                    return val;
                }
            }
            // fall through to error 
        }
        catch (JavaScriptException jse) {
            // fall through to error 
        }
        Object arg = typeHint == null ? "undefined" : typeHint.toString();
        Object[] args = { arg };
        throw Context.reportRuntimeError(Context.getMessage
                                         ("msg.default.value", args));
    }

    /**
     * Implements the instanceof operator.
     *
     * <p>This operator has been proposed to ECMA.
     *
     * @param instance The value that appeared on the LHS of the instanceof
     *              operator
     * @return true if "this" appears in value's prototype chain
     *
     */
    public boolean hasInstance(Scriptable instance) {
        // Default for JS objects (other than Function) is to do prototype
        // chasing.  This will be overridden in NativeFunction and non-JS objects.

        return ScriptRuntime.jsDelegatesTo(instance, this);
    }

    /**
     * Defines JavaScript objects from a Java class.
     *
     * If the given class has a method
     * <pre>
     * static void init(Scriptable scope);</pre>
     *
     * then it is invoked and no further initialization is done and the
     * result of the invocation will be returned.<p>
     *
     * However, if no such a method is found, then the class's constructors and
     * methods are used to initialize a class in the following manner.<p>
     *
     * First, the zero-parameter constructor of the class is called to
     * create the prototypical instance. If no such constructor exists,
     * a ClassDefinitionException is thrown. <p>
     *
     * Next, all methods are scanned. If any method
     * begins with a special prefix, then only methods with
     * special prefixes are considered for defining functions
     * and properties. These special prefixes are
     * <ul>
     * <li><code>js_</code> for a JavaScript property or function, as
     *     determined by the name of the method
     * <li><code>jsFunction_</code> for a JavaScript function
     * <li><code>jsStaticFunction_</code> for a JavaScript function that 
     *           is a property of the constructor
     * <li><code>jsProperty_</code> for a JavaScript property
     * </ul><p>
     *
     * If no methods begin with any of these prefixes, all methods
     * are added as JavaScript functions or properties except
     * those with names matching the names of methods in
     * <code>org.mozilla.javascript.Function</code>
     * or any of its supertypes. This means that
     * <code>call</code>, which is a method in Function, and
     * <code>get</code>, which is a method in Scriptable
     * (which Function extends), are both excluded from
     * defining JavaScript methods.<p>
     *
     * If after removing any prefixes, a method's name matches the name of
     * the class (determined by calling <code>getClassName()</code>
     * on the prototypical instance), it is considered to define the
     * JavaScript constructor.<p>
     *
     * If the method's name begins with "get" or "set" after
     * any prefix other than "jsFunction_" is removed, the method is
     * considered to define a property. Accesses to the defined property
     * will result in calls to these getter and setter methods. If no
     * setter is defined, the property is defined as READONLY.<p>
     *
     * Otherwise, a JavaScript function is created that will call the
     * given method. This function is then added as a property
     * of the prototypical instance. So if a JavaScript
     * function beginning with "get" or "set" is desired, it must be
     * prefixed with "jsFunction_" to distinquish it from a method
     * defining a property.<p>
     *
     * If no method is found that can serve as constructor, a Java
     * constructor will be selected to serve as the JavaScript
     * constructor in the following manner. If the class has only one
     * Java constructor, that constructor is used to define
     * the JavaScript constructor. If the the class has two constructors,
     * one must be the zero-argument constructor (otherwise an
     * ClassDefinitionException would have already been thrown
     * when the prototypical instance was to be created). In this case
     * the Java constructor with one or more parameters will be used
     * to define the JavaScript constructor. If the class has three
     * or more constructors, an ClassDefinitionException
     * will be thrown.<p>
     *
     * Finally, if there is a method
     * <pre>
     * static void finishInit(Scriptable scope, FunctionObject constructor,
     *                        Scriptable prototype)</pre>
     *
     * it will be called to finish any initialization. The <code>scope</code>
     * argument will be passed, along with the newly created constructor and
     * the newly created prototype.<p>
     *
     * @param scope The scope in which to define the constructor
     * @param clazz The Java class to use to define the JavaScript objects
     *              and properties
     * @exception IllegalAccessException if access is not available
     *            to a reflected class member
     * @exception InstantiationException if unable to instantiate
     *            the named class
     * @exception InvocationTargetException if an exception is thrown
     *            during execution of methods of the named class
     * @exception ClassDefinitionException if an appropriate
     *            constructor cannot be found to create the prototypical
     *            instance
     * @exception PropertyException if getter and setter
     *            methods do not conform to the requirements of the
     *            defineProperty method
     * @see org.mozilla.javascript.Function
     * @see org.mozilla.javascript.FunctionObject
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#defineProperty
     */
    public static void defineClass(Scriptable scope, Class clazz)
        throws IllegalAccessException, InstantiationException,
               InvocationTargetException, ClassDefinitionException,
               PropertyException
    {
        defineClass(scope, clazz, false);
    }
    
    /**
     * Defines JavaScript objects from a Java class, optionally 
     * allowing sealing.
     *
     * Similar to <code>defineClass(Scriptable scope, Class clazz)</code>
     * except that sealing is allowed. An object that is sealed cannot have 
     * properties added or removed. Note that sealing is not allowed in
     * the current ECMA/ISO language specification, but is likely for
     * the next version.
     * 
     * @param scope The scope in which to define the constructor
     * @param clazz The Java class to use to define the JavaScript objects
     *              and properties
     * @param sealed whether or not to create sealed standard objects that
     *               cannot be modified. 
     * @exception IllegalAccessException if access is not available
     *            to a reflected class member
     * @exception InstantiationException if unable to instantiate
     *            the named class
     * @exception InvocationTargetException if an exception is thrown
     *            during execution of methods of the named class
     * @exception ClassDefinitionException if an appropriate
     *            constructor cannot be found to create the prototypical
     *            instance
     * @exception PropertyException if getter and setter
     *            methods do not conform to the requirements of the
     *            defineProperty method
     * @since 1.4R3
     */
    public static void defineClass(Scriptable scope, Class clazz, 
                                   boolean sealed)
        throws IllegalAccessException, InstantiationException,
               InvocationTargetException, ClassDefinitionException,
               PropertyException
    {
        Method[] methods = clazz.getMethods();
        for (int i=0; i < methods.length; i++) {
            if (!methods[i].getName().equals("init"))
                continue;
            Class[] parmTypes = methods[i].getParameterTypes();
            if (parmTypes.length == 1 &&
                parmTypes[0] == ScriptRuntime.ScriptableClass &&
                Modifier.isStatic(methods[i].getModifiers()))
            {
                Object args[] = { scope };
                methods[i].invoke(null, args);
                return;
            }
        }

        // If we got here, there isn't an "init" method with the right
        // parameter types.
        Hashtable exclusionList = getExclusionList();

        Constructor[] ctors = clazz.getConstructors();
        Constructor protoCtor = null;
        for (int i=0; i < ctors.length; i++) {
            if (ctors[i].getParameterTypes().length == 0) {
                protoCtor = ctors[i];
                break;
            }
        }
        if (protoCtor == null) {
            Object[] args = { clazz.getName() };
            throw new ClassDefinitionException(
                    Context.getMessage("msg.zero.arg.ctor", args));
        }

        Scriptable proto = (Scriptable) 
                        protoCtor.newInstance(ScriptRuntime.emptyArgs);
        proto.setPrototype(getObjectPrototype(scope));
        String className = proto.getClassName();

        // Find out whether there are any methods that begin with
        // "js". If so, then only methods that begin with special
        // prefixes will be defined as JavaScript entities.
        final String genericPrefix = "js_";
        final String functionPrefix = "jsFunction_";
        final String staticFunctionPrefix = "jsStaticFunction_";
        final String propertyPrefix = "jsProperty_";

        boolean hasPrefix = false;
        Member ctorMember = null;
        for (int i=0; i < methods.length; i++) {
            String name = methods[i].getName();
            String prefix = null;
            if (!name.startsWith("js")) // common start to all prefixes
                prefix = null;
            else if (name.startsWith(genericPrefix))
                prefix = genericPrefix;
            else if (name.startsWith(functionPrefix))
                prefix = functionPrefix;
            else if (name.startsWith(staticFunctionPrefix))
                prefix = staticFunctionPrefix;
            else if (name.startsWith(propertyPrefix))
                prefix = propertyPrefix;
            if (prefix != null) {
                hasPrefix = true;
                name = name.substring(prefix.length());
            }
            if (name.equals(className)) {
                if (ctorMember != null) {
                    Object[] args = { ctorMember, methods[i] };
                    throw new ClassDefinitionException(
                        Context.getMessage("msg.multiple.ctors", args));
                }
                ctorMember = methods[i];
            }
        }

        if (ctorMember == null) {
            if (ctors.length == 1) {
                ctorMember = ctors[0];
            } else if (ctors.length == 2) {
                if (ctors[0].getParameterTypes().length == 0)
                    ctorMember = ctors[1];
                else if (ctors[1].getParameterTypes().length == 0)
                    ctorMember = ctors[0];
            }
            if (ctorMember == null) {
                Object[] args = { clazz.getName() };
                throw new ClassDefinitionException(
                    Context.getMessage("msg.ctor.multiple.parms", args));
            }
        }

        FunctionObject ctor = new FunctionObject(className, ctorMember, scope);
        ctor.addAsConstructor(scope, proto);

        Method finishInit = null;
        for (int i=0; i < methods.length; i++) {
            if (methods[i].getDeclaringClass() != clazz)
                continue;
            String name = methods[i].getName();
            if (name.equals("finishInit")) {
                Class[] parmTypes = methods[i].getParameterTypes();
                if (parmTypes.length == 3 &&
                    parmTypes[0] == ScriptRuntime.ScriptableClass &&
                    parmTypes[1] == FunctionObject.class &&
                    parmTypes[2] == ScriptRuntime.ScriptableClass &&
                    Modifier.isStatic(methods[i].getModifiers()))
                {
                    finishInit = methods[i];
                    continue;
                }
            }
            String prefix = null;
            if (hasPrefix) {
                if (name.startsWith(genericPrefix)) {
                    name = name.substring(genericPrefix.length());
                    prefix = genericPrefix;
                } else if (name.startsWith(functionPrefix)) {
                    name = name.substring(functionPrefix.length());
                    prefix = functionPrefix;
                } else if (name.startsWith(staticFunctionPrefix)) {
                    name = name.substring(staticFunctionPrefix.length());
                    prefix = staticFunctionPrefix;
                    if (!Modifier.isStatic(methods[i].getModifiers())) {
                        throw new ClassDefinitionException(
                            "jsStaticFunction must be used with static method.");
                    }
                } else if (name.startsWith(propertyPrefix)) {
                    name = name.substring(propertyPrefix.length());
                    prefix = propertyPrefix;
                } else {
                    continue;
                }
            } else if (exclusionList.get(name) != null)
                continue;
            if (methods[i] == ctorMember) {
                continue;
            }
            if ((name.startsWith("get") || name.startsWith("set"))
                && name.length() > 3 &&
                !(hasPrefix && (prefix.equals(functionPrefix) ||
                                prefix.equals(staticFunctionPrefix))))
            {
                if (!(proto instanceof ScriptableObject)) {
                    Object[] args = { proto.getClass().toString(), name };
                    throw new PropertyException(
                        Context.getMessage("msg.extend.scriptable", args));
                }
                if (name.startsWith("set"))
                    continue;   // deal with set when we see get
                StringBuffer buf = new StringBuffer();
                char c = name.charAt(3);
                buf.append(Character.toLowerCase(c));
                if (name.length() > 4)
                    buf.append(name.substring(4));
                String propertyName = buf.toString();
                buf.setCharAt(0, c);
                buf.insert(0, "set");
                String setterName = buf.toString();
                Method[] setter = FunctionObject.findMethods(
                                    clazz,
                                    hasPrefix ? genericPrefix + setterName
                                              : setterName);
                if (setter != null && setter.length != 1) {
                    Object[] errArgs = { name, clazz.getName() };
                    throw new PropertyException(
                        Context.getMessage("msg.no.overload", errArgs));
                }
                if (setter == null && hasPrefix)
                    setter = FunctionObject.findMethods(
                                clazz,
                                propertyPrefix + setterName);
                int attr = ScriptableObject.PERMANENT |
                           ScriptableObject.DONTENUM  |
                           (setter != null ? 0
                                           : ScriptableObject.READONLY);
                Method m = setter == null ? null : setter[0];
                ((ScriptableObject) proto).defineProperty(propertyName, null,
                                                          methods[i], m,
                                                          attr);
                continue;
            }
            FunctionObject f = new FunctionObject(name, methods[i], proto);
            Scriptable dest = prefix == staticFunctionPrefix
                              ? ctor
                              : proto;
            if (dest instanceof ScriptableObject) {
                ((ScriptableObject) dest).defineProperty(name, f, DONTENUM);
            } else {
                dest.put(name, dest, f);
            }
            if (sealed) {
                f.sealObject();
                f.addPropertyAttribute(READONLY);
            }
        }

        if (finishInit != null) {
            // call user code to complete the initialization
            Object[] finishArgs = { scope, ctor, proto };
            finishInit.invoke(null, finishArgs);
        }
        
        if (sealed) {
            ctor.sealObject();
            ctor.addPropertyAttribute(READONLY);
            if (proto instanceof ScriptableObject) {
                ((ScriptableObject) proto).sealObject();
                ((ScriptableObject) proto).addPropertyAttribute(READONLY);
            }
        }
    }

    /**
     * Define a JavaScript property.
     *
     * Creates the property with an initial value and sets its attributes.
     *
     * @param propertyName the name of the property to define.
     * @param value the initial value of the property
     * @param attributes the attributes of the JavaScript property
     * @see org.mozilla.javascript.Scriptable#put
     */
    public void defineProperty(String propertyName, Object value,
                               int attributes)
    {
        put(propertyName, this, value);
        try {
            setAttributes(propertyName, this, attributes);
        }
        catch (PropertyException e) {
            throw new RuntimeException("Cannot create property");
        }
    }

    /**
     * Define a JavaScript property with getter and setter side effects.
     *
     * If the setter is not found, the attribute READONLY is added to
     * the given attributes. <p>
     *
     * The getter must be a method with zero parameters, and the setter, if
     * found, must be a method with one parameter.<p>
     *
     * @param propertyName the name of the property to define. This name
     *                    also affects the name of the setter and getter
     *                    to search for. If the propertyId is "foo", then
     *                    <code>clazz</code> will be searched for "getFoo"
     *                    and "setFoo" methods.
     * @param clazz the Java class to search for the getter and setter
     * @param attributes the attributes of the JavaScript property
     * @exception PropertyException if multiple methods
     *            are found for the getter or setter, or if the getter
     *            or setter do not conform to the forms described in
     *            <code>defineProperty(String, Object, Method, Method,
     *            int)</code>
     * @see org.mozilla.javascript.Scriptable#put
     */
    public void defineProperty(String propertyName, Class clazz,
                               int attributes)
        throws PropertyException
    {
        StringBuffer buf = new StringBuffer(propertyName);
        buf.setCharAt(0, Character.toUpperCase(propertyName.charAt(0)));
        String s = buf.toString();
        Method[] getter = FunctionObject.findMethods(clazz, "get" + s);
        Method[] setter = FunctionObject.findMethods(clazz, "set" + s);
        if (setter == null)
            attributes |= ScriptableObject.READONLY;
        if (getter.length != 1 || (setter != null && setter.length != 1)) {
            Object[] errArgs = { propertyName, clazz.getName() };
            throw new PropertyException(
                Context.getMessage("msg.no.overload", errArgs));
        }
        defineProperty(propertyName, null, getter[0],
                       setter == null ? null : setter[0], attributes);
    }

    /**
     * Define a JavaScript property.
     *
     * Use this method only if you wish to define getters and setters for
     * a given property in a ScriptableObject. To create a property without
     * special getter or setter side effects, use
     * <code>defineProperty(String,int)</code>.
     *
     * If <code>setter</code> is null, the attribute READONLY is added to
     * the given attributes.<p>
     *
     * Several forms of getters or setters are allowed. The first are
     * nonstatic methods of the class referred to by 'this':
     * <pre>
     * Object getFoo();
     * void setFoo(Object value);</pre>
     * Next are static methods that may be of any class; the object whose
     * property is being accessed is passed in as an extra argument:
     * <pre>
     * static Object getFoo(ScriptableObject obj);
     * static void setFoo(ScriptableObject obj, Object value);</pre>
     * Finally, it is possible to delegate to another object entirely using
     * the <code>delegateTo</code> parameter. In this case the methods are
     * nonstatic methods of the class delegated to, and the object whose
     * property is being accessed is passed in as an extra argument:
     * <pre>
     * Object getFoo(ScriptableObject obj);
     * void setFoo(ScriptableObject obj, Object value);</pre>
     *
     * @param propertyName the name of the property to define.
     * @param delegateTo an object to call the getter and setter methods on,
     *                   or null, depending on the form used above.
     * @param getter the method to invoke to get the value of the property
     * @param setter the method to invoke to set the value of the property
     * @param attributes the attributes of the JavaScript property
     * @exception PropertyException if the getter or setter
     *            do not conform to the forms specified above
     */
    public void defineProperty(String propertyName, Object delegateTo,
                               Method getter, Method setter, int attributes)
        throws PropertyException
    {
        short flags = Slot.HAS_GETTER;
        if (delegateTo == null && (Modifier.isStatic(getter.getModifiers())))
            delegateTo = HAS_STATIC_ACCESSORS;
        Class[] parmTypes = getter.getParameterTypes();
        if (parmTypes.length != 0) {
            if (parmTypes.length != 1 ||
                parmTypes[0] != ScriptableObject.class)
            {
                Object[] args = { getter.toString() };
                throw new PropertyException(
                    Context.getMessage("msg.bad.getter.parms", args));
            }
        } else if (delegateTo != null) {
            Object[] args = { getter.toString() };
            throw new PropertyException(
                Context.getMessage("msg.obj.getter.parms", args));
        }
        if (setter != null) {
            flags |= Slot.HAS_SETTER;
            if ((delegateTo == HAS_STATIC_ACCESSORS) !=
                (Modifier.isStatic(setter.getModifiers())))
            {
                throw new PropertyException(
                    Context.getMessage("msg.getter.static", null));
            }
            parmTypes = setter.getParameterTypes();
            if (parmTypes.length == 2) {
                if (parmTypes[0] != ScriptableObject.class) {
                    throw new PropertyException(
                        Context.getMessage("msg.setter2.parms", null));
                }
                if (delegateTo == null) {
                    Object[] args = { setter.toString() };
                    throw new PropertyException(
                        Context.getMessage("msg.setter1.parms", args));
                }
            } else if (parmTypes.length == 1) {
                if (delegateTo != null) {
                    Object[] args = { setter.toString() };
                    throw new PropertyException(
                        Context.getMessage("msg.setter2.expected", args));
                }
            } else {
                throw new PropertyException(
                    Context.getMessage("msg.setter.parms", null));
            }
        }
        int slotIndex = getSlotToSet(propertyName,
                                     propertyName.hashCode(),
                                     true);
        GetterSlot slot = (GetterSlot) slots[slotIndex];
        slot.delegateTo = delegateTo;
        slot.getter = getter;
        slot.setter = setter;
        slot.value = null;
        slot.attributes = (short) attributes;
        slot.flags = flags;
    }

    /**
     * Search for names in a class, adding the resulting methods
     * as properties.
     *
     * <p> Uses reflection to find the methods of the given names. Then
     * FunctionObjects are constructed from the methods found, and
     * are added to this object as properties with the given names.
     *
     * @param names the names of the Methods to add as function properties
     * @param clazz the class to search for the Methods
     * @param attributes the attributes of the new properties
     * @exception PropertyException if any of the names
     *            has no corresponding method or more than one corresponding
     *            method in the class
     * @see org.mozilla.javascript.FunctionObject
     */
    public void defineFunctionProperties(String[] names, Class clazz,
                                         int attributes)
        throws PropertyException
    {
        for (int i=0; i < names.length; i++) {
            String name = names[i];
            Method[] m = FunctionObject.findMethods(clazz, name);
            if (m == null) {
                Object[] errArgs = { name, clazz.getName() };
                throw new PropertyException(
                    Context.getMessage("msg.method.not.found", errArgs));
            }
            if (m.length > 1) {
                Object[] errArgs = { name, clazz.getName() };
                throw new PropertyException(
                    Context.getMessage("msg.no.overload", errArgs));
            }
            FunctionObject f = new FunctionObject(name, m[0], this);
            defineProperty(name, f, attributes);
        }
    }

    /**
     * Get the Object.prototype property.
     * See ECMA 15.2.4.
     */
    public static Scriptable getObjectPrototype(Scriptable scope) {
        return getClassPrototype(scope, "Object");
    }

    /**
     * Get the Function.prototype property.
     * See ECMA 15.3.4.
     */
    public static Scriptable getFunctionPrototype(Scriptable scope) {
        return getClassPrototype(scope, "Function");
    }

    /**
     * Get the prototype for the named class.
     *
     * For example, <code>getClassPrototype(s, "Date")</code> will first
     * walk up the parent chain to find the outermost scope, then will
     * search that scope for the Date constructor, and then will
     * return Date.prototype. If any of the lookups fail, or
     * the prototype is not a JavaScript object, then null will
     * be returned.
     *
     * @param scope an object in the scope chain
     * @param className the name of the constructor
     * @return the prototype for the named class, or null if it
     *         cannot be found.
     */
    public static Scriptable getClassPrototype(Scriptable scope,
                                               String className)
    {
        scope = getTopLevelScope(scope);
        Object ctor = ScriptRuntime.getTopLevelProp(scope, className);
        if (ctor == NOT_FOUND || !(ctor instanceof Scriptable))
            return null;
        Scriptable ctorObj = (Scriptable) ctor;
        if (!ctorObj.has("prototype", ctorObj))
            return null;
        Object proto = ctorObj.get("prototype", ctorObj);
        if (!(proto instanceof Scriptable))
            return null;
        return (Scriptable) proto;
    }

    /**
     * Get the global scope.
     *
     * <p>Walks the parent scope chain to find an object with a null
     * parent scope (the global object).
     *
     * @param obj a JavaScript object
     * @return the corresponding global scope
     */
    public static Scriptable getTopLevelScope(Scriptable obj) {
        Scriptable next = obj;
        do {
            obj = next;
            next = obj.getParentScope();
        } while (next != null);
        return obj;
    }
    
    /**
     * Seal this object.
     * 
     * A sealed object may not have properties added or removed. Once
     * an object is sealed it may not be unsealed.
     * 
     * @since 1.4R3
     */
    public void sealObject() {
        count = -1;
    }
    
    /**
     * Return true if this object is sealed.
     * 
     * It is an error to attempt to add or remove properties to 
     * a sealed object.
     * 
     * @return true if sealed, false otherwise.
     * @since 1.4R3
     */
    public boolean isSealed() {
        return count == -1;
    }
        
    /**
     * Adds a property attribute to all properties.
     */
    synchronized void addPropertyAttribute(int attribute) {
        if (slots == null)
            return;
        for (int i=0; i < slots.length; i++) {
            if (slots[i] == null || slots[i] == REMOVED)
                continue;
            slots[i].attributes |= attribute;
        }
    }
    
    private static final int SLOT_NOT_FOUND = -1;

    private int getSlot(String id, int index) {
        if (slots == null)
            return SLOT_NOT_FOUND;
        int start = (index & 0x7fffffff) % slots.length;
        int i = start;
        do {
            Slot slot = slots[i];
            if (slot == null)
                return SLOT_NOT_FOUND;
            if (slot != REMOVED && slot.intKey == index && 
                (slot.stringKey == id || (id != null && 
                                          id.equals(slot.stringKey))))
            {
                return i;
            }
            if (++i == slots.length)
                i = 0;
        } while (i != start);
        return SLOT_NOT_FOUND;
    }

    private int getSlotToSet(String id, int index, boolean getterSlot) {
        if (slots == null)
            slots = new Slot[5];
        int start = (index & 0x7fffffff) % slots.length;
        int i = start;
        do {
            Slot slot = slots[i];
            if (slot == null) {
                return addSlot(id, index, getterSlot);
            }
            if (slot != REMOVED && slot.intKey == index && 
                (slot.stringKey == id || (id != null && 
                                          id.equals(slot.stringKey))))
            {
                return i;
            }
            if (++i == slots.length)
                i = 0;
        } while (i != start);
        throw new RuntimeException("Hashtable internal error");
    }

    /**
     * Add a new slot to the hash table.
     *
     * This method must be synchronized since it is altering the hash
     * table itself. Note that we search again for the slot to set
     * since another thread could have added the given property or
     * caused the table to grow while this thread was searching.
     */
    private synchronized int addSlot(String id, int index, boolean getterSlot)
    {
        if (count == -1)
            throw Context.reportRuntimeError(Context.getMessage
                                             ("msg.add.sealed", null));
        int start = (index & 0x7fffffff) % slots.length;
        int i = start;
        do {
            Slot slot = slots[i];
            if (slot == null || slot == REMOVED) {
                count++;
                if ((4 * count) > (3 * slots.length)) {
                    grow();
                    return getSlotToSet(id, index, getterSlot);
                }
                slot = getterSlot ? new GetterSlot() : new Slot();
                slot.stringKey = id;
                slot.intKey = index;
                slots[i] = slot;
                return i;
            }
            if (slot.intKey == index && 
                (slot.stringKey == id || (id != null && 
                                          id.equals(slot.stringKey)))) 
            {
                return i;
            }
            if (++i == slots.length)
                i = 0;
        } while (i != start);
        throw new RuntimeException("Hashtable internal error");
    }

    /**
     * Remove a slot from the hash table.
     *
     * This method must be synchronized since it is altering the hash
     * table itself. We might be able to optimize this more, but
     * deletes are not common.
     */
    private synchronized void removeSlot(String name, int index) {
        if (count == -1)
            throw Context.reportRuntimeError(Context.getMessage
                                             ("msg.remove.sealed", null));
        int slotIndex = getSlot(name, index);
        if (slotIndex == SLOT_NOT_FOUND)
            return;
        if ((slots[slotIndex].attributes & ScriptableObject.PERMANENT) != 0)
            return;
        slots[slotIndex] = REMOVED;
        count--;
    }

    /**
     * Grow the hash table to accommodate new entries.
     *
     * Note that by assigning the new array back at the end we
     * can continue reading the array from other threads.
     */
    private synchronized void grow() {
        Slot[] newSlots = new Slot[slots.length*2 + 1];
        for (int j=slots.length-1; j >= 0 ; j--) {
            Slot slot = slots[j];
            if (slot == null || slot == REMOVED)
                continue;
            int k = (slot.intKey & 0x7fffffff) % newSlots.length;
            while (newSlots[k] != null)
                if (++k == newSlots.length)
                    k = 0;
            // The end of the "synchronized" statement will cause the memory
            // writes to be propagated on a multiprocessor machine. We want
            // to make sure that the new table is prepared to be read.
            // XXX causes the 'this' pointer to be null in calling stack frames
            // on the MS JVM
            //synchronized (slot) { }
            newSlots[k] = slot;
        }
        slots = newSlots;
    }

    private Function getFunctionProperty(FlattenedObject f, String name) {
        Object val = f.getProperty(name);
        if (val == null || !(val instanceof FlattenedObject))
            return null;
        Scriptable s = ((FlattenedObject) val).getObject();
        if (s instanceof Function)
            return (Function) s;
        return null;
    }

    private static Hashtable getExclusionList() {
        if (exclusionList != null)
            return exclusionList;
        Hashtable result = new Hashtable(17);
        Method[] methods = ScriptRuntime.FunctionClass.getMethods();
        for (int i=0; i < methods.length; i++) {
            result.put(methods[i].getName(), Boolean.TRUE);
        }
        exclusionList = result;
        return result;
    }
        
    private Object[] getIds(boolean getAll) {
        if (slots == null)
            return ScriptRuntime.emptyArgs;
        int count = 0;
        for (int i=slots.length-1; i >= 0; i--) {
            Slot slot = slots[i];
            if (slot == null || slot == REMOVED)
                continue;
            if (getAll || (slot.attributes & DONTENUM) == 0)
                count++;
        }
        Object[] result = new Object[count];
        for (int i=slots.length-1; i >= 0; i--) {
            Slot slot = slots[i];
            if (slot == null || slot == REMOVED)
                continue;
            if (getAll || (slot.attributes & DONTENUM) == 0)
                result[--count] = slot.stringKey != null
                                  ? (Object) slot.stringKey
                                  : new Integer(slot.intKey);
        }
        return result;
    }

    
    /**
     * The prototype of this object.
     */
    protected Scriptable prototype;
    
    /**
     * The parent scope of this object.
     */
    protected Scriptable parent;

    private static final Object HAS_STATIC_ACCESSORS = Void.TYPE;
    private static final Slot REMOVED = new Slot();
    private static Hashtable exclusionList = null;
    
    private Slot[] slots;
    private int count;

    // cache; may be removed for smaller memory footprint
    private String lastName;
    private int lastHash;
    private Object lastValue = REMOVED;
}


class Slot implements Cloneable {
    static final int HAS_GETTER  = 0x01;
    static final int HAS_SETTER  = 0x02;
    
    public Object clone() {
        try {
            return super.clone();
        } catch (CloneNotSupportedException e) {
            // We know we've implemented Cloneable, so we'll never get here
            return null;
        }
    }
    
    int intKey;
    String stringKey;
    Object value;
    short attributes;
    short flags;
}

class GetterSlot extends Slot {
    Object delegateTo;  // OPT: merge with "value"
    Method getter;
    Method setter;
}


