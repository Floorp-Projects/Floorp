/*
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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Roger Lawrence
 * Hannes Wallnoefer
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


package org.mozilla.javascript.optimizer;

import org.mozilla.javascript.*;

public final class OptRuntime extends ScriptRuntime {

    /**
     * No instances should be created.
     */
    private OptRuntime() {
    }

    public static Object getElem(Object obj, double dblIndex, Scriptable scope)
    {
        int index = (int) dblIndex;
        Scriptable start = obj instanceof Scriptable
                           ? (Scriptable) obj
                           : toObject(scope, obj);
        Scriptable m = start;
        if (((double) index) != dblIndex) {
            String s = toString(dblIndex);
            while (m != null) {
                Object result = m.get(s, start);
                if (result != Scriptable.NOT_FOUND)
                    return result;
                m = m.getPrototype();
            }
        } else {
            while (m != null) {
                Object result = m.get(index, start);
                if (result != Scriptable.NOT_FOUND)
                    return result;
                m = m.getPrototype();
            }
        }
        return Undefined.instance;
    }

    public static Object setElem(Object obj, double dblIndex, Object value,
                                 Scriptable scope)
    {
        int index = (int) dblIndex;
        Scriptable start = obj instanceof Scriptable
                     ? (Scriptable) obj
                     : toObject(scope, obj);
        Scriptable m = start;
        if (((double) index) != dblIndex) {
            String s = toString(dblIndex);
            do {
                if (m.has(s, start)) {
                    m.put(s, start, value);
                    return value;
                }
                m = m.getPrototype();
            } while (m != null);
            start.put(s, start, value);
       } else {
            do {
                if (m.has(index, start)) {
                    m.put(index, start, value);
                    return value;
                }
                m = m.getPrototype();
            } while (m != null);
            start.put(index, start, value);
        }
        return value;
    }

    public static Object add(Object val1, double val2) {
        if (val1 instanceof Scriptable)
            val1 = ((Scriptable) val1).getDefaultValue(null);
        if (!(val1 instanceof String))
            return new Double(toNumber(val1) + val2);
        return toString(val1) + toString(val2);
    }

    public static Object add(double val1, Object val2) {
        if (val2 instanceof Scriptable)
            val2 = ((Scriptable) val2).getDefaultValue(null);
        if (!(val2 instanceof String))
            return new Double(toNumber(val2) + val1);
        return toString(val1) + toString(val2);
    }

    public static boolean neq(Object x, Object y) {
        return !eq(x, y);
    }

    public static boolean shallowNeq(Object x, Object y) {
        return !shallowEq(x, y);
    }

    public static Boolean cmp_LTB(double d1, Object val2) {
        if (cmp_LT(d1, val2) == 1)
            return Boolean.TRUE;
        else
            return Boolean.FALSE;
    }

    public static int cmp_LT(double d1, Object val2) {
        if (val2 instanceof Scriptable)
            val2 = ((Scriptable) val2).getDefaultValue(NumberClass);
        if (!(val2 instanceof String)) {
            if (d1 != d1)
                return 0;
            double d2 = toNumber(val2);
            if (d2 != d2)
                return 0;
            return d1 < d2 ? 1 : 0;
        }
        return toString(d1).compareTo(toString(val2)) < 0 ? 1 : 0;
    }

    public static Boolean cmp_LTB(Object val1, double d2) {
        if (cmp_LT(val1, d2) == 1)
            return Boolean.TRUE;
        else
            return Boolean.FALSE;
    }

    public static int cmp_LT(Object val1, double d2) {
        if (val1 instanceof Scriptable)
            val1 = ((Scriptable) val1).getDefaultValue(NumberClass);
        if (!(val1 instanceof String)) {
            double d1 = toNumber(val1);
            if (d1 != d1)
                return 0;
            if (d2 != d2)
                return 0;
            return d1 < d2 ? 1 : 0;
        }
        return toString(val1).compareTo(toString(d2)) < 0 ? 1 : 0;
    }

    public static Boolean cmp_LEB(double d1, Object val2) {
        if (cmp_LE(d1, val2) == 1)
            return Boolean.TRUE;
        else
            return Boolean.FALSE;
    }

    public static int cmp_LE(double d1, Object val2) {
        if (val2 instanceof Scriptable)
            val2 = ((Scriptable) val2).getDefaultValue(NumberClass);
        if (!(val2 instanceof String)) {
            if (d1 != d1)
                return 0;
            double d2 = toNumber(val2);
            if (d2 != d2)
                return 0;
            return d1 <= d2 ? 1 : 0;
        }
        return toString(d1).compareTo(toString(val2)) <= 0 ? 1 : 0;
    }

    public static Boolean cmp_LEB(Object val1, double d2) {
        if (cmp_LE(val1, d2) == 1)
            return Boolean.TRUE;
        else
            return Boolean.FALSE;
    }

    public static int cmp_LE(Object val1, double d2) {
        if (val1 instanceof Scriptable)
            val1 = ((Scriptable) val1).getDefaultValue(NumberClass);
        if (!(val1 instanceof String)) {
            double d1 = toNumber(val1);
            if (d1 != d1)
                return 0;
            if (d2 != d2)
                return 0;
            return d1 <= d2 ? 1 : 0;
        }
        return toString(val1).compareTo(toString(d2)) <= 0 ? 1 : 0;
    }

    public static int cmp(Object val1, Object val2) {
        if (val1 instanceof Scriptable)
            val1 = ((Scriptable) val1).getDefaultValue(NumberClass);
        if (val2 instanceof Scriptable)
            val2 = ((Scriptable) val2).getDefaultValue(NumberClass);
        if (!(val1 instanceof String) || !(val2 instanceof String)) {
            double d1 = toNumber(val1);
            if (d1 != d1)
                return -1;
            double d2 = toNumber(val2);
            if (d2 != d2)
                return -1;
            return d1 < d2 ? 1 : 0;
        }
        return toString(val1).compareTo(toString(val2)) < 0 ? 1 : 0;
    }

    public static Object callSimple(Context cx, String id, Scriptable scope,
                                    Object[] args)
        throws JavaScriptException
    {
        Scriptable obj = scope;
        Object prop = null;
        Scriptable thisArg = null;
 search:
        while (obj != null) {
            Scriptable m = obj;
            do {
                prop = m.get(id, obj);
                if (prop != Scriptable.NOT_FOUND) {
                    thisArg = obj;
                    break search;
                }
                m = m.getPrototype();
            } while (m != null);
            obj = obj.getParentScope();
        }
        if ((prop == null) || (prop == Scriptable.NOT_FOUND)) {
            Object[] errorArgs = { id };
            throw NativeGlobal.constructError(
                        cx, "ReferenceError",
                        ScriptRuntime.getMessage("msg.is.not.defined", errorArgs),
                        scope);
        }

        while (thisArg instanceof NativeWith)
            thisArg = thisArg.getPrototype();
        if (thisArg instanceof NativeCall)
            thisArg = ScriptableObject.getTopLevelScope(thisArg);

        Function function;
        try {
            function = (Function) prop;
        }
        catch (ClassCastException e) {
            Object[] errorArgs = { toString(prop)  };
            throw cx.reportRuntimeError(
                getMessage("msg.isnt.function", errorArgs));
        }

        return function.call(cx, scope, thisArg, args);
    }

    public static Object thisGet(Scriptable thisObj, String id,
                                 Scriptable scope)
    {
        if (thisObj == null) {
            throw Context.reportRuntimeError(
                getMessage("msg.null.to.object", null));
        }

        Object result = thisObj.get(id, thisObj);
        if (result != Scriptable.NOT_FOUND)
            return result;

        Scriptable m = thisObj.getPrototype();
        while (m != null) {
            result = m.get(id, thisObj);
            if (result != Scriptable.NOT_FOUND)
                return result;

            m = m.getPrototype();
        }
        return Undefined.instance;
    }

    public static Object[] padStart(Object[] currentArgs, int count) {
        Object[] result = new Object[currentArgs.length + count];
        System.arraycopy(currentArgs, 0, result, count, currentArgs.length);
        return result;
    }

    public static void setupFunction(NativeFunction fn,
                                     Scriptable scope,
                                     int functionType)
    {
        ScriptRuntime.initFunction(scope, fn);
        ScriptRuntime.putFunction(scope, fn.getFunctionName(), fn,
                                  functionType, false);
    }
}
