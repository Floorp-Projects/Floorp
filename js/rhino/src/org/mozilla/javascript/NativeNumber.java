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
public class NativeNumber extends ScriptableObject {

    public static void finishInit(Scriptable scope,
                                  FunctionObject ctor, Scriptable proto)
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
    }
	
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

    public static Object jsConstructor(Context cx, Object[] args, 
                                       Function funObj, boolean inNewExpr)
    {
        double d = args.length >= 1
                   ? ScriptRuntime.toNumber(args[0])
                   : defaultValue;
    	if (inNewExpr) {
    	    // new Number(val) creates a new Number object.
    	    return new NativeNumber(d);
    	}
    	// Number(val) converts val to a number value.
    	return new Double(d);
    }
    
    public String toString() {
        return jsFunction_toString(Undefined.instance);
    }

    public String jsFunction_toString(Object base) {
        int i = base == Undefined.instance
                ? 10 
                : ScriptRuntime.toInt32(base);
        return ScriptRuntime.numberToString(doubleValue, i);
    }

    public double jsFunction_valueOf() {
        return doubleValue;
    }

    public String jsFunction_toLocaleString(Object arg) {
		return toString();
    }

    public String jsFunction_toFixed(Object arg) {
    /* We allow a larger range of precision than 
		ECMA requires; this is permitted by ECMA. */
		return num_to(arg, DToA.DTOSTR_FIXED, DToA.DTOSTR_FIXED,
												-20, MAX_PRECISION, 0);
    }

    public String jsFunction_toExponential(Object arg) {
    /* We allow a larger range of precision than
		ECMA requires; this is permitted by ECMA. */
		return num_to(arg, DToA.DTOSTR_STANDARD_EXPONENTIAL,
							DToA.DTOSTR_EXPONENTIAL, 0, MAX_PRECISION, 1);
    }

    public String jsFunction_toPrecision(Object arg) {
    /* We allow a larger range of precision than
		ECMA requires; this is permitted by ECMA. */
		return num_to(arg, DToA.DTOSTR_STANDARD, 
							DToA.DTOSTR_PRECISION, 1, MAX_PRECISION, 0);
	}

	private String num_to(Object arg, int zeroArgMode,
       int oneArgMode, int precisionMin, int precisionMax, int precisionOffset)
	{
	    int precision;

		if (arg == Undefined.instance) {
			precision = 0;
			oneArgMode = zeroArgMode;							
	    } else {
			precision = ScriptRuntime.toInt32(arg);
	        if (precision < precisionMin || precision > precisionMax) {
			    throw NativeGlobal.constructError(
			               Context.getCurrentContext(), "RangeError",
                           ScriptRuntime.getMessage1(
                               "msg.bad.precision", Integer.toString(precision)),
			               this);
			}
		}
		StringBuffer result = new StringBuffer();	
	    DToA.JS_dtostr(result, oneArgMode, precision + precisionOffset, doubleValue);
		return result.toString();
	}	
	
    private static final double defaultValue = +0.0;
    private double doubleValue;
}
