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
 * Igor Bukanov
 * Roger Lawrence
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
import java.util.Hashtable;
import java.io.*;
import org.mozilla.javascript.debug.DebuggableObject;

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

public abstract class ScriptableObject implements Scriptable, Serializable,
                                                  DebuggableObject
{

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

    static void checkValidAttributes(int attributes)
    {
        final int mask = READONLY | DONTENUM | PERMANENT;
        if ((attributes & ~mask) != 0) {
            throw new IllegalArgumentException(String.valueOf(attributes));
        }
    }

    public ScriptableObject()
    {
    }

    public ScriptableObject(Scriptable scope, Scriptable prototype)
    {
        if (scope == null)
            throw new IllegalArgumentException();

        parentScopeObject = scope;
        prototypeObject = prototype;
    }

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
    public boolean has(String name, Scriptable start)
    {
        return null != getNamedSlot(name);
    }

    /**
     * Returns true if the property index is defined.
     *
     * @param index the numeric index for the property
     * @param start the object in which the lookup began
     * @return true if and only if the property was found in the object
     */
    public boolean has(int index, Scriptable start)
    {
        return null != getSlot(null, index);
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
    public Object get(String name, Scriptable start)
    {
        Slot slot = getNamedSlot(name);
        if (slot == null) {
            return Scriptable.NOT_FOUND;
        }
        if (slot.complexSlotFlag != 0) {
            GetterSlot gslot = (GetterSlot)slot;
            if (gslot.getter != null) {
                return getByGetter(gslot, start);
            }
        }

        return slot.value;
    }

    /**
     * Returns the value of the indexed property or NOT_FOUND.
     *
     * @param index the numeric index for the property
     * @param start the object in which the lookup began
     * @return the value of the property (may be null), or NOT_FOUND
     */
    public Object get(int index, Scriptable start)
    {
        Slot slot = getSlot(null, index);
        if (slot == null) {
            return Scriptable.NOT_FOUND;
        }
        return slot.value;
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
    public void put(String name, Scriptable start, Object value)
    {
        Slot slot = lastAccess; // Get local copy
        if (name != slot.stringKey || slot.wasDeleted != 0) {
            int hash = name.hashCode();
            slot = getSlot(name, hash);
            if (slot == null) {
                if (start != this) {
                    start.put(name, start, value);
                    return;
                }
                slot = addSlot(name, hash, null);
            }
            // Note: cache is not updated in put
        }
        if (start == this && isSealed()) {
            throw Context.reportRuntimeError1("msg.modify.sealed", name);
        }
        if ((slot.attributes & ScriptableObject.READONLY) != 0) {
            return;
        }
        if (slot.complexSlotFlag != 0) {
            GetterSlot gslot = (GetterSlot)slot;
            if (gslot.setter != null) {
                setBySetter(gslot, start, value);
            }
            return;
        }
        if (this == start) {
            slot.value = value;
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
    public void put(int index, Scriptable start, Object value)
    {
        Slot slot = getSlot(null, index);
        if (slot == null) {
            if (start != this) {
                start.put(index, start, value);
                return;
            }
            slot = addSlot(null, index, null);
        }
        if (start == this && isSealed()) {
            throw Context.reportRuntimeError1("msg.modify.sealed",
                                              Integer.toString(index));
        }
        if ((slot.attributes & ScriptableObject.READONLY) != 0) {
            return;
        }
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
     * @deprecated Use {@link #getAttributes(String name)}. The engine always
     * ignored the start argument.
     */
    public final int getAttributes(String name, Scriptable start)
    {
        return getAttributes(name);
    }

    /**
     * @deprecated Use {@link #getAttributes(int index)}. The engine always
     * ignored the start argument.
     */
    public final int getAttributes(int index, Scriptable start)
    {
        return getAttributes(index);
    }

    /**
     * @deprecated Use {@link #setAttributes(String name, int attributes)}.
     * The engine always ignored the start argument.
     */
    public final void setAttributes(String name, Scriptable start,
                                    int attributes)
    {
        setAttributes(name, attributes);
    }

    /**
     * @deprecated Use {@link #setAttributes(int index, int attributes)}.
     * The engine always ignored the start argument.
     */
    public void setAttributes(int index, Scriptable start,
                              int attributes)
    {
        setAttributes(index, attributes);
    }

    /**
     * Get the attributes of a named property.
     *
     * The property is specified by <code>name</code>
     * as defined for <code>has</code>.<p>
     *
     * @param name the identifier for the property
     * @return the bitset of attributes
     * @exception EvaluatorException if the named property is not found
     * @see org.mozilla.javascript.ScriptableObject#has
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#DONTENUM
     * @see org.mozilla.javascript.ScriptableObject#PERMANENT
     * @see org.mozilla.javascript.ScriptableObject#EMPTY
     */
    public int getAttributes(String name)
    {
        Slot slot = getNamedSlot(name);
        if (slot == null) {
            throw Context.reportRuntimeError1("msg.prop.not.found", name);
        }
        return slot.attributes;
    }

    /**
     * Get the attributes of an indexed property.
     *
     * @param index the numeric index for the property
     * @exception EvaluatorException if the named property is not found
     *            is not found
     * @return the bitset of attributes
     * @see org.mozilla.javascript.ScriptableObject#has
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#DONTENUM
     * @see org.mozilla.javascript.ScriptableObject#PERMANENT
     * @see org.mozilla.javascript.ScriptableObject#EMPTY
     */
    public int getAttributes(int index)
    {
        Slot slot = getSlot(null, index);
        if (slot == null) {
            throw Context.reportRuntimeError1("msg.prop.not.found",
                                              String.valueOf(index));
        }
        return slot.attributes;
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
     * @param attributes the bitset of attributes
     * @exception EvaluatorException if the named property is not found
     * @see org.mozilla.javascript.Scriptable#has
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#DONTENUM
     * @see org.mozilla.javascript.ScriptableObject#PERMANENT
     * @see org.mozilla.javascript.ScriptableObject#EMPTY
     */
    public void setAttributes(String name, int attributes)
    {
        checkValidAttributes(attributes);
        Slot slot = getNamedSlot(name);
        if (slot == null) {
            throw Context.reportRuntimeError1("msg.prop.not.found", name);
        }
        slot.attributes = (short) attributes;
    }

    /**
     * Set the attributes of an indexed property.
     *
     * @param index the numeric index for the property
     * @param attributes the bitset of attributes
     * @exception EvaluatorException if the named property is not found
     * @see org.mozilla.javascript.Scriptable#has
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#DONTENUM
     * @see org.mozilla.javascript.ScriptableObject#PERMANENT
     * @see org.mozilla.javascript.ScriptableObject#EMPTY
     */
    public void setAttributes(int index, int attributes)
    {
        checkValidAttributes(attributes);
        Slot slot = getSlot(null, index);
        if (slot == null) {
            throw Context.reportRuntimeError1("msg.prop.not.found",
                                              String.valueOf(index));
        }
        slot.attributes = (short) attributes;
    }

    /**
     * Returns the prototype of the object.
     */
    public Scriptable getPrototype()
    {
        return prototypeObject;
    }

    /**
     * Sets the prototype of the object.
     */
    public void setPrototype(Scriptable m)
    {
        prototypeObject = m;
    }

    /**
     * Returns the parent (enclosing) scope of the object.
     */
    public Scriptable getParentScope()
    {
        return parentScopeObject;
    }

    /**
     * Sets the parent (enclosing) scope of the object.
     */
    public void setParentScope(Scriptable m)
    {
        parentScopeObject = m;
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
    public Object getDefaultValue(Class typeHint)
    {
        Context cx = null;
        for (int i=0; i < 2; i++) {
            boolean tryToString;
            if (typeHint == ScriptRuntime.StringClass) {
                tryToString = (i == 0);
            } else {
                tryToString = (i == 1);
            }

            String methodName;
            Object[] args;
            if (tryToString) {
                methodName = "toString";
                args = ScriptRuntime.emptyArgs;
            } else {
                methodName = "valueOf";
                args = new Object[1];
                String hint;
                if (typeHint == null) {
                    hint = "undefined";
                } else if (typeHint == ScriptRuntime.StringClass) {
                    hint = "string";
                } else if (typeHint == ScriptRuntime.ScriptableClass) {
                    hint = "object";
                } else if (typeHint == ScriptRuntime.FunctionClass) {
                    hint = "function";
                } else if (typeHint == ScriptRuntime.BooleanClass
                           || typeHint == Boolean.TYPE)
                {
                    hint = "boolean";
                } else if (typeHint == ScriptRuntime.NumberClass ||
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
                {
                    hint = "number";
                } else {
                    throw Context.reportRuntimeError1(
                        "msg.invalid.type", typeHint.toString());
                }
                args[0] = hint;
            }
            Object v = getProperty(this, methodName);
            if (!(v instanceof Function))
                continue;
            Function fun = (Function) v;
            if (cx == null)
                cx = Context.getContext();
            v = fun.call(cx, fun.getParentScope(), this, args);
            if (v != null) {
                if (!(v instanceof Scriptable)) {
                    return v;
                }
                if (v == Undefined.instance
                    || typeHint == ScriptRuntime.ScriptableClass
                    || typeHint == ScriptRuntime.FunctionClass)
                {
                    return v;
                }
                if (tryToString && v instanceof Wrapper) {
                    // Let a wrapped java.lang.String pass for a primitive
                    // string.
                    Object u = ((Wrapper)v).unwrap();
                    if (u instanceof String)
                        return u;
                }
            }
        }
        // fall through to error
        String arg = (typeHint == null) ? "undefined" : typeHint.getName();
        throw ScriptRuntime.typeError1("msg.default.value", arg);
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
        // chasing.  This will be overridden in NativeFunction and non-JS
        // objects.

        return ScriptRuntime.jsDelegatesTo(instance, this);
    }

    /**
     * Custom <tt>==</tt> operator.
     * Must return {@link Scriptable#NOT_FOUND} if this object does not
     * have custom equality operator for the given value,
     * <tt>Boolean.TRUE</tt> if this object is equivalent to <tt>value</tt>,
     * <tt>Boolean.FALSE</tt> if this object is not equivalent to
     * <tt>value</tt>.
     * <p>
     * The default implementation returns Boolean.TRUE
     * if <tt>this == value</tt> or {@link Scriptable#NOT_FOUND} otherwise.
     * It indicates that by default custom equality is available only if
     * <tt>value</tt> is <tt>this</tt> in which case true is returned.
     */
    protected Object equivalentValues(Object value)
    {
        return (this == value) ? Boolean.TRUE : Scriptable.NOT_FOUND;
    }

    /**
     * Defines JavaScript objects from a Java class that implements Scriptable.
     *
     * If the given class has a method
     * <pre>
     * static void init(Context cx, Scriptable scope, boolean sealed);</pre>
     *
     * or its compatibility form
     * <pre>
     * static void init(Scriptable scope);</pre>
     *
     * then it is invoked and no further initialization is done.<p>
     *
     * However, if no such a method is found, then the class's constructors and
     * methods are used to initialize a class in the following manner.<p>
     *
     * First, the zero-parameter constructor of the class is called to
     * create the prototype. If no such constructor exists,
     * a {@link EvaluatorException} is thrown. <p>
     *
     * Next, all methods are scanned for special prefixes that indicate that they
     * have special meaning for defining JavaScript objects.
     * These special prefixes are
     * <ul>
     * <li><code>jsFunction_</code> for a JavaScript function
     * <li><code>jsStaticFunction_</code> for a JavaScript function that
     *           is a property of the constructor
     * <li><code>jsGet_</code> for a getter of a JavaScript property
     * <li><code>jsSet_</code> for a setter of a JavaScript property
     * <li><code>jsConstructor</code> for a JavaScript function that
     *           is the constructor
     * </ul><p>
     *
     * If the method's name begins with "jsFunction_", a JavaScript function
     * is created with a name formed from the rest of the Java method name
     * following "jsFunction_". So a Java method named "jsFunction_foo" will
     * define a JavaScript method "foo". Calling this JavaScript function
     * will cause the Java method to be called. The parameters of the method
     * must be of number and types as defined by the FunctionObject class.
     * The JavaScript function is then added as a property
     * of the prototype. <p>
     *
     * If the method's name begins with "jsStaticFunction_", it is handled
     * similarly except that the resulting JavaScript function is added as a
     * property of the constructor object. The Java method must be static.
     *
     * If the method's name begins with "jsGet_" or "jsSet_", the method is
     * considered to define a property. Accesses to the defined property
     * will result in calls to these getter and setter methods. If no
     * setter is defined, the property is defined as READONLY.<p>
     *
     * If the method's name is "jsConstructor", the method is
     * considered to define the body of the constructor. Only one
     * method of this name may be defined.
     * If no method is found that can serve as constructor, a Java
     * constructor will be selected to serve as the JavaScript
     * constructor in the following manner. If the class has only one
     * Java constructor, that constructor is used to define
     * the JavaScript constructor. If the the class has two constructors,
     * one must be the zero-argument constructor (otherwise an
     * {@link EvaluatorException} would have already been thrown
     * when the prototype was to be created). In this case
     * the Java constructor with one or more parameters will be used
     * to define the JavaScript constructor. If the class has three
     * or more constructors, an {@link EvaluatorException}
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
     * @see org.mozilla.javascript.Function
     * @see org.mozilla.javascript.FunctionObject
     * @see org.mozilla.javascript.ScriptableObject#READONLY
     * @see org.mozilla.javascript.ScriptableObject#defineProperty
     */
    public static void defineClass(Scriptable scope, Class clazz)
        throws IllegalAccessException, InstantiationException,
               InvocationTargetException
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
     *              and properties. The class must implement Scriptable.
     * @param sealed whether or not to create sealed standard objects that
     *               cannot be modified.
     * @exception IllegalAccessException if access is not available
     *            to a reflected class member
     * @exception InstantiationException if unable to instantiate
     *            the named class
     * @exception InvocationTargetException if an exception is thrown
     *            during execution of methods of the named class
     * @since 1.4R3
     */
    public static void defineClass(Scriptable scope, Class clazz,
                                   boolean sealed)
        throws IllegalAccessException, InstantiationException,
               InvocationTargetException
    {
        Method[] methods = FunctionObject.getMethodList(clazz);
        for (int i=0; i < methods.length; i++) {
            Method method = methods[i];
            if (!method.getName().equals("init"))
                continue;
            Class[] parmTypes = method.getParameterTypes();
            if (parmTypes.length == 3 &&
                parmTypes[0] == ScriptRuntime.ContextClass &&
                parmTypes[1] == ScriptRuntime.ScriptableClass &&
                parmTypes[2] == Boolean.TYPE &&
                Modifier.isStatic(method.getModifiers()))
            {
                Object args[] = { Context.getContext(), scope,
                                  sealed ? Boolean.TRUE : Boolean.FALSE };
                method.invoke(null, args);
                return;
            }
            if (parmTypes.length == 1 &&
                parmTypes[0] == ScriptRuntime.ScriptableClass &&
                Modifier.isStatic(method.getModifiers()))
            {
                Object args[] = { scope };
                method.invoke(null, args);
                return;
            }

        }

        // If we got here, there isn't an "init" method with the right
        // parameter types.

        Constructor[] ctors = clazz.getConstructors();
        Constructor protoCtor = null;
        for (int i=0; i < ctors.length; i++) {
            if (ctors[i].getParameterTypes().length == 0) {
                protoCtor = ctors[i];
                break;
            }
        }
        if (protoCtor == null) {
            throw Context.reportRuntimeError1(
                      "msg.zero.arg.ctor", clazz.getName());
        }

        Scriptable proto = (Scriptable)
                        protoCtor.newInstance(ScriptRuntime.emptyArgs);
        proto.setPrototype(getObjectPrototype(scope));
        String className = proto.getClassName();

        // Find out whether there are any methods that begin with
        // "js". If so, then only methods that begin with special
        // prefixes will be defined as JavaScript entities.
        final String functionPrefix = "jsFunction_";
        final String staticFunctionPrefix = "jsStaticFunction_";
        final String getterPrefix = "jsGet_";
        final String setterPrefix = "jsSet_";
        final String ctorName = "jsConstructor";

        Member ctorMember = FunctionObject.findSingleMethod(methods, ctorName);

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
                throw Context.reportRuntimeError1(
                          "msg.ctor.multiple.parms", clazz.getName());
            }
        }

        FunctionObject ctor = new FunctionObject(className, ctorMember, scope);
        if (ctor.isVarArgsMethod()) {
            throw Context.reportRuntimeError1
                ("msg.varargs.ctor", ctorMember.getName());
        }
        ctor.addAsConstructor(scope, proto);

        Method finishInit = null;
        for (int i=0; i < methods.length; i++) {
            if (methods[i] == ctorMember) {
                continue;
            }
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
            // ignore any compiler generated methods.
            if (name.indexOf('$') != -1)
                continue;
            if (name.equals(ctorName))
                continue;

            String prefix = null;
            if (name.startsWith(functionPrefix)) {
                prefix = functionPrefix;
            } else if (name.startsWith(staticFunctionPrefix)) {
                prefix = staticFunctionPrefix;
                if (!Modifier.isStatic(methods[i].getModifiers())) {
                    throw Context.reportRuntimeError(
                        "jsStaticFunction must be used with static method.");
                }
            } else if (name.startsWith(getterPrefix)) {
                prefix = getterPrefix;
            } else if (name.startsWith(setterPrefix)) {
                prefix = setterPrefix;
            } else {
                continue;
            }
            name = name.substring(prefix.length());
            if (prefix == setterPrefix)
                continue;   // deal with set when we see get
            if (prefix == getterPrefix) {
                if (!(proto instanceof ScriptableObject)) {
                    throw Context.reportRuntimeError2(
                        "msg.extend.scriptable",
                        proto.getClass().toString(), name);
                }
                Method setter = FunctionObject.findSingleMethod(
                                    methods,
                                    setterPrefix + name);
                int attr = ScriptableObject.PERMANENT |
                           ScriptableObject.DONTENUM  |
                           (setter != null ? 0
                                           : ScriptableObject.READONLY);
                ((ScriptableObject) proto).defineProperty(name, null,
                                                          methods[i], setter,
                                                          attr);
                continue;
            }

            FunctionObject f = new FunctionObject(name, methods[i], proto);
            if (f.isVarArgsConstructor()) {
                throw Context.reportRuntimeError1
                    ("msg.varargs.fun", ctorMember.getName());
            }
            Scriptable dest = prefix == staticFunctionPrefix
                              ? ctor
                              : proto;
            defineProperty(dest, name, f, DONTENUM);
            if (sealed) {
                f.sealObject();
            }
        }

        if (finishInit != null) {
            // call user code to complete the initialization
            Object[] finishArgs = { scope, ctor, proto };
            finishInit.invoke(null, finishArgs);
        }

        if (sealed) {
            ctor.sealObject();
            if (proto instanceof ScriptableObject) {
                ((ScriptableObject) proto).sealObject();
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
        setAttributes(propertyName, attributes);
    }

    /**
     * Utility method to add properties to arbitrary Scriptable object.
     * If destination is instance of ScriptableObject, calls
     * defineProperty there, otherwise calls put in destination
     * ignoring attributes
     */
    public static void defineProperty(Scriptable destination,
                                      String propertyName, Object value,
                                      int attributes)
    {
        if (!(destination instanceof ScriptableObject)) {
            destination.put(propertyName, destination, value);
            return;
        }
        ScriptableObject so = (ScriptableObject)destination;
        so.defineProperty(propertyName, value, attributes);
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
     * @see org.mozilla.javascript.Scriptable#put
     */
    public void defineProperty(String propertyName, Class clazz,
                               int attributes)
    {
        int length = propertyName.length();
        if (length == 0) throw new IllegalArgumentException();
        char[] buf = new char[3 + length];
        propertyName.getChars(0, length, buf, 3);
        buf[3] = Character.toUpperCase(buf[3]);
        buf[0] = 'g';
        buf[1] = 'e';
        buf[2] = 't';
        String getterName = new String(buf);
        buf[0] = 's';
        String setterName = new String(buf);

        Method[] methods = FunctionObject.getMethodList(clazz);
        Method getter = FunctionObject.findSingleMethod(methods, getterName);
        Method setter = FunctionObject.findSingleMethod(methods, setterName);
        if (setter == null)
            attributes |= ScriptableObject.READONLY;
        defineProperty(propertyName, null, getter,
                       setter == null ? null : setter, attributes);
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
     * Several forms of getters or setters are allowed. In all cases the
     * type of the value parameter can be any one of the following types:
     * Object, String, boolean, Scriptable, byte, short, int, long, float,
     * or double. The runtime will perform appropriate conversions based
     * upon the type of the parameter (see description in FunctionObject).
     * The first forms are nonstatic methods of the class referred to
     * by 'this':
     * <pre>
     * Object getFoo();
     * void setFoo(SomeType value);</pre>
     * Next are static methods that may be of any class; the object whose
     * property is being accessed is passed in as an extra argument:
     * <pre>
     * static Object getFoo(ScriptableObject obj);
     * static void setFoo(ScriptableObject obj, SomeType value);</pre>
     * Finally, it is possible to delegate to another object entirely using
     * the <code>delegateTo</code> parameter. In this case the methods are
     * nonstatic methods of the class delegated to, and the object whose
     * property is being accessed is passed in as an extra argument:
     * <pre>
     * Object getFoo(ScriptableObject obj);
     * void setFoo(ScriptableObject obj, SomeType value);</pre>
     *
     * @param propertyName the name of the property to define.
     * @param delegateTo an object to call the getter and setter methods on,
     *                   or null, depending on the form used above.
     * @param getter the method to invoke to get the value of the property
     * @param setter the method to invoke to set the value of the property
     * @param attributes the attributes of the JavaScript property
     */
    public void defineProperty(String propertyName, Object delegateTo,
                               Method getter, Method setter, int attributes)
    {
        if (delegateTo == null && (Modifier.isStatic(getter.getModifiers())))
            delegateTo = HAS_STATIC_ACCESSORS;
        Class[] parmTypes = getter.getParameterTypes();
        if (parmTypes.length != 0) {
            if (parmTypes.length != 1 ||
                parmTypes[0] != ScriptRuntime.ScriptableObjectClass)
            {
                throw Context.reportRuntimeError1(
                    "msg.bad.getter.parms", getter.toString());
            }
        } else if (delegateTo != null) {
            throw Context.reportRuntimeError1(
                "msg.obj.getter.parms", getter.toString());
        }
        if (setter != null) {
            if ((delegateTo == HAS_STATIC_ACCESSORS) !=
                (Modifier.isStatic(setter.getModifiers())))
            {
                throw Context.reportRuntimeError0("msg.getter.static");
            }
            parmTypes = setter.getParameterTypes();
            if (parmTypes.length == 2) {
                if (parmTypes[0] != ScriptRuntime.ScriptableObjectClass) {
                    throw Context.reportRuntimeError0("msg.setter2.parms");
                }
                if (delegateTo == null) {
                    throw Context.reportRuntimeError1(
                        "msg.setter1.parms", setter.toString());
                }
            } else if (parmTypes.length == 1) {
                if (delegateTo != null) {
                    throw Context.reportRuntimeError1(
                        "msg.setter2.expected", setter.toString());
                }
            } else {
                throw Context.reportRuntimeError0("msg.setter.parms");
            }
            Class setterType = parmTypes[parmTypes.length - 1];
            int setterTypeTag = FunctionObject.getTypeTag(setterType);
            if (setterTypeTag  == FunctionObject.JAVA_UNSUPPORTED_TYPE) {
                throw Context.reportRuntimeError2(
                    "msg.setter2.expected", setterType.getName(),
                    setter.toString());
            }
        }

        ClassCache cache = ClassCache.get(this);
        GetterSlot gslot = new GetterSlot();
        gslot.delegateTo = delegateTo;
        gslot.getter = new MemberBox(getter, cache);
        gslot.getter.prepareInvokerOptimization();
        if (setter != null) {
            gslot.setter = new MemberBox(setter, cache);
            gslot.setter.prepareInvokerOptimization();
        }
        gslot.attributes = (short) attributes;
        gslot.complexSlotFlag = 1;
        Slot inserted = addSlot(propertyName, propertyName.hashCode(), gslot);
        if (inserted != gslot) {
            throw new RuntimeException("Property already exists");
        }

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
     * @see org.mozilla.javascript.FunctionObject
     */
    public void defineFunctionProperties(String[] names, Class clazz,
                                         int attributes)
    {
        Method[] methods = FunctionObject.getMethodList(clazz);
        for (int i=0; i < names.length; i++) {
            String name = names[i];
            Method m = FunctionObject.findSingleMethod(methods, name);
            if (m == null) {
                throw Context.reportRuntimeError2(
                    "msg.method.not.found", name, clazz.getName());
            }
            FunctionObject f = new FunctionObject(name, m, this);
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
        Object ctor = getProperty(scope, className);
        Object proto;
        if (ctor instanceof BaseFunction) {
            proto = ((BaseFunction)ctor).getPrototypeProperty();
        } else if (ctor instanceof Scriptable) {
            Scriptable ctorObj = (Scriptable)ctor;
            proto = ctorObj.get("prototype", ctorObj);
        } else {
            return null;
        }
        if (proto instanceof Scriptable) {
            return (Scriptable)proto;
        }
        return null;
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
    public static Scriptable getTopLevelScope(Scriptable obj)
    {
        for (;;) {
            Scriptable parent = obj.getParentScope();
            if (parent == null) {
                return obj;
            }
            obj = parent;
        }
    }

    /**
     * Seal this object.
     *
     * A sealed object may not have properties added or removed. Once
     * an object is sealed it may not be unsealed.
     *
     * @since 1.4R3
     */
    public synchronized void sealObject() {
        if (count >= 0) {
            count = -1 - count;
        }
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
    public final boolean isSealed() {
        return count < 0;
    }

    /**
     * Gets a named property from an object or any object in its prototype chain.
     * <p>
     * Searches the prototype chain for a property named <code>name</code>.
     * <p>
     * @param obj a JavaScript object
     * @param name a property name
     * @return the value of a property with name <code>name</code> found in
     *         <code>obj</code> or any object in its prototype chain, or
     *         <code>Scriptable.NOT_FOUND</code> if not found
     * @since 1.5R2
     */
    public static Object getProperty(Scriptable obj, String name)
    {
        Scriptable start = obj;
        Object result;
        do {
            result = obj.get(name, start);
            if (result != Scriptable.NOT_FOUND)
                break;
            obj = obj.getPrototype();
        } while (obj != null);
        return result;
    }

    /**
     * Gets an indexed property from an object or any object in its prototype chain.
     * <p>
     * Searches the prototype chain for a property with integral index
     * <code>index</code>. Note that if you wish to look for properties with numerical
     * but non-integral indicies, you should use getProperty(Scriptable,String) with
     * the string value of the index.
     * <p>
     * @param obj a JavaScript object
     * @param index an integral index
     * @return the value of a property with index <code>index</code> found in
     *         <code>obj</code> or any object in its prototype chain, or
     *         <code>Scriptable.NOT_FOUND</code> if not found
     * @since 1.5R2
     */
    public static Object getProperty(Scriptable obj, int index)
    {
        Scriptable start = obj;
        Object result;
        do {
            result = obj.get(index, start);
            if (result != Scriptable.NOT_FOUND)
                break;
            obj = obj.getPrototype();
        } while (obj != null);
        return result;
    }

    /**
     * Returns whether a named property is defined in an object or any object
     * in its prototype chain.
     * <p>
     * Searches the prototype chain for a property named <code>name</code>.
     * <p>
     * @param obj a JavaScript object
     * @param name a property name
     * @return the true if property was found
     * @since 1.5R2
     */
    public static boolean hasProperty(Scriptable obj, String name)
    {
        return null != getBase(obj, name);
    }

    /**
     * Returns whether an indexed property is defined in an object or any object
     * in its prototype chain.
     * <p>
     * Searches the prototype chain for a property with index <code>index</code>.
     * <p>
     * @param obj a JavaScript object
     * @param index a property index
     * @return the true if property was found
     * @since 1.5R2
     */
    public static boolean hasProperty(Scriptable obj, int index)
    {
        return null != getBase(obj, index);
    }

    /**
     * Puts a named property in an object or in an object in its prototype chain.
     * <p>
     * Seaches for the named property in the prototype chain. If it is found,
     * the value of the property is changed. If it is not found, a new
     * property is added in <code>obj</code>.
     * @param obj a JavaScript object
     * @param name a property name
     * @param value any JavaScript value accepted by Scriptable.put
     * @since 1.5R2
     */
    public static void putProperty(Scriptable obj, String name, Object value)
    {
        Scriptable base = getBase(obj, name);
        if (base == null)
            base = obj;
        base.put(name, obj, value);
    }

    /**
     * Puts an indexed property in an object or in an object in its prototype chain.
     * <p>
     * Seaches for the indexed property in the prototype chain. If it is found,
     * the value of the property is changed. If it is not found, a new
     * property is added in <code>obj</code>.
     * @param obj a JavaScript object
     * @param index a property index
     * @param value any JavaScript value accepted by Scriptable.put
     * @since 1.5R2
     */
    public static void putProperty(Scriptable obj, int index, Object value)
    {
        Scriptable base = getBase(obj, index);
        if (base == null)
            base = obj;
        base.put(index, obj, value);
    }

    /**
     * Removes the property from an object or its prototype chain.
     * <p>
     * Searches for a property with <code>name</code> in obj or
     * its prototype chain. If it is found, the object's delete
     * method is called.
     * @param obj a JavaScript object
     * @param name a property name
     * @return true if the property doesn't exist or was successfully removed
     * @since 1.5R2
     */
    public static boolean deleteProperty(Scriptable obj, String name)
    {
        Scriptable base = getBase(obj, name);
        if (base == null)
            return true;
        base.delete(name);
        return !base.has(name, obj);
    }

    /**
     * Removes the property from an object or its prototype chain.
     * <p>
     * Searches for a property with <code>index</code> in obj or
     * its prototype chain. If it is found, the object's delete
     * method is called.
     * @param obj a JavaScript object
     * @param index a property index
     * @return true if the property doesn't exist or was successfully removed
     * @since 1.5R2
     */
    public static boolean deleteProperty(Scriptable obj, int index)
    {
        Scriptable base = getBase(obj, index);
        if (base == null)
            return true;
        base.delete(index);
        return !base.has(index, obj);
    }

    /**
     * Returns an array of all ids from an object and its prototypes.
     * <p>
     * @param obj a JavaScript object
     * @return an array of all ids from all object in the prototype chain.
     *         If a given id occurs multiple times in the prototype chain,
     *         it will occur only once in this list.
     * @since 1.5R2
     */
    public static Object[] getPropertyIds(Scriptable obj)
    {
        if (obj == null) {
            return ScriptRuntime.emptyArgs;
        }
        Object[] result = obj.getIds();
        ObjToIntMap map = null;
        for (;;) {
            obj = obj.getPrototype();
            if (obj == null) {
                break;
            }
            Object[] ids = obj.getIds();
            if (ids.length == 0) {
                continue;
            }
            if (map == null) {
                if (result.length == 0) {
                    result = ids;
                    continue;
                }
                map = new ObjToIntMap(result.length + ids.length);
                for (int i = 0; i != result.length; ++i) {
                    map.intern(result[i]);
                }
                result = null; // Allow to GC the result
            }
            for (int i = 0; i != ids.length; ++i) {
                map.intern(ids[i]);
            }
        }
        if (map != null) {
            result = map.getKeys();
        }
        return result;
    }

    /**
     * Call a method of an object.
     * @param obj the JavaScript object
     * @param methodName the name of the function property
     * @param args the arguments for the call
     *
     * @see Context#getCurrentContext()
     */
    public static Object callMethod(Scriptable obj, String methodName,
                                    Object[] args)
    {
        return callMethod(null, obj, methodName, args);
    }

    /**
     * Call a method of an object.
     * @param cx the Context object associated with the current thread.
     * @param obj the JavaScript object
     * @param methodName the name of the function property
     * @param args the arguments for the call
     */
    public static Object callMethod(Context cx, Scriptable obj,
                                    String methodName,
                                    Object[] args)
    {
        Object funObj = getProperty(obj, methodName);
        if (!(funObj instanceof Function)) {
            throw ScriptRuntime.notFunctionError(obj, methodName);
        }
        Function fun = (Function)funObj;
        Scriptable scope = ScriptableObject.getTopLevelScope(fun);
        // XXX: The following is only necessary for dynamic scope setup,
        // but to check for that Context instance is required.
        // Since it should not harm non-dynamic scope setup, do it always
        // for now.
        Scriptable dynamicScope = ScriptableObject.getTopLevelScope(obj);
        scope = ScriptRuntime.checkDynamicScope(dynamicScope, scope);
        if (cx != null) {
            return fun.call(cx, scope, obj, args);
        } else {
            return Context.call(null, fun, scope, obj, args);
        }
    }

    private static Scriptable getBase(Scriptable obj, String name)
    {
        do {
            if (obj.has(name, obj))
                break;
            obj = obj.getPrototype();
        } while(obj != null);
        return obj;
    }

    private static Scriptable getBase(Scriptable obj, int index)
    {
        do {
            if (obj.has(index, obj))
                break;
            obj = obj.getPrototype();
        } while(obj != null);
        return obj;
    }

    /**
     * Get arbitrary application-specific value associated with this object.
     * @param key key object to select particular value.
     * @see #associateValue(Object key, Object value)
     */
    public final Object getAssociatedValue(Object key)
    {
        Hashtable h = associatedValues;
        if (h == null)
            return null;
        return h.get(key);
    }

    /**
     * Get arbitrary application-specific value associated with the top scope
     * of the given scope.
     * The method first calls {@link #getTopLevelScope(Scriptable scope)}
     * and then searches the prototype chain of the top scope for the first
     * object containing the associated value with the given key.
     *
     * @param scope the starting scope.
     * @param key key object to select particular value.
     * @see #getAssociatedValue(Object key)
     */
    public static Object getTopScopeValue(Scriptable scope, Object key)
    {
        scope = ScriptableObject.getTopLevelScope(scope);
        for (;;) {
            if (scope instanceof ScriptableObject) {
                ScriptableObject so = (ScriptableObject)scope;
                Object value = so.getAssociatedValue(key);
                if (value != null) {
                    return value;
                }
            }
            scope = scope.getPrototype();
            if (scope == null) {
                return null;
            }
        }
    }

    /**
     * Associate arbitrary application-specific value with this object.
     * Value can only be associated with the given object and key only once.
     * The method ignores any subsequent attempts to change the already
     * associated value.
     * <p> The associated values are not serilized.
     * @param key key object to select particular value.
     * @param value the value to associate
     * @return the passed value if the method is called first time for the
     * given key or old value for any subsequent calls.
     * @see #getAssociatedValue(Object key)
     */
    public final Object associateValue(Object key, Object value)
    {
        if (value == null) throw new IllegalArgumentException();
        Hashtable h = associatedValues;
        if (h == null) {
            synchronized (this) {
                h = associatedValues;
                if (h == null) {
                    h = new Hashtable();
                    associatedValues = h;
                }
            }
        }
        return Kit.initHash(h, key, value);
    }

    private Object getByGetter(GetterSlot slot, Scriptable start)
    {
        Object getterThis;
        Object[] args;
        if (slot.delegateTo == null) {
            if (start != this) {
                // Walk the prototype chain to find an appropriate
                // object to invoke the getter on.
                Class clazz = slot.getter.getDeclaringClass();
                while (!clazz.isInstance(start)) {
                    start = start.getPrototype();
                    if (start == this) {
                        break;
                    }
                    if (start == null) {
                        start = this;
                        break;
                    }
                }
            }
            getterThis = start;
            args = ScriptRuntime.emptyArgs;
        } else {
            getterThis = slot.delegateTo;
            args = new Object[] { this };
        }

        return slot.getter.invoke(getterThis, args);
    }

    private void setBySetter(GetterSlot slot, Scriptable start, Object value)
    {
        if (start != this) {
            if (slot.delegateTo != null
                || !slot.setter.getDeclaringClass().isInstance(start))
            {
                start.put(slot.stringKey, start, value);
                return;
            }
        }

        Object setterThis;
        Object[] args;
        Object setterResult;
        Context cx = Context.getContext();
        Class pTypes[] = slot.setter.argTypes;
        Class desired = pTypes[pTypes.length - 1];
        // ALERT: cache tag since it is already calculated in defineProperty ?
        int tag = FunctionObject.getTypeTag(desired);
        Object actualArg = FunctionObject.convertArg(cx, start, value, tag);
        if (slot.delegateTo == null) {
            setterThis = start;
            args = new Object[] { actualArg };
        } else {
            if (start != this) Kit.codeBug();
            setterThis = slot.delegateTo;
            args = new Object[] { this, actualArg };
        }
        // Check start is sealed: start is always instance of ScriptableObject
        // due to logic in if (start != this) above
        if (((ScriptableObject)start).isSealed()) {
            throw Context.reportRuntimeError1("msg.modify.sealed",
                                              slot.stringKey);
        }

        setterResult = slot.setter.invoke(setterThis, args);

        if (slot.setter.method().getReturnType() != Void.TYPE) {
            // Replace Getter slot by a simple one
            Slot replacement = new Slot();
            replacement.intKey = slot.intKey;
            replacement.stringKey = slot.stringKey;
            replacement.attributes = slot.attributes;
            replacement.value = setterResult;

            synchronized (this) {
                int i = getSlotPosition(slots, slot.stringKey, slot.intKey);
                // Check slot was not deleted/replaced before synchronization
                if (i >= 0 && slots[i] == slot) {
                    slots[i] = replacement;
                    // It is important to make sure that lastAccess != slot
                    // to prevent accessing the old slot via lastAccess and
                    // then invoking setter one more time
                    lastAccess = replacement;
                }
            }
        }
    }

    private Slot getNamedSlot(String name)
    {
        // Query last access cache and check that it was not deleted
        Slot slot = lastAccess;
        if (name == slot.stringKey && slot.wasDeleted == 0) {
            return slot;
        }
        int hash = name.hashCode();
        Slot[] slots = this.slots; // Get stable local reference
        int i = getSlotPosition(slots, name, hash);
        if (i < 0) {
            return null;
        }
        slot = slots[i];
        // Update cache - here stringKey.equals(name) holds, but it can be
        // that slot.stringKey != name. To make last name cache work, need
        // to change the key
        slot.stringKey = name;
        lastAccess = slot;
        return slot;
    }

    private Slot getSlot(String id, int index)
    {
        Slot[] slots = this.slots; // Get local copy
        int i = getSlotPosition(slots, id, index);
        return (i < 0) ? null : slots[i];
    }

    private static int getSlotPosition(Slot[] slots, String id, int index)
    {
        if (slots != null) {
            int start = (index & 0x7fffffff) % slots.length;
            int i = start;
            do {
                Slot slot = slots[i];
                if (slot == null)
                    break;
                if (slot != REMOVED && slot.intKey == index
                    && (slot.stringKey == id
                        || (id != null && id.equals(slot.stringKey))))
                {
                    return i;
                }
                if (++i == slots.length)
                    i = 0;
            } while (i != start);
        }
        return -1;
    }

    /**
     * Add a new slot to the hash table.
     *
     * This method must be synchronized since it is altering the hash
     * table itself. Note that we search again for the slot to set
     * since another thread could have added the given property or
     * caused the table to grow while this thread was searching.
     */
    private synchronized Slot addSlot(String id, int index, Slot newSlot) {
        if (isSealed()) {
            String str = (id != null) ? id : Integer.toString(index);
            throw Context.reportRuntimeError1("msg.add.sealed", str);
        }

        if (slots == null) { slots = new Slot[5]; }

        return addSlotImpl(id, index, newSlot);
    }

    // Must be inside synchronized (this)
    private Slot addSlotImpl(String id, int index, Slot newSlot)
    {
        int start = (index & 0x7fffffff) % slots.length;
        int i = start;
        for (;;) {
            Slot slot = slots[i];
            if (slot == null || slot == REMOVED) {
                if ((4 * (count + 1)) > (3 * slots.length)) {
                    grow();
                    return addSlotImpl(id, index, newSlot);
                }
                slot = (newSlot == null) ? new Slot() : newSlot;
                slot.stringKey = id;
                slot.intKey = index;
                slots[i] = slot;
                count++;
                return slot;
            }
            if (slot.intKey == index &&
                (slot.stringKey == id || (id != null &&
                                          id.equals(slot.stringKey))))
            {
                return slot;
            }
            if (++i == slots.length)
                i = 0;
            if (i == start) {
                // slots should never be full or bug in grow code
                throw new IllegalStateException();
            }
        }
    }

    /**
     * Remove a slot from the hash table.
     *
     * This method must be synchronized since it is altering the hash
     * table itself. We might be able to optimize this more, but
     * deletes are not common.
     */
    private synchronized void removeSlot(String name, int index) {
        if (isSealed()) {
            String str = (name != null) ? name : Integer.toString(index);
            throw Context.reportRuntimeError1("msg.remove.sealed", str);
        }

        int i = getSlotPosition(slots, name, index);
        if (i >= 0) {
            Slot slot = slots[i];
            if ((slot.attributes & PERMANENT) == 0) {
                // Mark the slot as removed to handle a case when
                // another thread manages to put just removed slot
                // into lastAccess cache.
                slot.wasDeleted = (byte)1;
                if (slot == lastAccess) {
                    lastAccess = REMOVED;
                }
                count--;
                if (count != 0) {
                    slots[i] = REMOVED;
                } else {
                    // With no slots it is OK to mark with null.
                    slots[i] = null;
                }
            }
        }
    }

     // Grow the hash table to accommodate new entries.
     //
     // Note that by assigning the new array back at the end we
     // can continue reading the array from other threads.
     // Must be inside synchronized (this)
    private void grow() {
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

    Object[] getIds(boolean getAll) {
        Slot[] s = slots;
        Object[] a = ScriptRuntime.emptyArgs;
        if (s == null)
            return a;
        int c = 0;
        for (int i=0; i < s.length; i++) {
            Slot slot = s[i];
            if (slot == null || slot == REMOVED)
                continue;
            if (getAll || (slot.attributes & DONTENUM) == 0) {
                if (c == 0)
                    a = new Object[s.length - i];
                a[c++] = slot.stringKey != null
                             ? (Object) slot.stringKey
                             : new Integer(slot.intKey);
            }
        }
        if (c == a.length)
            return a;
        Object[] result = new Object[c];
        System.arraycopy(a, 0, result, 0, c);
        return result;
    }

    private synchronized void writeObject(ObjectOutputStream out)
        throws IOException
    {
        out.defaultWriteObject();
        int N = count;
        if (N < 0) {
            N = -1 - count;
        }
        Slot[] s = slots;
        if (s == null) {
            if (N != 0) Kit.codeBug();
            out.writeInt(0);
        } else {
            out.writeInt(s.length);
            for (int i = 0; N != 0; ++i) {
                Slot slot = s[i];
                if (slot != null && slot != REMOVED) {
                    --N;
                    out.writeObject(slot);
                }
            }
        }
    }

    private void readObject(ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
        in.defaultReadObject();
        lastAccess = REMOVED;

        int capacity = in.readInt();
        if (capacity != 0) {
            slots = new Slot[capacity];
            int N = count;
            boolean wasSealed = false;
            if (N < 0) {
                N = -1 - N; wasSealed = true;
            }
            count = 0;
            for (int i = 0; i != N; ++i) {
                Slot s = (Slot)in.readObject();
                addSlotImpl(s.stringKey, s.intKey, s);
            }
            if (wasSealed) {
                count = - 1 - count;
            }
        }
    }

    /**
     * The prototype of this object.
     */
    private Scriptable prototypeObject;

    /**
     * The parent scope of this object.
     */
    private Scriptable parentScopeObject;

    private static final Object HAS_STATIC_ACCESSORS = Void.TYPE;
    private static final Slot REMOVED = new Slot();

    private transient Slot[] slots;
    // If count >= 0, it gives number of keys or if count < 0,
    // it indicates sealed object where -1 - count gives number of keys
    private int count;

    // cache; may be removed for smaller memory footprint
    private transient Slot lastAccess = REMOVED;

    // associated values are not serialized
    private transient volatile Hashtable associatedValues;

    private static class Slot implements Serializable {
        static final int HAS_GETTER  = 0x01;
        static final int HAS_SETTER  = 0x02;

        private void readObject(ObjectInputStream in)
            throws IOException, ClassNotFoundException
        {
            in.defaultReadObject();
            if (stringKey != null) {
                intKey = stringKey.hashCode();
            }
            complexSlotFlag = (this instanceof GetterSlot) ? (byte)1 : (byte)0;
        }

        int intKey;
        String stringKey;
        Object value;
        short attributes;
        transient byte complexSlotFlag;
        transient byte wasDeleted;
    }

    private static class GetterSlot extends Slot
    {
        Object delegateTo;
        MemberBox getter;
        MemberBox setter;
    }
}
