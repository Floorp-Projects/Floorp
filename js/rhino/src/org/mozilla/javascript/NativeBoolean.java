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
 * Mike McCabe
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
 * This class implements the Boolean native object.
 * See ECMA 15.6.
 * @author Norris Boyd
 */
final class NativeBoolean extends IdScriptable {

    static void init(Context cx, Scriptable scope, boolean sealed) {
        NativeBoolean obj = new NativeBoolean(false);
        obj.prototypeFlag = true;
        obj.addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    private NativeBoolean(boolean b) {
        booleanValue = b;
    }

    public String getClassName() {
        return "Boolean";
    }

    public Object getDefaultValue(Class typeHint) {
        // This is actually non-ECMA, but will be proposed
        // as a change in round 2.
        if (typeHint == ScriptRuntime.BooleanClass)
            return wrap_boolean(booleanValue);
        return super.getDefaultValue(typeHint);
    }

    public int methodArity(int methodId) {
        if (prototypeFlag) {
            if (methodId == Id_constructor) return 1;
            if (methodId == Id_toString) return 0;
            if (methodId == Id_valueOf) return 0;
        }
        return super.methodArity(methodId);
    }

    public Object execMethod
        (int methodId, IdFunction f,
         Context cx, Scriptable scope, Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        if (prototypeFlag) {
            if (methodId == Id_constructor) {
                boolean b = ScriptRuntime.toBoolean(args, 0);
                if (thisObj == null) {
                    // new Boolean(val) creates a new boolean object.
                    return new NativeBoolean(b);
                }
                // Boolean(val) converts val to a boolean.
                return wrap_boolean(b);
            } else if (methodId == Id_toString) {
                return realThis(thisObj, f).booleanValue ? "true" : "false";
            } else if (methodId == Id_valueOf) {
                return wrap_boolean(realThis(thisObj, f).booleanValue);
            }
        }

        return super.execMethod(methodId, f, cx, scope, thisObj, args);
    }

    private NativeBoolean realThis(Scriptable thisObj, IdFunction f) {
        while (!(thisObj instanceof NativeBoolean)) {
            thisObj = nextInstanceCheck(thisObj, f, true);
        }
        return (NativeBoolean)thisObj;
    }

    protected String getIdName(int id) {
        if (prototypeFlag) {
            if (id == Id_constructor) return "constructor";
            if (id == Id_toString) return "toString";
            if (id == Id_valueOf) return "valueOf";
        }
        return null;
    }

// #string_id_map#

    protected int mapNameToId(String s) {
        if (!prototypeFlag) { return 0; }
        int id;
// #generated# Last update: 2001-04-23 10:38:18 CEST
        L0: { id = 0; String X = null;
            int s_length = s.length();
            if (s_length==7) { X="valueOf";id=Id_valueOf; }
            else if (s_length==8) { X="toString";id=Id_toString; }
            else if (s_length==11) { X="constructor";id=Id_constructor; }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor          = 1,
        Id_toString             = 2,
        Id_valueOf              = 3,
        MAX_PROTOTYPE_ID        = 3;

// #/string_id_map#

    private boolean booleanValue;

    private boolean prototypeFlag;
}
