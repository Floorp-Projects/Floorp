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
public class NativeObject extends IdScriptable
{
    public static void init(Context cx, Scriptable scope, boolean sealed)
    {
        new NativeObjectPrototype(cx, scope, sealed);
    }

    public String getClassName()
    {
        return "Object";
    }

    public String toString()
    {
        return ScriptRuntime.defaultObjectToString(this);
    }

    protected int mapNameToId(String s) { return 0; }

    protected String getIdName(int id) { return null; }

}

final class NativeObjectPrototype extends NativeObject
{
    NativeObjectPrototype(Context cx, Scriptable scope, boolean sealed)
    {
        addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    public int methodArity(IdFunction f)
    {
        switch (f.methodId) {
            case Id_constructor:           return 1;
            case Id_toString:              return 0;
            case Id_toLocaleString:        return 0;
            case Id_valueOf:               return 0;
            case Id_hasOwnProperty:        return 1;
            case Id_propertyIsEnumerable:  return 1;
            case Id_isPrototypeOf:         return 1;
            case Id_toSource:              return 0;
        }
        return super.methodArity(f);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        switch (f.methodId) {
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
                    String s = toSource(cx, scope, thisObj, args);
                    int L = s.length();
                    if (L != 0 && s.charAt(0) == '(' && s.charAt(L - 1) == ')')
                    {
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
                if (args.length != 0) {
                    String property = ScriptRuntime.toString(args[0]);
                    if (thisObj.has(property, thisObj))
                        return Boolean.TRUE;
                }
                return Boolean.FALSE;
            }

            case Id_propertyIsEnumerable: {
                if (args.length != 0) {
                    String name = ScriptRuntime.toString(args[0]);
                    if (thisObj.has(name, thisObj)) {
                        if (thisObj instanceof ScriptableObject) {
                            ScriptableObject so = (ScriptableObject)thisObj;
                            int a = so.getAttributes(name);
                            if ((a & ScriptableObject.DONTENUM) == 0) {
                                return Boolean.TRUE;
                            }
                        }
                    }
                }
                return Boolean.FALSE;
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
                return toSource(cx, scope, thisObj, args);
        }
        return super.execMethod(f, cx, scope, thisObj, args);
    }

    private static String toSource(Context cx, Scriptable scope,
                                   Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        boolean toplevel, iterating;
        if (cx.iterating == null) {
            toplevel = true;
            iterating = false;
            cx.iterating = new ObjToIntMap(31);
        } else {
            toplevel = false;
            iterating = cx.iterating.has(thisObj);
        }

        StringBuffer result = new StringBuffer(128);
        if (toplevel) {
            result.append("(");
        }
        result.append('{');

        // Make sure cx.iterating is set to null when done
        // so we don't leak memory
        try {
            if (!iterating) {
                cx.iterating.intern(thisObj); // stop recursion.
                Object[] ids = thisObj.getIds();
                for(int i=0; i < ids.length; i++) {
                    if (i > 0)
                        result.append(", ");
                    Object id = ids[i];
                    Object value;
                    if (id instanceof Integer) {
                        int intId = ((Integer)id).intValue();
                        value = thisObj.get(intId, thisObj);
                        result.append(intId);
                    } else {
                        String strId = (String)id;
                        value = thisObj.get(strId, thisObj);
                        if (ScriptRuntime.isValidIdentifierName(strId)) {
                            result.append(strId);
                        } else {
                            result.append('\'');
                            result.append(
                                ScriptRuntime.escapeString(strId, '\''));
                            result.append('\'');
                        }
                    }
                    result.append(':');
                    result.append(ScriptRuntime.uneval(cx, scope, value));
                }
            }
        } finally {
            if (toplevel) {
                cx.iterating = null;
            }
        }

        result.append('}');
        if (toplevel) {
            result.append(')');
        }
        return result.toString();
    }

    protected String getIdName(int id)
    {
        switch (id) {
            case Id_constructor:          return "constructor";
            case Id_toString:             return "toString";
            case Id_toLocaleString:       return "toLocaleString";
            case Id_valueOf:              return "valueOf";
            case Id_hasOwnProperty:       return "hasOwnProperty";
            case Id_propertyIsEnumerable: return "propertyIsEnumerable";
            case Id_isPrototypeOf:        return "isPrototypeOf";
            case Id_toSource:             return "toSource";
        }
        return null;
    }

// #string_id_map#

    protected int mapNameToId(String s)
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
