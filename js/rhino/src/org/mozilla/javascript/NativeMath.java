/* -*- Mode: java; tab-width: 4; indent-tabs-mode: 1; c-basic-offset: 4 -*-
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
 * This class implements the Math native object.
 * See ECMA 15.8.
 * @author Norris Boyd
 */

public class NativeMath extends IdScriptable
{
    public String getClassName() { return "Math"; }

    public void scopeInit(Context cx, Scriptable scope, boolean sealed) {
        activateIdMap(cx, sealed);
        setPrototype(getObjectPrototype(scope));
        setParentScope(scope);
        if (sealed) {
            sealObject();
        }
        ScriptableObject.defineProperty
            (scope, "Math", this, ScriptableObject.DONTENUM);
    }

    protected int getIdDefaultAttributes(int id) {
        if (id > LAST_METHOD_ID) {
            return DONTENUM | READONLY | PERMANENT;
        }
        return super.getIdDefaultAttributes(id);
    }

    protected Object getIdValue(int id, Scriptable start) {
        if (id > LAST_METHOD_ID) {
            return cacheIdValue(id, getField(id));
        }
        return super.getIdValue(id, start);
    }

    private Double getField(int fieldId) {
        switch (fieldId) {
            case Id_E:       return E;
            case Id_PI:      return PI;
            case Id_LN10:    return LN10;
            case Id_LN2:     return LN2;
            case Id_LOG2E:   return LOG2E;
            case Id_LOG10E:  return LOG10E;
            case Id_SQRT1_2: return SQRT1_2;
            case Id_SQRT2:   return SQRT2;
        }
        return null;
    }

    public int methodArity(int methodId) {
        switch (methodId) {
            case Id_abs:      return 1;
            case Id_acos:     return 1;
            case Id_asin:     return 1;
            case Id_atan:     return 1;
            case Id_atan2:    return 2;
            case Id_ceil:     return 1;
            case Id_cos:      return 1;
            case Id_exp:      return 1;
            case Id_floor:    return 1;
            case Id_log:      return 1;
            case Id_max:      return 2;
            case Id_min:      return 2;
            case Id_pow:      return 2;
            case Id_random:   return 0;
            case Id_round:    return 1;
            case Id_sin:      return 1;
            case Id_sqrt:     return 1;
            case Id_tan:      return 1;
        }
        return super.methodArity(methodId);
    }

    public Object execMethod
        (int methodId, IdFunction f,
         Context cx, Scriptable scope, Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        switch (methodId) {
            case Id_abs: return wrap_double
                (js_abs(ScriptRuntime.toNumber(args, 0)));

            case Id_acos: return wrap_double
                (js_acos(ScriptRuntime.toNumber(args, 0)));

            case Id_asin: return wrap_double
                (js_asin(ScriptRuntime.toNumber(args, 0)));

            case Id_atan: return wrap_double
                (js_atan(ScriptRuntime.toNumber(args, 0)));

            case Id_atan2: return wrap_double
                (js_atan2(ScriptRuntime.toNumber(args, 0),
                          ScriptRuntime.toNumber(args, 1)));

            case Id_ceil: return wrap_double
                (js_ceil(ScriptRuntime.toNumber(args, 0)));

            case Id_cos: return wrap_double
                (js_cos(ScriptRuntime.toNumber(args, 0)));

            case Id_exp: return wrap_double
                (js_exp(ScriptRuntime.toNumber(args, 0)));

            case Id_floor: return wrap_double
                (js_floor(ScriptRuntime.toNumber(args, 0)));

            case Id_log: return wrap_double
                (js_log(ScriptRuntime.toNumber(args, 0)));

            case Id_max: return wrap_double
                (js_max(args));

            case Id_min: return wrap_double
                (js_min(args));

            case Id_pow: return wrap_double
                (js_pow(ScriptRuntime.toNumber(args, 0),
                        ScriptRuntime.toNumber(args, 1)));

            case Id_random: return wrap_double
                (js_random());

            case Id_round: return wrap_double
                (js_round(ScriptRuntime.toNumber(args, 0)));

            case Id_sin: return wrap_double
                (js_sin(ScriptRuntime.toNumber(args, 0)));

            case Id_sqrt: return wrap_double
                (js_sqrt(ScriptRuntime.toNumber(args, 0)));

            case Id_tan: return wrap_double
                (js_tan(ScriptRuntime.toNumber(args, 0)));
        }
        return super.execMethod(methodId, f, cx, scope, thisObj, args);
    }

