/* -*- Mode: java; tab-width: 4; indent-tabs-mode: 1; c-basic-offset: 4 -*-
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
 * This class implements the Math native object.
 * See ECMA 15.8.
 * @author Norris Boyd
 */

final class NativeMath extends IdScriptable
{
    static void init(Context cx, Scriptable scope, boolean sealed)
    {
        NativeMath obj = new NativeMath();
        obj.setPrototype(getObjectPrototype(scope));
        obj.setParentScope(scope);
        if (sealed) { obj.sealObject(); }
        ScriptableObject.defineProperty(scope, "Math", obj,
                                        ScriptableObject.DONTENUM);
    }

    public String getClassName() { return "Math"; }

    protected int getIdAttributes(int id)
    {
        if (id > LAST_METHOD_ID) {
            return DONTENUM | READONLY | PERMANENT;
        }
        return super.getIdAttributes(id);
    }

    protected Object getIdValue(int id)
    {
        if (id > LAST_METHOD_ID) {
            double x;
            switch (id) {
                case Id_E:       x = Math.E;             break;
                case Id_PI:      x = Math.PI;            break;
                case Id_LN10:    x = 2.302585092994046;  break;
                case Id_LN2:     x = 0.6931471805599453; break;
                case Id_LOG2E:   x = 1.4426950408889634; break;
                case Id_LOG10E:  x = 0.4342944819032518; break;
                case Id_SQRT1_2: x = 0.7071067811865476; break;
                case Id_SQRT2:   x = 1.4142135623730951; break;
                default: /* Unreachable */ x = 0;
            }
            return cacheIdValue(id, wrap_double(x));
        }
        return super.getIdValue(id);
    }

