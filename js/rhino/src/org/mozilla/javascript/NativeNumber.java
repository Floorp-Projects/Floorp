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
public class NativeNumber extends IdScriptable {

    private static final int MAX_PRECISION = 100;

    /**
     * Zero-parameter constructor: just used to create Number.prototype
     */
    public NativeNumber() {
        doubleValue = defaultValue;
    }

    public NativeNumber(double number) {
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

        String[] names = { "NaN", "POSITIVE_INFINITY", "NEGATIVE_INFINITY",
                           "MAX_VALUE", "MIN_VALUE" };
        double[] values = { ScriptRuntime.NaN, Double.POSITIVE_INFINITY,
                            Double.NEGATIVE_INFINITY, Double.MAX_VALUE,
                            Double.MIN_VALUE };
        for (int i=0; i < names.length; i++) {
            ctor.defineProperty(names[i], new Double(values[i]), attr);
        }

        super.fillConstructorProperties(cx, ctor, sealed);
    }

    public int methodArity(int methodId) {
        switch (methodId) {
        case Id_constructor:     return 1; 
        case Id_toString:        return 1; 
        case Id_valueOf:         return 0; 
        case Id_toLocaleString:  return 1; 
        case Id_toFixed:         return 1;
        case Id_toExponential:   return 1;
        case Id_toPrecision:     return 1;
        }
        return super.methodArity(methodId);
    }

    public Object execMethod
        (int methodId, IdFunction f,
         Context cx, Scriptable scope, Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        switch (methodId) {

        case Id_constructor:
            return jsConstructor(args, thisObj == null);

        case Id_toString: return realThis(thisObj, f).
            jsFunction_toString(toBase(args, 0));

        case Id_valueOf: return wrap_double(realThis(thisObj, f).
            jsFunction_valueOf());

        case Id_toLocaleString: return realThis(thisObj, f).
            jsFunction_toLocaleString(toBase(args, 0));

        case Id_toFixed:
            return realThis(thisObj, f).jsFunction_toFixed(cx, args);

        case Id_toExponential:
            return realThis(thisObj, f).jsFunction_toExponential(cx, args);

        case Id_toPrecision:
            return realThis(thisObj, f).jsFunction_toPrecision(cx, args);

        }

        return super.execMethod(methodId, f, cx, scope, thisObj, args);
    }

    private NativeNumber realThis(Scriptable thisObj, IdFunction f) {
        while (!(thisObj instanceof NativeNumber)) {
            thisObj = nextInstanceCheck(thisObj, f, true);
        }
        return (NativeNumber)thisObj;
    }

    private static int toBase(Object[] args, int index) {
        return (index < args.length) ? ScriptRuntime.toInt32(args[index]) : 10;
    }

    private Object jsConstructor(Object[] args, boolean inNewExpr) {
        double d = args.length >= 1
                   ? ScriptRuntime.toNumber(args[0])
                   : defaultValue;
        if (inNewExpr) {
            // new Number(val) creates a new Number object.
            return new NativeNumber(d);
        }
        // Number(val) converts val to a number value.
        return wrap_double(d);
    }

    public String toString() {
        return jsFunction_toString(10);
    }

    private String jsFunction_toString(int base) {
        return ScriptRuntime.numberToString(doubleValue, base);
    }

    private double jsFunction_valueOf() {
        return doubleValue;
    }

    private String jsFunction_toLocaleString(int base) {
        return jsFunction_toString(base);
    }

    private String jsFunction_toFixed(Context cx, Object[] args) {
        /* We allow a larger range of precision than
           ECMA requires; this is permitted by ECMA. */
        return num_to(cx, args, DToA.DTOSTR_FIXED, DToA.DTOSTR_FIXED,
                      -20, MAX_PRECISION, 0);
    }

    private String jsFunction_toExponential(Context cx, Object[] args) {
        /* We allow a larger range of precision than
           ECMA requires; this is permitted by ECMA. */
        return num_to(cx, args,
                      DToA.DTOSTR_STANDARD_EXPONENTIAL,
                      DToA.DTOSTR_EXPONENTIAL,
                      0, MAX_PRECISION, 1);
    }

    private String jsFunction_toPrecision(Context cx, Object[] args) {
        /* We allow a larger range of precision than
           ECMA requires; this is permitted by ECMA. */
        return num_to(cx, args, DToA.DTOSTR_STANDARD, DToA.DTOSTR_PRECISION,
                      1, MAX_PRECISION, 0);
    }

    private String num_to(Context cx, Object[] args,
                          int zeroArgMode, int oneArgMode,
                          int precisionMin, int precisionMax,
                          int precisionOffset)
    {
        int precision;

        if (args.length == 0) {
            precision = 0;
            oneArgMode = zeroArgMode;
        } else {
            precision = ScriptRuntime.toInt32(args[0]);
            if (precision < precisionMin || precision > precisionMax) {
                String msg = ScriptRuntime.getMessage1(
                    "msg.bad.precision", ScriptRuntime.toString(args[0]));
                throw NativeGlobal.constructError(cx, "RangeError", msg, this);
            }
        }
        StringBuffer result = new StringBuffer();
        DToA.JS_dtostr(result, oneArgMode, precision + precisionOffset,
                       doubleValue);
        return result.toString();
    }

    protected int getMaximumId() { return MAX_ID; }

    protected String getIdName(int id) {
        switch (id) {
        case Id_constructor:     return "constructor"; 
        case Id_toString:        return "toString"; 
        case Id_valueOf:         return "valueOf"; 
        case Id_toLocaleString:  return "toLocaleString"; 
        case Id_toFixed:         return "toFixed";
        case Id_toExponential:   return "toExponential";
        case Id_toPrecision:     return "toPrecision";
        }
        return null;        
    }

// #string_id_map#

    protected int mapNameToId(String s) {
        int id;
// #generated# Last update: 2001-04-23 10:40:45 CEST
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 7: c=s.charAt(0);
                if (c=='t') { X="toFixed";id=Id_toFixed; }
                else if (c=='v') { X="valueOf";id=Id_valueOf; }
                break L;
            case 8: X="toString";id=Id_toString; break L;
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
        Id_valueOf               = 3,
        Id_toLocaleString        = 4,
        Id_toFixed               = 5,
        Id_toExponential         = 6,
        Id_toPrecision           = 7,
        MAX_ID                   = 7;

// #/string_id_map#

    private static final double defaultValue = +0.0;
    private double doubleValue;
}