    private double js_abs(double x) {
        // abs(-0.0) should be 0.0, but -0.0 < 0.0 == false
        return (x == 0.0) ? 0.0 : (x < 0.0) ? -x : x;
    }

    private double js_acos(double x) {
        return (x == x && -1.0 <= x && x <= 1.0) ? Math.acos(x) : Double.NaN;
    }

    private double js_asin(double x) {
        return (x == x && -1.0 <= x && x <= 1.0) ? Math.asin(x) : Double.NaN;
    }

    private double js_atan(double x) { return Math.atan(x); }

    private double js_atan2(double x, double y) { return Math.atan2(x, y); }

    private double js_ceil(double x) { return Math.ceil(x); }

    private double js_cos(double x) { return Math.cos(x); }

    private double js_exp(double x) {
        return (x == Double.POSITIVE_INFINITY) ? x
            : (x == Double.NEGATIVE_INFINITY) ? 0.0
            : Math.exp(x);
    }

    private double js_floor(double x) { return Math.floor(x); }

    private double js_log(double x) {
        // Java's log(<0) = -Infinity; we need NaN
        return (x < 0) ? Double.NaN : Math.log(x);
    }

    private double js_max(Object[] args) {
        double result = Double.NEGATIVE_INFINITY;
        if (args.length == 0)
            return result;
        for (int i = 0; i < args.length; i++) {
            double d = ScriptRuntime.toNumber(args[i]);
            if (d != d) return d;
            // if (result < d) result = d; does not work due to -0.0 >= +0.0
            result = Math.max(result, d);
        }
        return result;
    }

    private double js_min(Object[] args) {
        double result = Double.POSITIVE_INFINITY;
        if (args.length == 0)
            return result;
        for (int i = 0; i < args.length; i++) {
            double d = ScriptRuntime.toNumber(args[i]);
            if (d != d) return d;
            // if (result > d) result = d; does not work due to -0.0 >= +0.0
            result = Math.min(result, d);
        }
        return result;
    }

    private double js_pow(double x, double y) {
        if (y == 0) return 1.0;   // Java's pow(NaN, 0) = NaN; we need 1
        if ((x == 0) && (y < 0)) {
            if (1 / x > 0) {
                // x is +0, Java is -oo, we need +oo
                return Double.POSITIVE_INFINITY;
            }
            /* if x is -0 and y is an odd integer, -oo */
            int y_int = (int)y;
            if (y_int == y && (y_int & 0x1) != 0)
                return Double.NEGATIVE_INFINITY;
            return Double.POSITIVE_INFINITY;
        }
        return Math.pow(x, y);
    }

    private double js_random() { return Math.random(); }

    private double js_round(double d) {
        if (d != d)
            return d;   // NaN
        if (d == Double.POSITIVE_INFINITY || d == Double.NEGATIVE_INFINITY)
            return d;
        long l = Math.round(d);
        if (l == 0) {
            // We must propagate the sign of d into the result
            if (d < 0.0)
                return ScriptRuntime.negativeZero;
            return d == 0.0 ? d : 0.0;
        }
        return (double) l;
    }

    private double js_sin(double x) { return Math.sin(x); }

    private double js_sqrt(double x) { return Math.sqrt(x); }

    private double js_tan(double x) { return Math.tan(x); }

    protected int getMaximumId() { return MAX_ID; }

    protected String getIdName(int id) {
        switch (id) {
            case Id_abs:      return "abs";
            case Id_acos:     return "acos";
            case Id_asin:     return "asin";
            case Id_atan:     return "atan";
            case Id_atan2:    return "atan2";
            case Id_ceil:     return "ceil";
            case Id_cos:      return "cos";
            case Id_exp:      return "exp";
            case Id_floor:    return "floor";
            case Id_log:      return "log";
            case Id_max:      return "max";
            case Id_min:      return "min";
            case Id_pow:      return "pow";
            case Id_random:   return "random";
            case Id_round:    return "round";
            case Id_sin:      return "sin";
            case Id_sqrt:     return "sqrt";
            case Id_tan:      return "tan";

            case Id_E:        return "E";
            case Id_PI:       return "PI";
            case Id_LN10:     return "LN10";
            case Id_LN2:      return "LN2";
            case Id_LOG2E:    return "LOG2E";
            case Id_LOG10E:   return "LOG10E";
            case Id_SQRT1_2:  return "SQRT1_2";
            case Id_SQRT2:    return "SQRT2";
        }
        return null;
    }

// #string_id_map#

