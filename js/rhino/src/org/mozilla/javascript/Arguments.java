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
 * This class implements the "arguments" object.
 *
 * See ECMA 10.1.8
 *
 * @see org.mozilla.javascript.NativeCall
 * @author Norris Boyd
 */
class Arguments extends ScriptableObject {

    public Arguments(NativeCall activation) {
        this.activation = activation;

        Scriptable parent = activation.getParentScope();
        setParentScope(parent);
        setPrototype(ScriptableObject.getObjectPrototype(parent));

        args = activation.getOriginalArguments();
        int length = args.length;
        Object callee = activation.funObj;

        defineProperty("length", new Integer(length),
                       ScriptableObject.DONTENUM);
        defineProperty("callee", callee, ScriptableObject.DONTENUM);

        hasCaller = (activation.funObj.version <= Context.VERSION_1_3 &&
                     activation.funObj.version != Context.VERSION_DEFAULT);
    }

    public String getClassName() {
        return "Arguments";
    }

    public boolean has(String name, Scriptable start) {
        return (hasCaller && name.equals("caller")) || super.has(name, start);
    }

    public boolean has(int index, Scriptable start) {
        Object[] args = activation.getOriginalArguments();
        return (0 <= index && index < args.length) || super.has(index, start);
    }

    public Object get(String name, Scriptable start) {
        if (hasCaller && name.equals("caller")) {
            NativeCall caller = activation.caller;
            if (caller == null || caller.originalArgs == null)
                return null;
            return caller.get("arguments", caller);
        }
        return super.get(name, start);
    }

    public Object get(int index, Scriptable start) {
        if (0 <= index && index < args.length) {
            NativeFunction f = activation.funObj;
            if (index < f.argCount)
                return activation.get(f.argNames[index], activation);
            return args[index];
        }
        return super.get(index, start);
    }

    public void put(String name, Scriptable start, Object value) {
        if (name.equals("caller")) {
            // Set "hasCaller" to false so that we won't look up a
            // computed value.
            hasCaller = false;
        }
        super.put(name, start, value);
    }

    public void put(int index, Scriptable start, Object value) {
        if (0 <= index && index < args.length) {
            NativeFunction f = activation.funObj;
            if (index < f.argCount)
                activation.put(f.argNames[index], activation, value);
            else
                args[index] = value;
            return;
        }
        super.put(index, start, value);
    }

    public void delete(String name) {
        if (name.equals("caller"))
            hasCaller = false;
        super.delete(name);
    }

    public void delete(int index) {
        if (0 <= index && index < args.length) {
            NativeFunction f = activation.funObj;
            if (index < f.argCount)
                activation.delete(f.argNames[index]);
            else
                args[index] = Undefined.instance;
        }
    }

    private NativeCall activation;
    private Object[] args;
    private boolean hasCaller;
}
