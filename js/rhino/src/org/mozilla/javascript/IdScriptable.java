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
    getMaxPrototypeMethodId
    mapNameToMethodId
    execMethod
    methodArity

Any implementation of execMethod and methodArity should assume that if
methodId is 0 (CONSTRUCTOR_ID), it denotes EcmaScript constructor for
<class-name> object.

To customize initializition of constructor and protype objects, descendant
may override initForGlobal or fillConstructorProperties methods. 

To wrap primitive java types to script objects, descaendant
should use wrap_<primitive-type> set of methods.

*/
public abstract class IdScriptable extends ScriptableObject
    implements IdFunction.Master, ScopeInitializer
{
    public static final int CONSTRUCTOR_ID = 0;

    /** 'thisObj' will be null if invoked as constructor, in which case
     ** instance of Scriptable should be returned.
     */
    public abstract Object execMethod(int methodId, IdFunction function,
                             Context cx, Scriptable scope, 
                             Scriptable thisObj, Object[] args)
        throws JavaScriptException;

    public abstract int methodArity(int methodId, IdFunction function);

    /** Return maximum method id for prototype function */
    protected abstract int getMaxPrototypeMethodId();
    
    /** Map name into id for prototypr method. 
     ** Should return 0 if not name is not prototype method 
     ** or value between 1 and getMaxPrototypeMethodId()
     */
    protected abstract int mapNameToMethodId(String name);

    public boolean has(String name, Scriptable start) {
        if (prototypeFunctionPool != null) {
            int id = mapNameToMethodId(name);
            if (id != 0) { 
                IdFunction f = prototypeFunctionPool[id];
                if (f != IdFunction.WAS_OVERWRITTEN) { return true; }
            }
            else {
                if (null != checkConstructor(name)) { return true; }
            }
        }
        return super.has(name, start);
    }

    public Object get(String name, Scriptable start) {
        L: if (prototypeFunctionPool != null) {
            IdFunction f = lastFunction;
            if (f.methodName == name) { 
                if (prototypeFunctionPool[f.methodId] == f) {
                    return f;
                }
                lastFunction = IdFunction.WAS_OVERWRITTEN;
            }
            int id = mapNameToMethodId(name);
            if (id != 0) {
                f = prototypeFunctionPool[id];
                if (f == null) { f = wrapMethod(name, id); }
                if (f == IdFunction.WAS_OVERWRITTEN) { break L; }
            }
            else {
                f = checkConstructor(name);
                if (f == null) { break L; }
            }
            // Update cache
            f.methodName = name;
            lastFunction = f;
            return f; 
        }
        return super.get(name, start);
    }

    public void put(String name, Scriptable start, Object value) {
        if (doOverwrite(name, start)) {
            super.put(name, start, value);
        }
    }

    public void delete(String name) {
        if (doOverwrite(name, this)) {
            super.delete(name);
        }
    }
    
    public void scopeInit(Context cx, Scriptable scope, boolean sealed) {
        useDynamicScope = cx.hasCompileFunctionsWithDynamicScope();

        prototypeFunctionPool = new IdFunction[getMaxPrototypeMethodId() + 1];

        seal_functions = sealed;
        
        setPrototype(getObjectPrototype(scope));
        
        String name = getClassName();

        IdFunction ctor = newIdFunction(name, CONSTRUCTOR_ID);
        ctor.setFunctionType(IdFunction.FUNCTION_AND_CONSTRUCTOR);
        ctor.setParentScope(scope);
        ctor.setPrototype(getFunctionPrototype(scope));
        setParentScope(ctor);

        ctor.setImmunePrototypeProperty(this);

        fillConstructorProperties(cx, ctor, sealed);

        if (!name.equals("With")) {
            // A "With" object would delegate these calls to the prototype:
            // not the right thing to do here!
            prototypeFunctionPool[CONSTRUCTOR_ID] = ctor;
        }
        
        if (sealed) {
            ctor.sealObject();
            ctor.addPropertyAttribute(READONLY);
            sealObject();
        }

        defineProperty(scope, name, ctor, ScriptableObject.DONTENUM);
    }

    protected void fillConstructorProperties
        (Context cx, IdFunction ctor, boolean sealed)
    {
    }

    protected void addIdFunctionProperty
        (Scriptable obj, String name, int id, boolean sealed)
    {
        IdFunction f = newIdFunction(name, id);
        if (sealed) { f.sealObject(); }
        defineProperty(obj, name, f, DONTENUM);
    }
    
/** Utility method for converting target object into native this.
    Possible usage would be to have a private function like realThis:
        
    private NativeSomething realThis(Scriptable thisObj, 
                                     IdFunction f, boolean searchPrototype) 
    {
        while (!(thisObj instanceof NativeSomething)) {
            thisObj = nextInstanceCheck(thisObj, f, searchPrototype);
        }
        return (NativeSomething)thisObj;
    } 
     
    Note that although such function can be implemented universally via
    java.lang.Class.isInstance(), it would be much more slower
*/
    protected Scriptable nextInstanceCheck(Scriptable thisObj, 
                                           IdFunction f, 
                                           boolean searchPrototype) 
    {
        if (searchPrototype && useDynamicScope) {
            thisObj = thisObj.getPrototype();
            if (thisObj != null) { return thisObj; }
        }
           throw NativeGlobal.typeError1("msg.incompat.call", f.methodName, f);
    }
    
    protected IdFunction newIdFunction(String name, int id) {
        return new IdFunction(this, name, id);
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
    
    // Return true to invoke put/delete in super class
    private boolean doOverwrite(String name, Scriptable start) {
        // Let the super class to throw exceptions for sealed objects
        if (!isSealed() && prototypeFunctionPool != null) { 
            if (null != checkConstructor(name)) {
                // If constructor is present, it is read-only
                return false;
            }
            if (this == start) {
                int id = mapNameToMethodId(name);
                if (id != 0) {
                    overwriteMethod(id);
                    return true;
                }
            }
        }
        return true;
    }
    
    private IdFunction wrapMethod(String name, int id) {
        synchronized (this) {
            IdFunction f = prototypeFunctionPool[id];
            if (f == null) {
                f = newIdFunction(name, id);
                if (seal_functions) { f.sealObject(); }
                prototypeFunctionPool[id] = f;
            }
            return f;
        }
    }
    
    private void overwriteMethod(int id) {
        if (prototypeFunctionPool[id] != IdFunction.WAS_OVERWRITTEN) {
            // Must be synchronized to avoid clearance of overwritten mark
            // by another thread running in wrapMethod 
            synchronized (this) {
                prototypeFunctionPool[id] = IdFunction.WAS_OVERWRITTEN;
            }
        }
    }
    
    private IdFunction checkConstructor(String name) {
        return name.equals("constructor") 
            ? prototypeFunctionPool[CONSTRUCTOR_ID]
            : null;
    }

    private IdFunction[] prototypeFunctionPool;
    
// Not null to simplify logic 
    private IdFunction lastFunction = IdFunction.WAS_OVERWRITTEN;
    
    private boolean seal_functions;

    private boolean useDynamicScope;
}

