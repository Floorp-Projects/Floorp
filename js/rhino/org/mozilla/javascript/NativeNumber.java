/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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

    public static Object Number(Context cx, Object[] args, Function funObj,
                                boolean inNewExpr)
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
	    return ScriptRuntime.numberToString(doubleValue);
    }

    public double valueOf() {
	    return doubleValue;
    }

    private static final double defaultValue = +0.0;
    private double doubleValue;
}
