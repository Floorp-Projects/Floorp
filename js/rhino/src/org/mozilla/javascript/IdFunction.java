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

public class IdFunction extends BaseFunction
{
    public static final int FUNCTION_ONLY = 0;

    public static final int CONSTRUCTOR_ONLY = 1;

    public static final int FUNCTION_AND_CONSTRUCTOR = 2;

    public IdFunction(IdFunctionMaster master, String name, int id) {
        this.functionName = name;
        this.master = master;
        this.methodId = id;
    }
    
    public final int functionType() {
        return functionType;
    }
    
    public void setFunctionType(int type) {
        functionType = type;
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

    public String decompile(Context cx, int indent, boolean justbody) {
        StringBuffer sb = new StringBuffer();
        if (!justbody) {
            sb.append("function ");
            sb.append(getFunctionName());
            sb.append("() { ");
        }
        sb.append("[native code for ");
        if (master instanceof Scriptable) {
            Scriptable smaster = (Scriptable)master;
            sb.append(smaster.getClassName());
            sb.append('.');
        }
        sb.append(getFunctionName());
        sb.append(", arity=");
        sb.append(getArity());
        sb.append(justbody ? "]\n" : "] }\n");
        return sb.toString();
    }
    
    public int getArity() {
        int arity = master.methodArity(methodId);
        if (arity < 0) { 
            throw onBadMethodId(master, methodId);
        }
        return arity;
    }
    
    public int getLength() { return getArity(); }

    /** Prepare to be used as constructor .
     ** @param scope constructor scope
     ** @param prototype DontEnum, DontDelete, ReadOnly prototype property 
     ** of the constructor */
    public void initAsConstructor(Scriptable scope, Scriptable prototype) {
        setFunctionType(FUNCTION_AND_CONSTRUCTOR);
        setParentScope(scope);
        setImmunePrototypeProperty(prototype);
    }
    
    static RuntimeException onBadMethodId(IdFunctionMaster master, int id) {
        // It is program error to call id-like methods for unknown or 
        // non-function id
        return new RuntimeException("BAD FUNCTION ID="+id+" MASTER="+master);
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

    protected IdFunctionMaster master;
    protected int methodId;

    protected int functionType = FUNCTION_ONLY;
}