    public int methodArity(IdFunction f)
    {
        switch (f.methodId) {
            case Id_toSource: return 0;
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
        return super.methodArity(f);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        double x;
        switch (f.methodId) {
            case Id_toSource:
                return "Math";

            case Id_abs:
                x = ScriptRuntime.toNumber(args, 0);
                // abs(-0.0) should be 0.0, but -0.0 < 0.0 == false
                x = (x == 0.0) ? 0.0 : (x < 0.0) ? -x : x;
                break;

            case Id_acos:
            case Id_asin:
                x = ScriptRuntime.toNumber(args, 0);
                if (x == x && -1.0 <= x && x <= 1.0) {
                    x = (f.methodId == Id_acos) ? Math.acos(x) : Math.asin(x);
                } else {
                    x = Double.NaN;
                }
                break;

            case Id_atan:
                x = ScriptRuntime.toNumber(args, 0);
                x = Math.atan(x);
                break;

            case Id_atan2:
                x = ScriptRuntime.toNumber(args, 0);
                x = Math.atan2(x, ScriptRuntime.toNumber(args, 1));
                break;

            case Id_ceil:
                x = ScriptRuntime.toNumber(args, 0);
                x = Math.ceil(x);
                break;

            case Id_cos:
                x = ScriptRuntime.toNumber(args, 0);
                x = (x == Double.POSITIVE_INFINITY
                     || x == Double.NEGATIVE_INFINITY)
                    ? Double.NaN : Math.cos(x);
                break;

            case Id_exp:
                x = ScriptRuntime.toNumber(args, 0);
                x = (x == Double.POSITIVE_INFINITY) ? x
                    : (x == Double.NEGATIVE_INFINITY) ? 0.0
                    : Math.exp(x);
                break;

            case Id_floor:
                x = ScriptRuntime.toNumber(args, 0);
                x = Math.floor(x);
                break;

            case Id_log:
                x = ScriptRuntime.toNumber(args, 0);
                // Java's log(<0) = -Infinity; we need NaN
                x = (x < 0) ? Double.NaN : Math.log(x);
                break;

            case Id_max:
            case Id_min:
                x = (f.methodId == Id_max)
                    ? Double.NEGATIVE_INFINITY : Double.POSITIVE_INFINITY;
                for (int i = 0; i != args.length; ++i) {
                    double d = ScriptRuntime.toNumber(args[i]);
                    if (d != d) {
                        x = d; // NaN
                        break;
                    }
                    if (f.methodId == Id_max) {
                        // if (x < d) x = d; does not work due to -0.0 >= +0.0
                        x = Math.max(x, d);
                    } else {
                        x = Math.min(x, d);
                    }
                }
                break;

            case Id_pow:
                x = ScriptRuntime.toNumber(args, 0);
                x = js_pow(x, ScriptRuntime.toNumber(args, 1));
                break;

            case Id_random:
                x = Math.random();
                break;

            case Id_round:
                x = ScriptRuntime.toNumber(args, 0);
                if (x == x && x != Double.POSITIVE_INFINITY
                    && x != Double.NEGATIVE_INFINITY)
                {
                    // Round only finite x
                    long l = Math.round(x);
                    if (l != 0) {
                        x = (double)l;
                    } else {
                        // We must propagate the sign of d into the result
                        if (x < 0.0) {
                            x = ScriptRuntime.negativeZero;
                        } else if (x != 0.0) {
                            x = 0.0;
                        }
                    }
                }
                break;

            case Id_sin:
                x = ScriptRuntime.toNumber(args, 0);
                x = (x == Double.POSITIVE_INFINITY
                     || x == Double.NEGATIVE_INFINITY)
                    ? Double.NaN : Math.sin(x);
                break;

            case Id_sqrt:
                x = ScriptRuntime.toNumber(args, 0);
                x = Math.sqrt(x);
                break;

            case Id_tan:
                x = ScriptRuntime.toNumber(args, 0);
                x = Math.tan(x);
                break;

            default:
                return super.execMethod(f, cx, scope, thisObj, args);
        }
        return wrap_double(x);
    }

    // See Ecma 15.8.2.13
    private double js_pow(double x, double y) {
        double result;
        if (y != y) {
            // y is NaN, result is always NaN
            result = y;
        } else if (y == 0) {
            // Java's pow(NaN, 0) = NaN; we need 1
            result = 1.0;
        } else if (x == 0) {
            // Many dirrerences from Java's Math.pow
            if (1 / x > 0) {
                result = (y > 0) ? 0 : Double.POSITIVE_INFINITY;
            } else {
                // x is -0, need to check if y is an odd integer
                long y_long = (long)y;
                if (y_long == y && (y_long & 0x1) != 0) {
                    result = (y > 0) ? -0.0 : Double.NEGATIVE_INFINITY;
                } else {
                    result = (y > 0) ? 0.0 : Double.POSITIVE_INFINITY;
                }
            }
        } else {
            result = Math.pow(x, y);
            if (result != result) {
                // Check for broken Java implementations that gives NaN
                // when they should return something else
                if (y == Double.POSITIVE_INFINITY) {
                    if (x < -1.0 || 1.0 < x) {
                        result = Double.POSITIVE_INFINITY;
                    } else if (-1.0 < x && x < 1.0) {
                        result = 0;
                    }
                } else if (y == Double.NEGATIVE_INFINITY) {
                    if (x < -1.0 || 1.0 < x) {
                        result = 0;
                    } else if (-1.0 < x && x < 1.0) {
                        result = Double.POSITIVE_INFINITY;
                    }
                } else if (x == Double.POSITIVE_INFINITY) {
                    result = (y > 0) ? Double.POSITIVE_INFINITY : 0.0;
                } else if (x == Double.NEGATIVE_INFINITY) {
                    long y_long = (long)y;
                    if (y_long == y && (y_long & 0x1) != 0) {
                        // y is odd integer
                        result = (y > 0) ? Double.NEGATIVE_INFINITY : -0.0;
                    } else {
                        result = (y > 0) ? Double.POSITIVE_INFINITY : 0.0;
                    }
                }
            }
        }
        return result;
    }

    protected String getIdName(int id)
    {
        switch (id) {
            case Id_toSource: return "toSource";
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

    protected int mapNameToId(String s)
    {
        int id;
// #generated# Last update: 2004-03-17 13:51:32 CET
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
            case 8: X="toSource";id=Id_toSource; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_toSource     =  1,
        Id_abs          =  2,
        Id_acos         =  3,
        Id_asin         =  4,
        Id_atan         =  5,
        Id_atan2        =  6,
        Id_ceil         =  7,
        Id_cos          =  8,
        Id_exp          =  9,
        Id_floor        = 10,
        Id_log          = 11,
        Id_max          = 12,
        Id_min          = 13,
        Id_pow          = 14,
        Id_random       = 15,
        Id_round        = 16,
        Id_sin          = 17,
        Id_sqrt         = 18,
        Id_tan          = 19,

        LAST_METHOD_ID  = 19;

    private static final int
        Id_E            = LAST_METHOD_ID + 1,
        Id_PI           = LAST_METHOD_ID + 2,
        Id_LN10         = LAST_METHOD_ID + 3,
        Id_LN2          = LAST_METHOD_ID + 4,
        Id_LOG2E        = LAST_METHOD_ID + 5,
        Id_LOG10E       = LAST_METHOD_ID + 6,
        Id_SQRT1_2      = LAST_METHOD_ID + 7,
        Id_SQRT2        = LAST_METHOD_ID + 8,

        MAX_INSTANCE_ID = LAST_METHOD_ID + 8;

    { setMaxId(MAX_INSTANCE_ID); }

// #/string_id_map#
}
