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
 * This class implements the Object native object.
 * See ECMA 15.2.
 * @author Norris Boyd
 */
public class NativeObject extends IdScriptableObject
{
    private static final Object OBJECT_TAG = new Object();

    public static void init(Context cx, Scriptable scope, boolean sealed)
    {
        NativeObject obj = new NativeObject();
        obj.exportAsJSClass(MAX_PROTOTYPE_ID, scope, sealed);
    }

    public String getClassName()
    {
        return "Object";
    }

    public String toString()
    {
        return ScriptRuntime.defaultObjectToString(this);
    }

    protected void initPrototypeId(int id)
    {
        String s;
        int arity;
        switch (id) {
          case Id_constructor:    arity=1; s="constructor";    break;
          case Id_toString:       arity=0; s="toString";       break;
          case Id_toLocaleString: arity=0; s="toLocaleString"; break;
          case Id_valueOf:        arity=0; s="valueOf";        break;
          case Id_hasOwnProperty: arity=1; s="hasOwnProperty"; break;
          case Id_propertyIsEnumerable:
            arity=1; s="propertyIsEnumerable"; break;
          case Id_isPrototypeOf:  arity=1; s="isPrototypeOf";  break;
          case Id_toSource:       arity=0; s="toSource";       break;
          default: throw new IllegalArgumentException(String.valueOf(id));
        }
        initPrototypeMethod(OBJECT_TAG, id, s, arity);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (!f.hasTag(OBJECT_TAG)) {
            return super.execMethod(f, cx, scope, thisObj, args);
        }
        int id = f.methodId();
        switch (id) {
          case Id_constructor: {
            if (thisObj != null) {
                // BaseFunction.construct will set up parent, proto
                return f.construct(cx, scope, args);
            }
            if (args.length == 0 || args[0] == null
                || args[0] == Undefined.instance)
            {
                return new NativeObject();
            }
            return ScriptRuntime.toObject(cx, scope, args[0]);
          }

          case Id_toLocaleString: // For now just alias toString
          case Id_toString: {
            if (cx.hasFeature(Context.FEATURE_TO_STRING_AS_SOURCE)) {
                String s = ScriptRuntime.defaultObjectToSource(cx, scope,
                                                               thisObj, args);
                int L = s.length();
                if (L != 0 && s.charAt(0) == '(' && s.charAt(L - 1) == ')') {
                    // Strip () that surrounds toSource
                    s = s.substring(1, L - 1);
                }
                return s;
            }
            return ScriptRuntime.defaultObjectToString(thisObj);
          }

          case Id_valueOf:
            return thisObj;

          case Id_hasOwnProperty: {
            boolean result;
            if (args.length == 0) {
                result = false;
            } else {
                String s = ScriptRuntime.toStringIdOrIndex(cx, args[0]);
                if (s == null) {
                    int index = ScriptRuntime.lastIndexResult(cx);
                    result = thisObj.has(index, thisObj);
                } else {
                    result = thisObj.has(s, thisObj);
                }
            }
            return result ? Boolean.TRUE : Boolean.FALSE;
          }

          case Id_propertyIsEnumerable: {
            boolean result;
            if (args.length == 0) {
                result = false;
            } else {
                String s = ScriptRuntime.toStringIdOrIndex(cx, args[0]);
                if (s == null) {
                    int index = ScriptRuntime.lastIndexResult(cx);
                    result = thisObj.has(index, thisObj);
                    if (result && thisObj instanceof ScriptableObject) {
                        ScriptableObject so = (ScriptableObject)thisObj;
                        int attrs = so.getAttributes(index);
                        result = ((attrs & ScriptableObject.DONTENUM) == 0);
                    }
                } else {
                    result = thisObj.has(s, thisObj);
                    if (result && thisObj instanceof ScriptableObject) {
                        ScriptableObject so = (ScriptableObject)thisObj;
                        int attrs = so.getAttributes(s);
                        result = ((attrs & ScriptableObject.DONTENUM) == 0);
                    }
                }
            }
            return result ? Boolean.TRUE : Boolean.FALSE;
          }

          case Id_isPrototypeOf: {
            if (args.length != 0 && args[0] instanceof Scriptable) {
                Scriptable v = (Scriptable) args[0];
                do {
                    v = v.getPrototype();
                    if (v == thisObj)
                        return Boolean.TRUE;
                } while (v != null);
            }
            return Boolean.FALSE;
          }

          case Id_toSource:
            return ScriptRuntime.defaultObjectToSource(cx, scope, thisObj,
                                                       args);
          default: throw new IllegalArgumentException(String.valueOf(id));
        }
    }

// #string_id_map#

    protected int findPrototypeId(String s)
    {
        int id;
// #generated# Last update: 2003-11-11 01:51:40 CET
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 7: X="valueOf";id=Id_valueOf; break L;
            case 8: c=s.charAt(3);
                if (c=='o') { X="toSource";id=Id_toSource; }
                else if (c=='t') { X="toString";id=Id_toString; }
                break L;
            case 11: X="constructor";id=Id_constructor; break L;
            case 13: X="isPrototypeOf";id=Id_isPrototypeOf; break L;
            case 14: c=s.charAt(0);
                if (c=='h') { X="hasOwnProperty";id=Id_hasOwnProperty; }
                else if (c=='t') { X="toLocaleString";id=Id_toLocaleString; }
                break L;
            case 20: X="propertyIsEnumerable";id=Id_propertyIsEnumerable; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor           = 1,
        Id_toString              = 2,
        Id_toLocaleString        = 3,
        Id_valueOf               = 4,
        Id_hasOwnProperty        = 5,
        Id_propertyIsEnumerable  = 6,
        Id_isPrototypeOf         = 7,
        Id_toSource              = 8,
        MAX_PROTOTYPE_ID         = 8;

// #/string_id_map#
}
