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
 * This class implements the object lookup required for the
 * <code>with</code> statement.
 * It simply delegates every action to its prototype except
 * for operations on its parent.
 */
public class NativeWith implements Scriptable, IdFunctionCall {

    static void init(Context cx, Scriptable scope, boolean sealed) {
        NativeWith obj = new NativeWith();

        obj.setParentScope(scope);
        obj.setPrototype(ScriptableObject.getObjectPrototype(scope));

        IdFunctionObject ctor = new IdFunctionObject(obj, FTAG, Id_constructor,
                                         "With", 0, scope);
        ctor.markAsConstructor(obj);
        if (sealed) {
            ctor.sealObject();
        }
        ctor.exportAsScopeProperty();
    }

    private NativeWith() {
    }

    protected NativeWith(Scriptable parent, Scriptable prototype) {
        this.parent = parent;
        this.prototype = prototype;
    }

    public String getClassName() {
        return "With";
    }

    public boolean has(String id, Scriptable start)
    {
        return prototype.has(id, prototype);
    }

    public boolean has(int index, Scriptable start)
    {
        return prototype.has(index, prototype);
    }

    public Object get(String id, Scriptable start)
    {
        if (start == this)
            start = prototype;
        return prototype.get(id, start);
    }

    public Object get(int index, Scriptable start)
    {
        if (start == this)
            start = prototype;
        return prototype.get(index, start);
    }

    public void put(String id, Scriptable start, Object value)
    {
        if (start == this)
            start = prototype;
        prototype.put(id, start, value);
    }

    public void put(int index, Scriptable start, Object value)
    {
        if (start == this)
            start = prototype;
        prototype.put(index, start, value);
    }

    public void delete(String id)
    {
        prototype.delete(id);
    }

    public void delete(int index)
    {
        prototype.delete(index);
    }

    public Scriptable getPrototype() {
        return prototype;
    }

    public void setPrototype(Scriptable prototype) {
        this.prototype = prototype;
    }

    public Scriptable getParentScope() {
        return parent;
    }

    public void setParentScope(Scriptable parent) {
        this.parent = parent;
    }

    public Object[] getIds() {
        return prototype.getIds();
    }

    public Object getDefaultValue(Class typeHint) {
        return prototype.getDefaultValue(typeHint);
    }

    public boolean hasInstance(Scriptable value) {
        return prototype.hasInstance(value);
    }

    /**
     * Must return null to continue looping or the final collection result.
     */
    public Object updateDotQuery(boolean value)
    {
        // NativeWith itself does not support it
        throw new IllegalStateException();
    }

    public Object execIdCall(IdFunctionObject f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (f.hasTag(FTAG)) {
            if (f.methodId() == Id_constructor) {
                throw Context.reportRuntimeError1("msg.cant.call.indirect", "With");
            }
        }
        throw f.unknown();
    }

    static boolean isWithFunction(Object functionObj)
    {
        if (functionObj instanceof IdFunctionObject) {
            IdFunctionObject f = (IdFunctionObject)functionObj;
            return f.hasTag(FTAG) && f.methodId() == Id_constructor;
        }
        return false;
    }

    static Object newWithSpecial(Context cx, Scriptable scope, Object[] args)
    {
        ScriptRuntime.checkDeprecated(cx, "With");
        scope = ScriptableObject.getTopLevelScope(scope);
        NativeWith thisObj = new NativeWith();
        thisObj.setPrototype(args.length == 0
                             ? ScriptableObject.getClassPrototype(scope,
                                                                  "Object")
                             : ScriptRuntime.toObject(cx, scope, args[0]));
        thisObj.setParentScope(scope);
        return thisObj;
    }

    private static final Object FTAG = new Object();

    private static final int
        Id_constructor = 1;

    protected Scriptable prototype;
    protected Scriptable parent;
}
