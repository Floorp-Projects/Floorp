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
 * The Original Code is Delegator.java, released Sep 27, 2000.
 *
 * The Initial Developer of the Original Code is Matthias Radestock.
 * <matthias@sorted.org>. Portions created by Matthias Radestock are
 * Copyright (C) 2000 Matthias Radestock. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Matthias Radestock, Redfig Ltd (http://www.redfig.com)
 * Matthias Radestock, LShift Ltd (http://www.lshift.net)
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

/**
 * This is a helper class for implementing wrappers around Scriptable
 * objects. It implements the Function interface and delegates all
 * invocations to a delegee Scriptable object. The normal use of this
 * class involves creating a sub-class and overriding one or more of
 * the methods.
 *
 * A useful application is the implementation of interceptors,
 * pre/post conditions, debugging.
 *
 * @see Function
 * @see Scriptable
 * @author Matthias Radestock
 */

public class Delegator implements Function {

    protected Scriptable obj = null;

    /**
     * Create a Delegator prototype.
     *
     * This constructor should only be used for creating prototype
     * objects of Delegator.
     *
     * @see org.mozilla.javascript.Delegator#construct
     */
    public Delegator() {
    }

    /**
     * Create a new Delegator that forwards requests to a delegee
     * Scriptable object.
     *
     * @param obj the delegee
     * @see org.mozilla.javascript.Scriptable
     */
    public Delegator(Scriptable obj) {
        this.obj = obj;
    }

    /**
     * Crete new Delegator instance.
     * The default implementation calls this.getClass().newInstance().
     *
     * @see #construct(Context cx, Scriptable scope, Object[] args)
     */
    protected Delegator newInstance()
    {
        try {
            return (Delegator)this.getClass().newInstance();
        } catch (Exception ex) {
            throw Context.throwAsScriptRuntimeEx(ex);
        }
    }

    /**
     * Retrieve the delegee.
     *
     * @return the delegee
     */
    public Scriptable getDelegee() {
        return obj;
    }
    /**
     * Set the delegee.
     *
     * @param obj the delegee
     * @see org.mozilla.javascript.Scriptable
     */
    public void setDelegee(Scriptable obj) {
        this.obj = obj;
    }
    /**
     * @see org.mozilla.javascript.Scriptable#getClassName
     */
    public String getClassName() {
        return obj.getClassName();
    }
    /**
     * @see org.mozilla.javascript.Scriptable#get
     */
    public Object get(String name, Scriptable start) {
        return obj.get(name,start);
    }
    /**
     * @see org.mozilla.javascript.Scriptable#get
     */
    public Object get(int index, Scriptable start) {
        return obj.get(index,start);
        }
    /**
     * @see org.mozilla.javascript.Scriptable#has
     */
    public boolean has(String name, Scriptable start) {
        return obj.has(name,start);
        }
    /**
     * @see org.mozilla.javascript.Scriptable#has
     */
    public boolean has(int index, Scriptable start) {
        return obj.has(index,start);
        }
    /**
     * @see org.mozilla.javascript.Scriptable#put
     */
    public void put(String name, Scriptable start, Object value) {
        obj.put(name,start,value);
    }
    /**
     * @see org.mozilla.javascript.Scriptable#put
     */
    public void put(int index, Scriptable start, Object value) {
        obj.put(index,start,value);
    }
    /**
     * @see org.mozilla.javascript.Scriptable#delete
     */
    public void delete(String name) {
        obj.delete(name);
    }
    /**
     * @see org.mozilla.javascript.Scriptable#delete
     */
    public void delete(int index) {
        obj.delete(index);
    }
    /**
     * @see org.mozilla.javascript.Scriptable#getPrototype
     */
    public Scriptable getPrototype() {
        return obj.getPrototype();
    }
    /**
     * @see org.mozilla.javascript.Scriptable#setPrototype
     */
    public void setPrototype(Scriptable prototype) {
        obj.setPrototype(prototype);
    }
    /**
     * @see org.mozilla.javascript.Scriptable#getParentScope
     */
    public Scriptable getParentScope() {
        return obj.getParentScope();
    }
    /**
     * @see org.mozilla.javascript.Scriptable#setParentScope
     */
    public void setParentScope(Scriptable parent) {
        obj.setParentScope(parent);
    }
    /**
     * @see org.mozilla.javascript.Scriptable#getIds
     */
    public Object[] getIds() {
        return obj.getIds();
    }
    /**
     * Note that this method does not get forwarded to the delegee if
     * the <code>hint</code> parameter is null,
     * <code>ScriptRuntime.ScriptableClass</code> or
     * <code>ScriptRuntime.FunctionClass</code>. Instead the object
     * itself is returned.
     *
     * @param hint the type hint
     * @return the default value
     *
     * @see org.mozilla.javascript.Scriptable#getDefaultValue
     */
    public Object getDefaultValue(Class hint) {
        return (hint == null ||
                hint == ScriptRuntime.ScriptableClass ||
                hint == ScriptRuntime.FunctionClass) ?
            this : obj.getDefaultValue(hint);
    }
    /**
     * @see org.mozilla.javascript.Scriptable#hasInstance
     */
    public boolean hasInstance(Scriptable instance) {
        return obj.hasInstance(instance);
    }
    /**
     * @see org.mozilla.javascript.Function#call
     */
    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        return ((Function)obj).call(cx,scope,thisObj,args);
    }

    /**
     * Note that if the <code>delegee</code> is <code>null</code>,
     * this method creates a new instance of the Delegator itself
     * rathert than forwarding the call to the
     * <code>delegee</code>. This permits the use of Delegator
     * prototypes.
     *
     * @param cx the current Context for this thread
     * @param scope an enclosing scope of the caller except
     *              when the function is called from a closure.
     * @param args the array of arguments
     * @return the allocated object
     *
     * @see Function#construct(Context, Scriptable, Object[])
     */
    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
    {
        if (obj == null) {
            //this little trick allows us to declare prototype objects for
            //Delegators
            Delegator n = newInstance();
            Scriptable delegee;
            if (args.length == 0) {
                delegee = Undefined.instance;
            } else {
                delegee = ScriptRuntime.toObject(cx, scope, args[0]);
            }
            n.setDelegee(delegee);
            return n;
        }
        else {
            return ((Function)obj).construct(cx,scope,args);
        }
    }
}
