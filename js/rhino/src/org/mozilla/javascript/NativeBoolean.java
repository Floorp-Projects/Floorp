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
final class NativeBoolean extends IdScriptable
{
    private static final Object BOOLEAN_TAG = new Object();

    static void init(Context cx, Scriptable scope, boolean sealed)
    {
        NativeBoolean obj = new NativeBoolean(false);
        obj.exportAsJSClass(MAX_PROTOTYPE_ID, scope, sealed);
    }

    private NativeBoolean(boolean b)
    {
        booleanValue = b;
    }

    public String getClassName()
    {
        return "Boolean";
    }

    public Object getDefaultValue(Class typeHint) {
        // This is actually non-ECMA, but will be proposed
        // as a change in round 2.
        if (typeHint == ScriptRuntime.BooleanClass)
            return wrap_boolean(booleanValue);
        return super.getDefaultValue(typeHint);
    }

    protected void initPrototypeId(int id)
    {
        String s;
        int arity;
        switch (id) {
          case Id_constructor: arity=1; s="constructor"; break;
          case Id_toString:    arity=0; s="toString";    break;
          case Id_toSource:    arity=0; s="toSource";    break;
          case Id_valueOf:     arity=0; s="valueOf";     break;
          default: throw new IllegalArgumentException(String.valueOf(id));
        }
        initPrototypeMethod(BOOLEAN_TAG, id, s, arity);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (!f.hasTag(BOOLEAN_TAG)) {
            return super.execMethod(f, cx, scope, thisObj, args);
        }
        int id = f.methodId();
        switch (id) {
          case Id_constructor: {
            boolean b = ScriptRuntime.toBoolean(args, 0);
            if (thisObj == null) {
                // new Boolean(val) creates a new boolean object.
                return new NativeBoolean(b);
            }
            // Boolean(val) converts val to a boolean.
            return wrap_boolean(b);
          }

          case Id_toString:
            return realThisBoolean(thisObj, f) ? "true" : "false";

          case Id_toSource:
            if (realThisBoolean(thisObj, f))
                return "(new Boolean(true))";
            else
                return "(new Boolean(false))";

          case Id_valueOf:
            return wrap_boolean(realThisBoolean(thisObj, f));
        }
        throw new IllegalArgumentException(String.valueOf(id));
    }

    private static boolean realThisBoolean(Scriptable thisObj, IdFunction f)
    {
        if (!(thisObj instanceof NativeBoolean))
            throw incompatibleCallError(f);
        return ((NativeBoolean)thisObj).booleanValue;
    }

// #string_id_map#

    protected int findPrototypeId(String s)
    {
        int id;
// #generated# Last update: 2004-03-17 13:28:00 CET
        L0: { id = 0; String X = null; int c;
            int s_length = s.length();
            if (s_length==7) { X="valueOf";id=Id_valueOf; }
            else if (s_length==8) {
                c=s.charAt(3);
                if (c=='o') { X="toSource";id=Id_toSource; }
                else if (c=='t') { X="toString";id=Id_toString; }
            }
            else if (s_length==11) { X="constructor";id=Id_constructor; }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor          = 1,
        Id_toString             = 2,
        Id_toSource             = 3,
        Id_valueOf              = 4,
        MAX_PROTOTYPE_ID        = 4;

// #/string_id_map#

    private boolean booleanValue;
}
