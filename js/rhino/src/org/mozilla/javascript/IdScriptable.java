/* -*- Mode: java; tab-width: 4; indent-tabs-mode: 1; c-basic-offset: 4 -*-
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
 * Igor Bukanov
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

/**
Base class for native object implementation that uses IdFunction to export its methods to script via <class-name>.prototype object.

Any descendant should implement at least the following methods:
    mapNameToId
    getIdName
    execMethod
    methodArity

To define non-function properties, the descendant should customize
    getIdValue
    setIdValue
    getIdDefaultAttributes
    maxInstanceId
to get/set property value and provide its default attributes.

To customize initializition of constructor and protype objects, descendant
may override scopeInit or fillConstructorProperties methods.

*/
public abstract class IdScriptable extends ScriptableObject
    implements IdFunctionMaster
{
    /** NULL_TAG can be used to distinguish between uninitialized and null
     ** values
     */
    protected static final Object NULL_TAG = new Object();

    public IdScriptable() {
        activateIdMap(maxInstanceId());
    }

    public boolean has(String name, Scriptable start) {
        if (maxId != 0) {
            int id = mapNameToId(name);
            if (id != 0) {
                return hasValue(id);
            }
        }
        return super.has(name, start);
    }

    public Object get(String name, Scriptable start) {
        if (CACHE_NAMES) {
            int maxId = this.maxId;
            L:if (maxId != 0) {
                Object[] data = idMapData;
                if (data == null) {
                    int id = mapNameToId(name);
                    if (id != 0) {
                        return getIdValue(id);
                    }
                }
                else {
                    int id = lastIdCache;
                    if (data[id - 1 + maxId] != name) {
                        id = mapNameToId(name);
                        if (id == 0) { break L; }
                           data[id - 1 + maxId] = name;
                           lastIdCache = id;
                    }
                    Object value = data[id - 1];
                    if (value == null) {
                        value = getIdValue(id);
                    }
                    else if (value == NULL_TAG) {
                        value = null;
                    }
                    return value;
                }
            }
        }
        else {
            if (maxId != 0) {
                int id = mapNameToId(name);
                if (id != 0) {
                    Object[] data = idMapData;
                    if (data == null) {
                        return getIdValue(id);
                    }
                    else {
                        Object value = data[id - 1];
                        if (value == null) {
                            value = getIdValue(id);
                        }
                        else if (value == NULL_TAG) {
                            value = null;
                        }
                        return value;
                    }
                }
            }
        }
        return super.get(name, start);
    }

    public void put(String name, Scriptable start, Object value) {
        if (maxId != 0) {
            int id = mapNameToId(name);
            if (id != 0) {
                int attr = getAttributes(id);
                if ((attr & READONLY) == 0) {
                    if (start == this) {
                        setIdValue(id, value);
                    }
                    else {
                        start.put(name, start, value);
                    }
                }
                return;
            }
        }
        super.put(name, start, value);
    }

    public void delete(String name) {
        if (maxId != 0) {
            int id = mapNameToId(name);
            if (id != 0) {
                // Let the super class to throw exceptions for sealed objects
                if (!isSealed()) {
                    int attr = getAttributes(id);
                    if ((attr & PERMANENT) == 0) {
                        deleteIdValue(id);
                    }
                    return;
                }
            }
        }
        super.delete(name);
    }

    public int getAttributes(String name, Scriptable start)
        throws PropertyException
    {
        if (maxId != 0) {
            int id = mapNameToId(name);
            if (id != 0) {
                if (hasValue(id)) {
                    return getAttributes(id);
                }
                // For ids with deleted values super will throw exceptions
            }
        }
        return super.getAttributes(name, start);
    }

    public void setAttributes(String name, Scriptable start,
                              int attributes)
        throws PropertyException
    {
        if (maxId != 0) {
            int id = mapNameToId(name);
            if (id != 0) {
                if (hasValue(id)) {
                    synchronized (this) {
                        setAttributes(id, attributes);
                    }
                    return;
                }
                // For ids with deleted values super will throw exceptions
            }
        }
        super.setAttributes(name, start, attributes);
    }

    synchronized void addPropertyAttribute(int attribute) {
        extraIdAttributes |= (byte)attribute;
        super.addPropertyAttribute(attribute);
    }

    /**
     * Redefine ScriptableObject.defineProperty to allow changing
     * values/attributes of id-based properties unless
     * getIdDefaultAttributes contains the READONLY attribute.
     * @see #getIdDefaultAttributes
     * @see org.mozilla.javascript.ScriptableObject#defineProperty
     */
    public void defineProperty(String propertyName, Object value,
                               int attributes)
    {
        if (maxId != 0) {
            int id = mapNameToId(propertyName);
            if (id != 0) {
                int default_attributes = getIdDefaultAttributes(id);
                if ((default_attributes & READONLY) != 0) {
                    // It is a bug to redefine id with readonly attributes
                    throw new RuntimeException
                        ("Attempt to redefine read-only id " + propertyName);
                }
                setAttributes(id, attributes);
                setIdValue(id, value);
                return;
            }
        }
        super.defineProperty(propertyName, value, attributes);
    }

    Object[] getIds(boolean getAll) {
        Object[] result = super.getIds(getAll);

        if (maxId != 0) {
            Object[] ids = null;
            int count = 0;

            for (int id = maxId; id != 0; --id) {
                if (hasValue(id)) {
                    if (getAll || (getAttributes(id) & DONTENUM) == 0) {
                        if (count == 0) {
                            // Need extra room for nor more then [1..id] names
                            ids = new Object[id];
                        }
                        ids[count++] = getIdName(id);
                    }
                }
            }
            if (count != 0) {
                if (result.length == 0 && ids.length == count) {
                    result = ids;
                }
                else {
                    Object[] tmp = new Object[result.length + count];
                    System.arraycopy(result, 0, tmp, 0, result.length);
                    System.arraycopy(ids, 0, tmp, result.length, count);
                    result = tmp;
                }
            }
        }
        return result;
    }

    /** Return maximum id number that should be present in each instance. */
    protected int maxInstanceId() { return 0; }

    /**
     * Map name to id of prototype or instance property.
     * Should return 0 if not found
     */
    protected abstract int mapNameToId(String name);

    /** Map id back to property name it defines.
     */
    protected abstract String getIdName(int id);

    /** Get default attributes for id.
     ** Default implementation return DONTENUM that is the standard attribute
     ** for core EcmaScript function. Typically descendants need to overwrite
     ** this for non-function attributes like length to return
     ** DONTENUM | READONLY | PERMANENT or DONTENUM | PERMANENT
     */
    protected int getIdDefaultAttributes(int id) {
        return DONTENUM;
    }

    /** Check if id value exists.
     ** Default implementation always returns true */
    protected boolean hasIdValue(int id) {
        return true;
    }

    /** Get id value.
     ** If id value is constant, descendant can call cacheIdValue to store
     ** value in the permanent cache.
     ** Default implementation creates IdFunction instance for given id
     ** and cache its value
     */
    protected Object getIdValue(int id) {
        IdFunction f = newIdFunction(id);
        f.setParentScope(getParentScope());
        return cacheIdValue(id, f);
    }

    /**
     * Set id value.
     * IdScriptable never calls this method if result of
     * <code>getIdDefaultAttributes(id)</code> contains READONLY attribute.
     * Descendants can overwrite this method to provide custom handler for
     * property assignments.
     */
    protected void setIdValue(int id, Object value) {
        synchronized (this) {
            ensureIdData()[id - 1] = (value != null) ? value : NULL_TAG;
        }
    }

    /**
     * Store value in permanent cache unless value was already assigned to id.
     * After this call IdScriptable never calls hasIdValue and getIdValue
     * for the given id.
     */
    protected Object cacheIdValue(int id, Object value) {
        synchronized (this) {
            Object[] data = ensureIdData();
            Object curValue = data[id - 1];
            if (curValue == null) {
                data[id - 1] = (value != null) ? value : NULL_TAG;
            }
            else {
                value = curValue;
            }
        }
        return value;
    }

    /**
     * Delete value represented by id so hasIdValue return false.
     * IdScriptable never calls this method if result of
     * <code>getIdDefaultAttributes(id)</code> contains PERMANENT attribute.
     * Descendants can overwrite this method to provide custom handler for
     * property delete.
     */
    protected void deleteIdValue(int id) {
        synchronized (this) {
            ensureIdData()[id - 1] = NOT_FOUND;
        }
    }

    /** 'thisObj' will be null if invoked as constructor, in which case
     ** instance of Scriptable should be returned. */
    public Object execMethod(int methodId, IdFunction function,
                             Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        throw IdFunction.onBadMethodId(this, methodId);
    }

    /** Get arity or defined argument count for method with given id.
     ** Should return -1 if methodId is not known or can not be used
     ** with execMethod call. */
    public int methodArity(int methodId) {
        return -1;
    }

    /** Activate id support with the given maximum id */
    protected void activateIdMap(int maxId) {
        this.maxId = maxId;
    }

    /** Sets whether newly constructed function objects should be sealed */
    protected void setSealFunctionsFlag(boolean sealed) {
        setSetupFlag(SEAL_FUNCTIONS_FLAG, sealed);
    }

    /**
     * Set parameters of function properties.
     * Currently only determines whether functions should use dynamic scope.
     * @param cx context to read function parameters.
     *
     * @see org.mozilla.javascript.Context#hasCompileFunctionsWithDynamicScope
     */
    protected void setFunctionParametrs(Context cx) {
        setSetupFlag(USE_DYNAMIC_SCOPE_FLAG,
                     cx.hasCompileFunctionsWithDynamicScope());
    }

    private void setSetupFlag(int flag, boolean value) {
        setupFlags = (byte)(value ? setupFlags | flag : setupFlags & ~flag);
    }

    /**
     * Prepare this object to serve as the prototype property of constructor
     * object with name <code>getClassName()<code> defined in
     * <code>scope</code>.
     * @param maxId maximum id available in prototype object
     * @param cx current context
     * @param scope object to define constructor in.
     * @param sealed indicates whether object and all its properties should
     *        be sealed
     */
    public void addAsPrototype(int maxId, Context cx, Scriptable scope,
                               boolean sealed)
    {
        activateIdMap(maxId);

        setSealFunctionsFlag(sealed);
        setFunctionParametrs(cx);

        int constructorId = mapNameToId("constructor");
        if (constructorId == 0) {
            // It is a bug to call this function without id for constructor
            throw new RuntimeException("No id for constructor property");
        }

        IdFunction ctor = newIdFunction(getClassName(), constructorId);
        ctor.initAsConstructor(scope, this);
        fillConstructorProperties(cx, ctor, sealed);
        if (sealed) {
            ctor.sealObject();
            ctor.addPropertyAttribute(READONLY);
        }

        setParentScope(ctor);
        setPrototype(getObjectPrototype(scope));
        cacheIdValue(constructorId, ctor);

        if (sealed) {
            sealObject();
        }

        defineProperty(scope, getClassName(), ctor, ScriptableObject.DONTENUM);
    }

    protected void fillConstructorProperties
        (Context cx, IdFunction ctor, boolean sealed)
    {
    }

    protected void addIdFunctionProperty
        (Scriptable obj, int id, boolean sealed)
    {
        IdFunction f = newIdFunction(id);
        if (sealed) { f.sealObject(); }
        defineProperty(obj, getIdName(id), f, DONTENUM);
    }

    /**
     * Utility method for converting target object into native this.
     * Possible usage would be to have a private function like realThis:
     * <pre>
       private NativeSomething realThis(Scriptable thisObj,
                                        IdFunction f, boolean readOnly)
       {
           while (!(thisObj instanceof NativeSomething)) {
               thisObj = nextInstanceCheck(thisObj, f, readOnly);
           }
           return (NativeSomething)thisObj;
       }
    * </pre>
    * Note that although such function can be implemented universally via
    * java.lang.Class.isInstance(), it would be much more slower.
    * @param readOnly specify if the function f does not change state of object.
    * @return Scriptable object suitable for a check by the instanceof operator.
    * @throws RuntimeException if no more instanceof target can be found
    */
    protected Scriptable nextInstanceCheck(Scriptable thisObj,
                                           IdFunction f,
                                           boolean readOnly)
    {
        if (readOnly && 0 != (setupFlags & USE_DYNAMIC_SCOPE_FLAG)) {
            // for read only functions under dynamic scope look prototype chain
            thisObj = thisObj.getPrototype();
            if (thisObj != null) { return thisObj; }
        }
        throw NativeGlobal.typeError1("msg.incompat.call",
                                      f.getFunctionName(), f);
    }

    protected IdFunction newIdFunction(int id) {
        return newIdFunction(getIdName(id), id);
    }

    protected IdFunction newIdFunction(String name, int id) {
        IdFunction f = new IdFunction(this, name, id);
        if (0 != (setupFlags & SEAL_FUNCTIONS_FLAG)) { f.sealObject(); }
        return f;
    }

    protected final Object wrap_double(double x) {
        return (x == x) ? new Double(x) : ScriptRuntime.NaNobj;
    }

    protected final Object wrap_int(int x) {
        byte b = (byte)x;
        if (b == x) { return new Byte(b); }
        return new Integer(x);
    }

    protected final Object wrap_long(long x) {
        int i = (int)x;
        if (i == x) { return wrap_int(i); }
        return new Long(x);
    }

    protected final Object wrap_boolean(boolean x) {
        return x ? Boolean.TRUE : Boolean.FALSE;
    }

    private boolean hasValue(int id) {
        Object value;
        Object[] data = idMapData;
        if (data == null || (value = data[id - 1]) == null) {
            return hasIdValue(id);
        }
        else {
            return value != NOT_FOUND;
        }
    }

    // Must be called only from synchronized (this)
    private Object[] ensureIdData() {
        Object[] data = idMapData;
        if (data == null) {
            idMapData = data = new Object[CACHE_NAMES ? maxId * 2 : maxId];
        }
        return data;
    }

    private int getAttributes(int id) {
        int attributes = getIdDefaultAttributes(id) | extraIdAttributes;
        byte[] array = attributesArray;
        if (array != null) {
            attributes |= 0xFF & array[id - 1];
        }
        return attributes;
    }

    private void setAttributes(int id, int attributes) {
        int defaultAttrs = getIdDefaultAttributes(id);
        if ((attributes & defaultAttrs) != defaultAttrs) {
            // It is a bug to set attributes to less restrictive values
            // then given by defaultAttrs
            throw new RuntimeException("Attempt to unset default attributes");
        }
        // Store only additional bits
        attributes &= ~defaultAttrs;
        byte[] array = attributesArray;
        if (array == null && attributes != 0) {
            synchronized (this) {
                array = attributesArray;
                if (array == null) {
                    attributesArray = array = new byte[maxId];
                }
            }
        }
        if (array != null) {
            array[id - 1] = (byte)attributes;
        }
    }

    private int maxId;
    private Object[] idMapData;
    private byte[] attributesArray;

    private static final boolean CACHE_NAMES = true;
    private int lastIdCache;

    private static final int USE_DYNAMIC_SCOPE_FLAG = 1 << 0;
    private static final int SEAL_FUNCTIONS_FLAG    = 1 << 1;

    private byte setupFlags;
    private byte extraIdAttributes;
}

