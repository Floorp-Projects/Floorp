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

// API class

package org.mozilla.javascript;

public class IdFunction extends ScriptableObject implements Function
{
    /** Indicates that native implementation was overwritten by script */
    public static final 
        IdFunction WAS_OVERWRITTEN = new IdFunction(null, "", 0);
    
    public static final int FUNCTION_ONLY = 0;

    public static final int CONSTRUCTOR_ONLY = 1;

    public static final int FUNCTION_AND_CONSTRUCTOR = 2;

    public IdFunction(IdFunctionMaster master, String name, int id) {
        this.master = master;
        this.methodName = name;
        this.methodId = id;
    }
    
    public final int functionType() {
        return functionType;
    }
    
    public void setFunctionType(int type) {
        functionType = type;
    }
    
    public String getClassName() { return "Function"; }

    public boolean has(String name, Scriptable start) {
        return nameToId(name) != 0 || super.has(name, start);
    }

    public Object get(String name, Scriptable start) {
        int id = nameToId(name);
        return (0 != id) ? getField(id) : super.get(name, start);
    }

    public void put(String name, Scriptable start, Object value) {
        if (nameToId(name) == 0) {
            super.put(name, start, value);
        }
    }

    public void delete(String name) {
        // Let the super class throw exceptions for sealed objects
        if (isSealed() || nameToId(name) == 0) {
            super.delete(name);
        }
    }
    
    /**
     * Implements the instanceof operator for JavaScript Function objects.
     * <p>
     * <code>
     * foo = new Foo();<br>
     * foo instanceof Foo;  // true<br>
     * </code>
     *
     * @param instance The value that appeared on the LHS of the instanceof
     *              operator
     * @return true if the "prototype" property of "this" appears in
     *              value's prototype chain
     *
     */
    public boolean hasInstance(Scriptable instance) {
        Object protoProp = ScriptableObject.getProperty(this, "prototype");
        if (protoProp instanceof Scriptable && protoProp != Undefined.instance)
        {
            return ScriptRuntime.jsDelegatesTo(instance, (Scriptable)protoProp);
        }
        throw NativeGlobal.typeError1
            ("msg.instanceof.bad.prototype", this.methodName, instance);
    }
    
    public Object call(Context cx, Scriptable scope, Scriptable thisObj, 
                       Object[] args)
        throws JavaScriptException
    {
        if (functionType != CONSTRUCTOR_ONLY) {
            return master.execMethod(methodId, this, cx, scope, thisObj, args);
        }
        else {
            return Undefined.instance;
        }
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        if (functionType != FUNCTION_ONLY) {
            // It is program error not to return Scriptable from constructor
            Scriptable result = (Scriptable)master.execMethod(methodId, this,
                                                              cx, scope, 
                                                              null, args);
            postConstruction(result);
            return result;
        }
        else {
            return Undefined.instance;
        }
    }

    public Scriptable getPrototype() {
        // Lazy initialization of prototype: for native functions this
        // may not be called at all
        Scriptable proto = super.getPrototype(); 
        if (proto == null) {
            proto = getFunctionPrototype(getParentScope());
            setPrototype(proto);
        }
        return proto;
    }

    // Copied from NativeFunction
    protected Scriptable getClassPrototype() {
        Object protoVal = immunePrototypeProperty;
        if (protoVal == NOT_FOUND) {
            protoVal = super.get("prototype", this);
        }
        if (!(protoVal instanceof Scriptable)
            || protoVal == Undefined.instance)
        {
            protoVal = getClassPrototype(this, "Object");
        }
        return (Scriptable) protoVal;
    }

    protected Object toStringForScript(Context cx) {
        StringBuffer sb = new StringBuffer();
        sb.append("function ");
        sb.append(methodName);
        sb.append("() { [native code for ");
        if (master instanceof Scriptable) {
            Scriptable smaster = (Scriptable)master;
            sb.append(smaster.getClassName());
            sb.append('.');
        }
        sb.append(methodName);
        sb.append(", arity=");
        sb.append(getArity());
        sb.append("] }");
        return sb.toString();
    }
    
    /** Prepare to be used as constructor .
     ** @param scope constructor scope
     ** @param prototype DontEnum, DontDelete, ReadOnly prototype property 
     ** of the constructor */
    public void initAsConstructor(Scriptable scope, Scriptable prototype) {
        setFunctionType(FUNCTION_AND_CONSTRUCTOR);
        setParentScope(scope);
        setImmunePrototypeProperty(prototype);
    }
    
    /** Make value as DontEnum, DontDelete, ReadOnly
     ** prototype property of this native Function object
     ** if not already set
     */
    protected void setImmunePrototypeProperty(Object value) {
        if (immunePrototypeProperty == NOT_FOUND) {
            immunePrototypeProperty = value;
        }
    }
    
    static RuntimeException onBadMethodId(IdFunctionMaster master, int id) {
        // It is program error to call id-like methods for unknown or 
        // non-function id
        return new RuntimeException("BAD FUNCTION ID="+id+" MASTER="+master);
    }



    private int getArity() {
        int arity = master.methodArity(methodId);
        if (arity < 0) { 
            throw onBadMethodId(master, methodId);
        }
        return arity;
    }
    
    // Copied from NativeFunction.construct
    private void postConstruction(Scriptable newObj) {
        if (newObj.getPrototype() == null) {
            newObj.setPrototype(getClassPrototype());
        }
        if (newObj.getParentScope() == null) {
            Scriptable parent = getParentScope();
            if (newObj != parent) {
                newObj.setParentScope(parent);
            }
        }
    }

    private Object getField(int fieldId) {
        switch (fieldId) {
            case ID_ARITY: case ID_LENGTH: 
                return new Integer(getArity());
            case ID_NAME: 
                return methodName;
            case ID_PROTOTYPE:
                return immunePrototypeProperty;
        }
        return null;
    }
    
    private int nameToId(String s) {
        int id = 0;
        String guess = null;
        switch (s.length()) {
            case 4: guess = "name"; id = ID_NAME; break;
            case 5: guess = "arity"; id = ID_ARITY; break;
            case 6: guess = "length"; id = ID_LENGTH; break;
            case 9: 
                if (immunePrototypeProperty != NOT_FOUND) {
                // Try to guess only if immunePrototypeProperty is defined, 
                // delegate to ScriptableObject otherwise
                    guess = "prototype"; id = ID_PROTOTYPE; 
                }
                break;
            
        }
        return (guess != null && guess.equals(s)) ? id : 0;
    }

    private static final int 
        ID_ARITY     = 1,
        ID_LENGTH    = 2,
        ID_NAME      = 3,
        ID_PROTOTYPE = 4;
        
    protected /*final*/ IdFunctionMaster master;
    protected /*final*/ int methodId;
    
    protected String methodName;

    protected int functionType = FUNCTION_ONLY;

    // If != NOT_FOUND, represent script-immune prototype property
    private Object immunePrototypeProperty = NOT_FOUND; 
}