    protected int mapNameToId(String s) {
        int id;
// #generated# Last update: 2001-03-23 13:50:14 GMT+01:00
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 1: if (s.charAt(0)=='E') {id=Id_E; break L0;} break L;
            case 2: if (s.charAt(0)=='P' && s.charAt(1)=='I') {id=Id_PI; break L0;} break L;
            case 3: switch (s.charAt(0)) {
                case 'L': if (s.charAt(2)=='2' && s.charAt(1)=='N') {id=Id_LN2; break L0;} break L;
                case 'a': if (s.charAt(2)=='s' && s.charAt(1)=='b') {id=Id_abs; break L0;} break L;
                case 'c': if (s.charAt(2)=='s' && s.charAt(1)=='o') {id=Id_cos; break L0;} break L;
                case 'e': if (s.charAt(2)=='p' && s.charAt(1)=='x') {id=Id_exp; break L0;} break L;
                case 'l': if (s.charAt(2)=='g' && s.charAt(1)=='o') {id=Id_log; break L0;} break L;
                case 'm': c=s.charAt(2);
                    if (c=='n') { if (s.charAt(1)=='i') {id=Id_min; break L0;} }
                    else if (c=='x') { if (s.charAt(1)=='a') {id=Id_max; break L0;} }
                    break L;
                case 'p': if (s.charAt(2)=='w' && s.charAt(1)=='o') {id=Id_pow; break L0;} break L;
                case 's': if (s.charAt(2)=='n' && s.charAt(1)=='i') {id=Id_sin; break L0;} break L;
                case 't': if (s.charAt(2)=='n' && s.charAt(1)=='a') {id=Id_tan; break L0;} break L;
                } break L;
            case 4: switch (s.charAt(1)) {
                case 'N': X="LN10";id=Id_LN10; break L;
                case 'c': X="acos";id=Id_acos; break L;
                case 'e': X="ceil";id=Id_ceil; break L;
                case 'q': X="sqrt";id=Id_sqrt; break L;
                case 's': X="asin";id=Id_asin; break L;
                case 't': X="atan";id=Id_atan; break L;
                } break L;
            case 5: switch (s.charAt(0)) {
                case 'L': X="LOG2E";id=Id_LOG2E; break L;
                case 'S': X="SQRT2";id=Id_SQRT2; break L;
                case 'a': X="atan2";id=Id_atan2; break L;
                case 'f': X="floor";id=Id_floor; break L;
                case 'r': X="round";id=Id_round; break L;
                } break L;
            case 6: c=s.charAt(0);
                if (c=='L') { X="LOG10E";id=Id_LOG10E; }
                else if (c=='r') { X="random";id=Id_random; }
                break L;
            case 7: X="SQRT1_2";id=Id_SQRT1_2; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_abs          =  1,
        Id_acos         =  2,
        Id_asin         =  3,
        Id_atan         =  4,
        Id_atan2        =  5,
        Id_ceil         =  6,
        Id_cos          =  7,
        Id_exp          =  8,
        Id_floor        =  9,
        Id_log          = 10,
        Id_max          = 11,
        Id_min          = 12,
        Id_pow          = 13,
        Id_random       = 14,
        Id_round        = 15,
        Id_sin          = 16,
        Id_sqrt         = 17,
        Id_tan          = 18,

        LAST_METHOD_ID  = 18,

        Id_E            = 19,
        Id_PI           = 20,
        Id_LN10         = 21,
        Id_LN2          = 22,
        Id_LOG2E        = 23,
        Id_LOG10E       = 24,
        Id_SQRT1_2      = 25,
        Id_SQRT2        = 26,

        MAX_ID          = 26;

// #/string_id_map#

    private static final Double
        E       = new Double(Math.E),
        PI      = new Double(Math.PI),
        LN10    = new Double(2.302585092994046),
        LN2     = new Double(0.6931471805599453),
        LOG2E   = new Double(1.4426950408889634),
        LOG10E  = new Double(0.4342944819032518),
        SQRT1_2 = new Double(0.7071067811865476),
        SQRT2   = new Double(1.4142135623730951);
}

