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
 * This class implements the Number native object.
 *
 * See ECMA 15.7.
 *
 * @author Norris Boyd
 */
final class NativeNumber extends IdScriptable {

    private static final int MAX_PRECISION = 100;

    static void init(Context cx, Scriptable scope, boolean sealed) {
        NativeNumber obj = new NativeNumber();
        obj.prototypeFlag = true;
        obj.addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    /**
     * Zero-parameter constructor: just used to create Number.prototype
     */
    private NativeNumber() {
    }

    private NativeNumber(double number) {
        doubleValue = number;
    }

    public String getClassName() {
        return "Number";
    }

    protected void fillConstructorProperties
        (Context cx, IdFunction ctor, boolean sealed)
    {
        final int attr = ScriptableObject.DONTENUM |
                         ScriptableObject.PERMANENT |
                         ScriptableObject.READONLY;

        ctor.defineProperty("NaN", ScriptRuntime.NaNobj, attr);
        ctor.defineProperty("POSITIVE_INFINITY",
                            wrap_double(Double.POSITIVE_INFINITY), attr);
        ctor.defineProperty("NEGATIVE_INFINITY",
                            wrap_double(Double.NEGATIVE_INFINITY), attr);
        ctor.defineProperty("MAX_VALUE", wrap_double(Double.MAX_VALUE), attr);
        ctor.defineProperty("MIN_VALUE", wrap_double(Double.MIN_VALUE), attr);

        super.fillConstructorProperties(cx, ctor, sealed);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (prototypeFlag) {
            switch (f.methodId()) {
                case Id_constructor: {
                    double val = (args.length >= 1)
                        ? ScriptRuntime.toNumber(args[0]) : 0.0;
                    if (thisObj == null) {
                        // new Number(val) creates a new Number object.
                        return new NativeNumber(val);
                    }
                    // Number(val) converts val to a number value.
                    return wrap_double(val);
                }

                case Id_toString:
                case Id_toLocaleString: {
                    // toLocaleString is just an alias for toString for now
                    double val = realThisValue(thisObj, f);
                    int base = (args.length == 0)
                        ? 10 : ScriptRuntime.toInt32(args[0]);
                    return ScriptRuntime.numberToString(val, base);
                }

                case Id_toSource: {
                    double val = realThisValue(thisObj, f);
                    return "(new Number("+ScriptRuntime.toString(val)+"))";
                }

                case Id_valueOf:
                    return wrap_double(realThisValue(thisObj, f));

                case Id_toFixed:
                    return num_to(f, thisObj, args,
                                  DToA.DTOSTR_FIXED, DToA.DTOSTR_FIXED,
                                  -20, 0);

                case Id_toExponential:
                    return num_to(f, thisObj, args,
                                  DToA.DTOSTR_STANDARD_EXPONENTIAL,
                                  DToA.DTOSTR_EXPONENTIAL,
                                  0, 1);

                case Id_toPrecision:
                    return num_to(f, thisObj, args,
                                  DToA.DTOSTR_STANDARD, DToA.DTOSTR_PRECISION,
                                  1, 0);
            }
        }
        return super.execMethod(f, cx, scope, thisObj, args);
    }

    private static double realThisValue(Scriptable thisObj, IdFunction f)
    {
        if (!(thisObj instanceof NativeNumber))
            throw incompatibleCallError(f);
        return ((NativeNumber)thisObj).doubleValue;
    }

    public String toString() {
        return ScriptRuntime.numberToString(doubleValue, 10);
    }

    private static String num_to(IdFunction f, Scriptable thisObj,
                                 Object[] args,
                                 int zeroArgMode, int oneArgMode,
                                 int precisionMin, int precisionOffset)
    {
        double val = realThisValue(thisObj, f);
        int precision;
        if (args.length == 0) {
            precision = 0;
            oneArgMode = zeroArgMode;
        } else {
            /* We allow a larger range of precision than
               ECMA requires; this is permitted by ECMA. */
            precision = ScriptRuntime.toInt32(args[0]);
            if (precision < precisionMin || precision > MAX_PRECISION) {
                String msg = ScriptRuntime.getMessage1(
                    "msg.bad.precision", ScriptRuntime.toString(args[0]));
                throw ScriptRuntime.constructError("RangeError", msg);
            }
        }
        StringBuffer sb = new StringBuffer();
        DToA.JS_dtostr(sb, oneArgMode, precision + precisionOffset, val);
        return sb.toString();
    }

    protected String getIdName(int id) {
        if (prototypeFlag) {
            switch (id) {
                case Id_constructor:     return "constructor";
                case Id_toString:        return "toString";
                case Id_toLocaleString:  return "toLocaleString";
                case Id_toSource:        return "toSource";
                case Id_valueOf:         return "valueOf";
                case Id_toFixed:         return "toFixed";
                case Id_toExponential:   return "toExponential";
                case Id_toPrecision:     return "toPrecision";
            }
        }
        return null;
    }

    protected int methodArity(int methodId) {
        if (prototypeFlag) {
            switch (methodId) {
                case Id_constructor:     return 1;
                case Id_toString:        return 1;
                case Id_toLocaleString:  return 1;
                case Id_toSource:        return 0;
                case Id_valueOf:         return 0;
                case Id_toFixed:         return 1;
                case Id_toExponential:   return 1;
                case Id_toPrecision:     return 1;
            }
        }
        return super.methodArity(methodId);
    }

// #string_id_map#

    protected int mapNameToId(String s) {
        if (!prototypeFlag) { return 0; }
        int id;
// #generated# Last update: 2004-03-17 13:41:35 CET
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 7: c=s.charAt(0);
                if (c=='t') { X="toFixed";id=Id_toFixed; }
                else if (c=='v') { X="valueOf";id=Id_valueOf; }
                break L;
            case 8: c=s.charAt(3);
                if (c=='o') { X="toSource";id=Id_toSource; }
                else if (c=='t') { X="toString";id=Id_toString; }
                break L;
            case 11: c=s.charAt(0);
                if (c=='c') { X="constructor";id=Id_constructor; }
                else if (c=='t') { X="toPrecision";id=Id_toPrecision; }
                break L;
            case 13: X="toExponential";id=Id_toExponential; break L;
            case 14: X="toLocaleString";id=Id_toLocaleString; break L;
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
        Id_toSource              = 4,
        Id_valueOf               = 5,
        Id_toFixed               = 6,
        Id_toExponential         = 7,
        Id_toPrecision           = 8,
        MAX_PROTOTYPE_ID         = 8;

// #/string_id_map#

    private double doubleValue;

    private boolean prototypeFlag;
}
