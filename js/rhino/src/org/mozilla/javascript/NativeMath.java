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
 * This class implements the Math native object.
 * See ECMA 15.8.
 * @author Norris Boyd
 */
public class NativeMath extends ScriptableObject {

    public static Scriptable init(Scriptable scope)
        throws PropertyException
    {
        NativeMath m = new NativeMath();
        m.setPrototype(getObjectPrototype(scope));
        m.setParentScope(scope);

        String[] names = { "acos", "asin", "atan", "atan2", "ceil",
                           "cos", "floor", "log", "random",
                           "sin", "sqrt", "tan" };

        m.defineFunctionProperties(names, java.lang.Math.class,
                                   ScriptableObject.DONTENUM);

        // These functions exist in java.lang.Math, but
        // are overloaded. Define our own wrappers.
        String[] localNames = { "abs", "exp", "max", "min", "round", "pow" };

        m.defineFunctionProperties(localNames, NativeMath.class,
                                   ScriptableObject.DONTENUM);

        final int attr = ScriptableObject.DONTENUM  |
                         ScriptableObject.PERMANENT |
                         ScriptableObject.READONLY;

        m.defineProperty("E", new Double(Math.E), attr);
        m.defineProperty("PI", new Double(Math.PI), attr);
        m.defineProperty("LN10", new Double(2.302585092994046), attr);
        m.defineProperty("LN2", new Double(0.6931471805599453), attr);
        m.defineProperty("LOG2E", new Double(1.4426950408889634), attr);
        m.defineProperty("LOG10E", new Double(0.4342944819032518), attr);
        m.defineProperty("SQRT1_2", new Double(0.7071067811865476), attr);
        m.defineProperty("SQRT2", new Double(1.4142135623730951), attr);

        // We know that scope is a Scriptable object since we
        // constrained the type on initStandardObjects.
        ScriptableObject global = (ScriptableObject) scope;
        global.defineProperty("Math", m, ScriptableObject.DONTENUM);

        return m;
    }

    public NativeMath() {
    }

    public String getClassName() {
        return "Math";
    }

    public double abs(double d) {
        if (d == 0.0)
            return 0.0; // abs(-0.0) should be 0.0, but -0.0 < 0.0 == false
        else if (d < 0.0)
            return -d;
        else
            return d;
    }

    public double max(double x, double y) {
        return Math.max(x, y);
    }

    public double min(double x, double y) {
        return Math.min(x, y);
    }

    public double round(double d) {
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

    public double pow(double x, double y) {
        if (y == 0)
            return 1.0;   // Java's pow(NaN, 0) = NaN; we need 1
        return Math.pow(x, y);
    }
    
    public double exp(double d) {
        if (d == Double.POSITIVE_INFINITY)
            return d;
        if (d == Double.NEGATIVE_INFINITY)
            return 0.0;
        return Math.exp(d);
    }
}
