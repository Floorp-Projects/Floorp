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
    getMaximumId
    mapNameToId
    getIdName
    execMethod
    methodArity

To define non-function properties, the descendant should customize
    getIdValue
    setIdValue
    getIdDefaultAttributes
to get/set property value and provide its default attributes.

To customize initializition of constructor and protype objects, descendant
may override scopeInit or fillConstructorProperties methods.

*/
public abstract class IdScriptable extends ScriptableObject
    implements IdFunctionMaster, ScopeInitializer
{
    public boolean has(String name, Scriptable start) {
        if (maxId != 0) {
            int id = getId(name);
            if (id != 0) {
                return hasIdValue(id);
            }
        }
        return super.has(name, start);
    }

    public Object get(String name, Scriptable start) {
        if (maxId != 0) {
            int id = getId(name);
            if (id != 0) {
                Object[] data = idMapData;
                if (data == null) {
                    return getIdValue(id, start);
                }
                else {
                    Object value = data[id - 1];
                    if (value == null) {
                        value = getIdValue(id, start);
                    }
                    else if (value == NULL_TAG) {
                        value = null;
                    }
                    return value;
                }
            }
        }
        return super.get(name, start);
    }

    public void put(String name, Scriptable start, Object value) {
        if (maxId != 0) {
            int id = getId(name);
            if (id != 0) {
                int attr = getAttributes(id);
                if ((attr & READONLY) == 0) {
                    setIdValue(id, start, value);
                }
                return;
            }
        }
        super.put(name, start, value);
    }

    public void delete(String name) {
        if (maxId != 0) {
            int id = getId(name);
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
            int id = getId(name);
            if (id != 0) {
                if (hasIdValue(id)) {
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
            int id = getId(name);
            if (id != 0) {
                if (hasIdValue(id)) {
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
        extraIdAttributes |= attribute;
        if (maxId != 0) {
            byte[] array = attributesArray;
            if (array != null) {
                for (int i = attributesArray.length; i-- != 0;) {
                    int old = array[i];
                    if (old != 0) {
                        array[i] = (byte)(attribute | old);
                    }
                }
            }
        }
        super.addPropertyAttribute(attribute);
    }

    Object[] getIds(boolean getAll) {
        Object[] result = super.getIds(getAll);
        
        if (maxId != 0) {
            Object[] ids = null;
            int count = 0;
            
            for (int id = maxId; id != 0; --id) {
                if (hasIdValue(id)) {
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
                }
            }
        }
        return result;
    }


    /** Return minimum possible id, must be 0 or negative number.
     ** If descendant needs to use ids not visible via mapNameToId, 
     ** to define, for example, IdFunction-based properties in other objects,
     ** it should use negative number and adjust getMinimumId() value
     */
    protected int getMinimumId() { return 0; }

    /** Return maximum id, must be positive number.
     */
    protected abstract int getMaximumId();

    /** Map name to id of prototype or instance property.
     ** Should return 0 if not found or value within [1..getMaximumId()].
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
     ** Default implementation return false only if id were explicitly deleted
     ** via deleteIdValue */
    protected boolean hasIdValue(int id) {
        Object[] data = idMapData;
        return data == null || data[id - 1] != NOT_FOUND;
    }

    /** Get id value. 
     ** Default implementation returns IdFunction instance for given id.
     ** If id value is constant, descendant can call cacheIdValue to store
     ** value in the permanent cache.
     */
    protected Object getIdValue(int id, Scriptable start) {
        IdFunction f = newIdFunction(id);
        f.setParentScope(getParentScope());
        return cacheIdValue(id, f);
    }

    /** Set id value. */
    protected void setIdValue(int id, Scriptable start, Object value) {
        if (start == this) {
            synchronized (this) {
                ensureIdData()[id - 1] = (value != null) ? value : NULL_TAG;
            }
        }
        else {
            start.put(getIdName(id), start, value);
        }
    }
    
    /** Store value in a permamnet cache. After this call getIdValue will
     ** never be called for id. */
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

    /** Delete value represented by id so hasIdValue return false. 
     ** Note: this will be called only for id without PERMANENT attribute.
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
    
    protected void activateIdMap(Context cx, boolean sealed) {
        maxId = getMaximumId();
        useDynamicScope = cx.hasCompileFunctionsWithDynamicScope();
        sealFunctions = sealed;
    }

    /** Do scope initialization. 
     ** Default implementation calls activateIdMap() and then if 
     ** mapNameToId("constructor") returns positive id, defines EcmaScript
     ** constructor with name getClassName in scope and makes its prototype
     ** property to point to this object.
     */ 
    public void scopeInit(Context cx, Scriptable scope, boolean sealed) {

        activateIdMap(cx, sealed);

        int constructorId = mapNameToId("constructor");
        if (constructorId > 0) {

            IdFunction ctor = newIdFunction(constructorId);
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

            defineProperty(scope, getClassName(), ctor,
                           ScriptableObject.DONTENUM);
        }
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

/** Utility method for converting target object into native this.
    Possible usage would be to have a private function like realThis:

    private NativeSomething realThis(Scriptable thisObj,
                                     IdFunction f, boolean readOnly)
    {
        while (!(thisObj instanceof NativeSomething)) {
            thisObj = nextInstanceCheck(thisObj, f, readOnly);
        }
        return (NativeSomething)thisObj;
    }

    Note that although such function can be implemented universally via
    java.lang.Class.isInstance(), it would be much more slower.

    @param readOnly specify if the function f does not change state of object.
    @return Scriptable object suitable for a check by the instanceof operator.
    @throws RuntimeException if no more instanceof target can be found
*/
    protected Scriptable nextInstanceCheck(Scriptable thisObj,
                                           IdFunction f,
                                           boolean readOnly)
    {
        if (readOnly && useDynamicScope) {
            // for read only functions under dynamic scope look prototype chain
            thisObj = thisObj.getPrototype();
            if (thisObj != null) { return thisObj; }
        }
        throw NativeGlobal.typeError1("msg.incompat.call", f.methodName, f);
    }

    protected IdFunction newIdFunction(int id) {
        IdFunction f = new IdFunction(this, getIdName(id), id);
        if (sealFunctions) { f.sealObject(); }
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

    private int getId(String name) {
        Object[] data = idMapData;
        if (data == null || !CACHE_NAMES) { return mapNameToId(name); }
        else {
            int id = lastIdCache;
            if (data[id - 1 + maxId] != name) {
                id = mapNameToId(name);
                if (id != 0) {
                    data[id - 1 + maxId] = name;
                    lastIdCache = id;
                }
            }
            return id;
        }
    }
    
    private Object[] ensureIdData() {
        Object[] data = idMapData;
        if (data == null) { 
            idMapData = data = new Object[CACHE_NAMES ? maxId * 2 : maxId];
        }
        return data;
    }
    
    private int getAttributes(int id) {
        int attributes;
        byte[] array = attributesArray;
        if (array == null || (attributes = array[id - 1]) == 0) { 
            attributes = getIdDefaultAttributes(id) | extraIdAttributes; 
        }
        return VISIBLE_ATTR_MASK & attributes;
    }

    private void setAttributes(int id, int attributes) {
        byte[] array = attributesArray;
        if (array == null) {
            synchronized (this) {
                array = attributesArray;
                if (array == null) {
                    attributesArray = array = new byte[maxId];
                }
            }
        }
        attributes &= VISIBLE_ATTR_MASK;
        array[id - 1] = (byte)(ASSIGNED_ATTRIBUTE_MASK | attributes);
    }

    private static final boolean CACHE_NAMES = true;

    private static final int 
        VISIBLE_ATTR_MASK = READONLY | DONTENUM | PERMANENT;

    private static final int ASSIGNED_ATTRIBUTE_MASK = 0x80;
    
    private static final Object NULL_TAG = new Object();

    private int maxId;
    private Object[] idMapData;
    private byte[] attributesArray;
    private int extraIdAttributes;

    private int lastIdCache;

    private boolean useDynamicScope;

    protected boolean sealFunctions;
}

