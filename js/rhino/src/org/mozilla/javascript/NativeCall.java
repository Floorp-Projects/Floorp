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
 * This class implements the activation object.
 *
 * See ECMA 10.1.6
 *
 * @see org.mozilla.javascript.Arguments
 * @author Norris Boyd
 */
public final class NativeCall extends IdScriptable
{
    private static final Object CALL_TAG = new Object();

    static void init(Context cx, Scriptable scope, boolean sealed)
    {
        NativeCall obj = new NativeCall();
        obj.exportAsJSClass(MAX_PROTOTYPE_ID, scope, sealed);
    }

    NativeCall() { }

    NativeCall(Context cx, Scriptable scope, NativeFunction funObj,
               Scriptable thisObj, Object[] args)
    {
        this.funObj = funObj;
        this.thisObj = thisObj;

        setParentScope(scope);
        // leave prototype null

        // save current activation
        this.caller = cx.currentActivation;
        cx.currentActivation = this;

        this.originalArgs = (args == null) ? ScriptRuntime.emptyArgs : args;

        // initialize values of arguments
        String[] argNames = funObj.argNames;
        if (argNames != null) {
            for (int i=0; i < funObj.argCount; i++) {
                Object val = i < args.length ? args[i]
                                             : Undefined.instance;
                super.put(argNames[i], this, val);
            }
        }

        // initialize "arguments" property but only if it was not overriden by
        // the parameter with the same name
        if (!super.has("arguments", this)) {
            super.put("arguments", this, new Arguments(this));
        }

        if (argNames != null) {
            for (int i = funObj.argCount; i != argNames.length; i++) {
                String name = argNames[i];
                if (!super.has(name, this)) {
                    super.put(name, this, Undefined.instance);
                }
            }
        }
    }

    public String getClassName()
    {
        return "Call";
    }

    NativeCall getActivation(Function f)
    {
        NativeCall x = this;
        do {
            if (x.funObj == f)
                return x;
            x = x.caller;
        } while (x != null);
        return null;
    }

    public Function getFunctionObject()
    {
        return funObj;
    }

    public Object[] getOriginalArguments()
    {
        return originalArgs;
    }

    public NativeCall getCaller()
    {
        return caller;
    }

    public Scriptable getThisObj()
    {
        return thisObj;
    }

    protected void initPrototypeId(int id)
    {
        String s;
        int arity;
        if (id == Id_constructor) {
            arity=1; s="constructor";
        } else {
          throw new IllegalArgumentException(String.valueOf(id));
        }
        initPrototypeMethod(CALL_TAG, id, s, arity);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (!f.hasTag(CALL_TAG)) {
            return super.execMethod(f, cx, scope, thisObj, args);
        }
        int id = f.methodId();
        if (id == Id_constructor) {
            if (thisObj != null) {
                throw Context.reportRuntimeError1("msg.only.from.new", "Call");
            }
            ScriptRuntime.checkDeprecated(cx, "Call");
            NativeCall result = new NativeCall();
            result.setPrototype(getObjectPrototype(scope));
            return result;
        }
        throw new IllegalArgumentException(String.valueOf(id));
    }

    protected int findPrototypeId(String s)
    {
        return s.equals("constructor") ? Id_constructor : 0;
    }

    private static final int
        Id_constructor   = 1,
        MAX_PROTOTYPE_ID = 1;


    NativeCall caller;
    NativeFunction funObj;
    Scriptable thisObj;
    private Object[] originalArgs;
}

