/* -*- Mode: java; tab-width: 4; indent-tabs-mode: 1; c-basic-offset: 4 -*-
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

To define non-function properties, the descendant should override
    getIdValue
    setIdValue
    getIdAttributes
to get/set property value and provide its default attributes.

During initialization descendant should call setMaxId directly or via calling addAsPrototype.

To customize initializition of constructor and protype objects, descendant
may override scopeInit or fillConstructorProperties methods.

*/
public abstract class IdScriptable extends ScriptableObject
    implements IdFunctionMaster
{
    public boolean has(String name, Scriptable start)
    {
        if (maxId != 0) {
            int id = mapNameToId_writeCached(name);
            if (id != 0) {
                return hasValue(id);
            }
        }
        return super.has(name, start);
    }

    public Object get(String name, Scriptable start)
    {
        if (maxId != 0) {
            int id = mapNameToId_writeCached(name);
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
                    else if (value == UniqueTag.NULL_VALUE) {
                        value = null;
                    }
                    return value;
                }
            }
        }
        return super.get(name, start);
    }

    public void put(String name, Scriptable start, Object value)
    {
        if (maxId != 0) {
            int id = mapNameToId_cached(name);
            if (id != 0) {
                if (start == this && isSealed()) {
                    throw Context.reportRuntimeError1("msg.modify.sealed",
                                                      name);
                }
                int attr = getIdAttributes(id);
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

    public void delete(String name)
    {
        if (maxId != 0) {
            int id = mapNameToId(name);
            if (id != 0) {
                // Let the super class to throw exceptions for sealed objects
                if (!isSealed()) {
                    int attr = getIdAttributes(id);
                    if ((attr & PERMANENT) == 0) {
                        deleteIdValue(id);
                    }
                    return;
                }
            }
        }
        super.delete(name);
    }

    public int getAttributes(String name)
    {
        if (maxId != 0) {
            int id = mapNameToId(name);
            if (id != 0) {
                if (hasValue(id)) {
                    return getIdAttributes(id);
                }
                // For ids with deleted values super will throw exceptions
            }
        }
        return super.getAttributes(name);
    }

    public void setAttributes(String name, int attributes)
    {
        if (maxId != 0) {
            int id = mapNameToId(name);
            if (id != 0) {
                synchronized (this) {
                    if (hasValue(id)) {
                        setIdAttributes(id, attributes);
                        return;
                    }
                }
                // For ids with deleted values super will throw exceptions
            }
        }
        super.setAttributes(name, attributes);
    }

    /**
     * Redefine ScriptableObject.defineProperty to allow changing
     * values/attributes of id-based properties unless
     * getIdAttributes contains the READONLY attribute.
     * @see #getIdAttributes
     * @see org.mozilla.javascript.ScriptableObject#defineProperty
     */
    public void defineProperty(String propertyName, Object value,
                               int attributes)
    {
        if (maxId != 0) {
            int id = mapNameToId(propertyName);
            if (id != 0) {
                int current_attributes = getIdAttributes(id);
                if ((current_attributes & READONLY) != 0) {
                    // It is a bug to redefine id with readonly attributes
                    throw new RuntimeException
                        ("Attempt to redefine read-only id " + propertyName);
                }
                setIdAttributes(id, attributes);
                setIdValue(id, value);
                return;
            }
        }
        super.defineProperty(propertyName, value, attributes);
    }

    Object[] getIds(boolean getAll)
    {
        Object[] result = super.getIds(getAll);

        if (maxId != 0) {
            Object[] ids = null;
            int count = 0;

            for (int id = maxId; id != 0; --id) {
                if (hasValue(id)) {
                    if (getAll || (getIdAttributes(id) & DONTENUM) == 0) {
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

// Try to avoid calls to mapNameToId by quering name cache
    private int mapNameToId_cached(String name)
    {
        if (CACHE_NAMES) {
            Object[] data = idMapData;
            if (data != null) {
                int id = lastIdCache;
                if (data[id - 1 + maxId] == name) {
                    return id;
                }
            }
        }
        return mapNameToId(name);
    }

// Same as mapNameToId_cached but put to cache id found by mapNameToId
    private int mapNameToId_writeCached(String name) {
        if (CACHE_NAMES) {
            Object[] data = idMapData;
            if (data != null) {
                int id = lastIdCache;
                if (data[id - 1 + maxId] == name) {
                    return id;
                }
                id = mapNameToId(name);
                if (id != 0) {
                    data[id - 1 + maxId] = name;
                    lastIdCache = id;
                }
                return id;
            }
        }
        return mapNameToId(name);
    }

    /**
     * Map name to id of prototype or instance property.
     * Should return 0 if not found
     */
    protected abstract int mapNameToId(String name);

    /** Map id back to property name it defines.
     */
    protected String getIdName(int id)
    {
        throw new IllegalArgumentException(String.valueOf(id));
    }

    /** Get attributes for id.
     ** Default implementation return DONTENUM that is the standard attribute
     ** for core EcmaScript function. Typically descendants need to overwrite
     ** this for non-function attributes like length to return
     ** DONTENUM | READONLY | PERMANENT or DONTENUM | PERMANENT
     */
    protected int getIdAttributes(int id)
    {
        return DONTENUM;
    }

    /**
     * Set attributes for id.
     * Descendants should override the default implementation if they want to
     * allow to change id attributes since the default implementation throw an
     * exception unless new attributes eqaul the result of
     * <tt>getIdAttributes(id)</tt>.
     */
    protected void setIdAttributes(int id, int attributes)
    {
        int current = getIdAttributes(id);
        if (attributes != current) {
            throw new RuntimeException(
                "Change of attributes for this id is not supported");
        }
    }

    /** Check if id value exists.
     ** Default implementation always returns true */
    protected boolean hasIdValue(int id)
    {
        return true;
    }

    /** Get id value.
     ** If id value is constant, descendant can call cacheIdValue to store
     ** value in the permanent cache.
     ** Default implementation creates IdFunction instance for given id
     ** and cache its value
     */
    protected Object getIdValue(int id)
    {
        Scriptable scope = ScriptableObject.getTopLevelScope(this);
        IdFunction f = newIdFunction(id, scope);
        return cacheIdValue(id, f);
    }

    /**
     * Set id value.
     * IdScriptable never calls this method if result of
     * <code>getIdAttributes(id)</code> contains READONLY attribute.
     * Descendants can overwrite this method to provide custom handler for
     * property assignments.
     */
    protected void setIdValue(int id, Object value)
    {
        synchronized (this) {
            Object[] data = ensureIdData();
            data[id - 1] = (value != null) ? value : UniqueTag.NULL_VALUE;
        }
    }

    /**
     * Store value in permanent cache unless value was already assigned to id.
     * After this call IdScriptable never calls hasIdValue and getIdValue
     * for the given id.
     */
    protected Object cacheIdValue(int id, Object value)
    {
        synchronized (this) {
            Object[] data = ensureIdData();
            Object curValue = data[id - 1];
            if (curValue == null) {
                data[id - 1] = (value != null) ? value : UniqueTag.NULL_VALUE;
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
     * <code>getIdAttributes(id)</code> contains PERMANENT attribute.
     * Descendants can overwrite this method to provide custom handler for
     * property delete.
     */
    protected void deleteIdValue(int id)
    {
        synchronized (this) {
            ensureIdData()[id - 1] = NOT_FOUND;
        }
    }

    /** 'thisObj' will be null if invoked as constructor, in which case
     ** instance of Scriptable should be returned. */
    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        throw f.unknown();
    }

    /**
     * Get arity or defined argument count for the given function id.
     * If subclass overrides ths method, it should always calls
     * <tt>super.methodArity(id)</tt> for unknown functions.
     */
    protected int methodArity(int methodId)
    {
        throw new IllegalArgumentException(String.valueOf(methodId));
    }

    /** Get maximum id mapNameToId can generate */
    protected final int getMaxId()
    {
        return maxId;
    }

    /** Set maximum id mapNameToId can generate */
    protected final void setMaxId(int maxId)
    {
        // maxId can only go up
        if (maxId < this.maxId) Kit.codeBug();
        this.maxId = maxId;
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
        setMaxId(maxId);

        int constructorId = mapNameToId("constructor");
        if (constructorId == 0) {
            // It is a bug to call this function without id for constructor
            throw new RuntimeException("No id for constructor property");
        }

        // Set scope and prototype unless IdScriptable is top level scope itself
        if (scope != this && scope != null) {
            setParentScope(scope);
            setPrototype(getObjectPrototype(scope));
        }

        IdFunction ctor = newIdFunction(getClassName(), constructorId, scope);
        ctor.markAsConstructor(this);
        fillConstructorProperties(cx, ctor, sealed);
        cacheIdValue(constructorId, ctor);

        ctor.exportAsScopeProperty(sealed);
    }

    protected void fillConstructorProperties
        (Context cx, IdFunction ctor, boolean sealed)
    {
    }

    protected void addIdFunctionProperty
        (Scriptable obj, int id, boolean sealed)
    {
        Scriptable scope = ScriptableObject.getTopLevelScope(this);
        IdFunction f = newIdFunction(id, scope);
        if (sealed) { f.sealObject(); }
        defineProperty(obj, getIdName(id), f, DONTENUM);
    }

    /**
     * Utility method to construct type error to indicate incompatible call
     * when converting script thisObj to a particular type is not possible.
     * Possible usage would be to have a private function like realThis:
     * <pre>
     *  private static NativeSomething realThis(Scriptable thisObj,
     *                                          IdFunction f)
     *  {
     *      if (!(thisObj instanceof NativeSomething))
     *          throw incompatibleCallError(f);
     *      return (NativeSomething)thisObj;
     * }
     * </pre>
     * Note that although such function can be implemented universally via
     * java.lang.Class.isInstance(), it would be much more slower.
     * @param readOnly specify if the function f does not change state of
     * object.
     * @return Scriptable object suitable for a check by the instanceof
     * operator.
     * @throws RuntimeException if no more instanceof target can be found
     */
    protected static EcmaError incompatibleCallError(IdFunction f)
    {
        throw ScriptRuntime.typeError1("msg.incompat.call",
                                       f.getFunctionName());
    }

    protected IdFunction newIdFunction(int id, Scriptable scope)
    {
        return newIdFunction(getIdName(id), id, scope);
    }

    protected IdFunction newIdFunction(String name, int id, Scriptable scope)
    {
        IdFunction f = new IdFunction(this, null, id, name, methodArity(id),
                                      scope);
        if (isSealed()) { f.sealObject(); }
        return f;
    }

    protected final Object wrap_double(double x)
    {
        return (x == x) ? new Double(x) : ScriptRuntime.NaNobj;
    }

    protected final Object wrap_int(int x)
    {
        return ScriptRuntime.wrapInt(x);
    }

    protected final Object wrap_long(long x)
    {
        int i = (int)x;
        if (i == x) { return wrap_int(i); }
        return new Long(x);
    }

    protected final Object wrap_boolean(boolean x)
    {
        return x ? Boolean.TRUE : Boolean.FALSE;
    }

    private boolean hasValue(int id)
    {
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
    private Object[] ensureIdData()
    {
        Object[] data = idMapData;
        if (data == null) {
            idMapData = data = new Object[CACHE_NAMES ? maxId * 2 : maxId];
        }
        return data;
    }

    private int maxId;
    private Object[] idMapData;

    private static final boolean CACHE_NAMES = true;
    private int lastIdCache;
}

