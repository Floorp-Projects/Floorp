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
public class NativeMath extends ScriptableObject {

    public static Scriptable init(Scriptable scope)
        throws PropertyException
    {
        NativeMath m = new NativeMath();
        m.setPrototype(getObjectPrototype(scope));
        m.setParentScope(scope);

        String[] names = { "atan", "atan2", "ceil",
                           "cos", "floor", "random",
                           "sin", "sqrt", "tan" };

        m.defineFunctionProperties(names, java.lang.Math.class,
                                   ScriptableObject.DONTENUM);

        // These functions exist in java.lang.Math, but
        // are overloaded. Define our own wrappers.
        String[] localNames = { "acos", "asin", "abs", "exp", "max", "min",
                                "round", "pow", "log" };

        m.defineFunctionProperties(localNames, NativeMath.class,
                                   ScriptableObject.DONTENUM);

        /*
            have to fix up the length property for max & min
            which are varargs form, but need to have a length of 2
        */
        ((FunctionObject)m.get("max", scope)).setLength((short)2);
        ((FunctionObject)m.get("min", scope)).setLength((short)2);

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

    public static double abs(double d) {
        if (d == 0.0)
            return 0.0; // abs(-0.0) should be 0.0, but -0.0 < 0.0 == false
        else if (d < 0.0)
            return -d;
        else
            return d;
    }

    public static double acos(double d) {
        if ((d != d) 
                || (d > 1.0)
                || (d < -1.0))
            return Double.NaN;
        return Math.acos(d);
    }

    public static double asin(double d) {
        if ((d != d) 
                || (d > 1.0)
                || (d < -1.0))
            return Double.NaN;
        return Math.asin(d);
    }

    public static double max(Context cx, Scriptable thisObj, 
                             Object[] args, Function funObj) 
    {        
        double result = Double.NEGATIVE_INFINITY;
        if (args.length == 0)
            return result;
        for (int i = 0; i < args.length; i++) {
            double d = ScriptRuntime.toNumber(args[i]);
            if (d != d) return d;
            result = Math.max(result, d);
        }
        return result;
    }

    public static double min(Context cx, Scriptable thisObj,
                             Object[] args, Function funObj)
    {
        double result = Double.POSITIVE_INFINITY;
        if (args.length == 0)
            return result;
        for (int i = 0; i < args.length; i++) {
            double d = ScriptRuntime.toNumber(args[i]);
            if (d != d) return d;
            result = Math.min(result, d);
        }
        return result;
    }

    public static double round(double d) {
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

    public static double pow(double x, double y) {
        if (y == 0)
            return 1.0;   // Java's pow(NaN, 0) = NaN; we need 1
        if ((x == 0) && (y < 0)) {
            Double d = new Double(x);
            if (d.equals(new Double(0)))            // x is +0
                return Double.POSITIVE_INFINITY;    // Java is -Infinity
            /* if x is -0 and y is an odd integer, -Infinity */
            if (((int)y == y) && (((int)y & 0x1) == 1))
                return Double.NEGATIVE_INFINITY;
            return Double.POSITIVE_INFINITY;
        }
        return Math.pow(x, y);
    }
    
    public static double exp(double d) {
        if (d == Double.POSITIVE_INFINITY)
            return d;
        if (d == Double.NEGATIVE_INFINITY)
            return 0.0;
        return Math.exp(d);
    }

    public static double log(double x) {
        if (x < 0)
            return Double.NaN;   // Java's log(<0) = -Infinity; we need NaN
        return Math.log(x);
    }
    
}
