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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Patrick Beard
 * Norris Boyd
 * Igor Bukanov 
 * Roger Lawrence
 * Frank Mitchell
 * Andrew Wason
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

import java.util.*;
import java.lang.reflect.*;

/**
 * This is the class that implements the runtime.
 *
 * @author Norris Boyd
 */

public class ScriptRuntime {

    /**
     * No instances should be created.
     */
    protected ScriptRuntime() {
    }

    /*
     * There's such a huge space (and some time) waste for the Foo.class
     * syntax: the compiler sticks in a test of a static field in the
     * enclosing class for null and the code for creating the class value.
     * It has to do this since the reference has to get pushed off til
     * executiontime (i.e. can't force an early load), but for the
     * 'standard' classes - especially those in java.lang, we can trust
     * that they won't cause problems by being loaded early.
     */

    public final static Class UndefinedClass = Undefined.class;
    public final static Class ScriptableClass = Scriptable.class;
    public final static Class StringClass = String.class;
    public final static Class NumberClass = Number.class;
    public final static Class BooleanClass = Boolean.class;
    public final static Class ByteClass = Byte.class;
    public final static Class ShortClass = Short.class;
    public final static Class IntegerClass = Integer.class;
    public final static Class LongClass = Long.class;
    public final static Class FloatClass = Float.class;
    public final static Class DoubleClass = Double.class;
    public final static Class CharacterClass = Character.class;
    public final static Class ObjectClass = Object.class;
    public final static Class FunctionClass = Function.class;
    public final static Class ClassClass = Class.class;

    /**
     * Convert the value to a boolean.
     *
     * See ECMA 9.2.
     */
    public static boolean toBoolean(Object val) {
        if (val == null)
            return false;
        if (val instanceof Scriptable) {
            if (Context.getContext().isVersionECMA1()) {
                // pure ECMA
                return val != Undefined.instance;
            }
            // ECMA extension
            val = ((Scriptable) val).getDefaultValue(BooleanClass);
            if (val instanceof Scriptable)
                throw errorWithClassName("msg.primitive.expected", val);
            // fall through
        }
        if (val instanceof String)
            return ((String) val).length() != 0;
        if (val instanceof Number) {
            double d = ((Number) val).doubleValue();
            return (d == d && d != 0.0);
        }
        if (val instanceof Boolean)
            return ((Boolean) val).booleanValue();
        throw errorWithClassName("msg.invalid.type", val);
    }

    public static boolean toBoolean(Object[] args, int index) {
        return (index < args.length) ? toBoolean(args[index]) : false;
    }
    /**
     * Convert the value to a number.
     *
     * See ECMA 9.3.
     */
    public static double toNumber(Object val) {
        if (val == null) 
            return +0.0;
        if (val instanceof Scriptable) {
            val = ((Scriptable) val).getDefaultValue(NumberClass);
            if (val != null && val instanceof Scriptable)
                throw errorWithClassName("msg.primitive.expected", val);
            // fall through
        }
        if (val instanceof String)
            return toNumber((String) val);
        if (val instanceof Number)
            return ((Number) val).doubleValue();
        if (val instanceof Boolean)
            return ((Boolean) val).booleanValue() ? 1 : +0.0;
        throw errorWithClassName("msg.invalid.type", val);
    }

    public static double toNumber(Object[] args, int index) {
        return (index < args.length) ? toNumber(args[index]) : NaN;
    }
    
    // This definition of NaN is identical to that in java.lang.Double
    // except that it is not final. This is a workaround for a bug in
    // the Microsoft VM, versions 2.01 and 3.0P1, that causes some uses
    // (returns at least) of Double.NaN to be converted to 1.0.
    // So we use ScriptRuntime.NaN instead of Double.NaN.
    public static double NaN = 0.0d / 0.0;
    public static Double NaNobj = new Double(0.0d / 0.0);

    // A similar problem exists for negative zero.
    public static double negativeZero = -0.0;

    /*
     * Helper function for toNumber, parseInt, and TokenStream.getToken.
     */
    static double stringToNumber(String s, int start, int radix) {
        char digitMax = '9';
        char lowerCaseBound = 'a';
        char upperCaseBound = 'A';
        int len = s.length();
        if (radix < 10) {
            digitMax = (char) ('0' + radix - 1);
        }
        if (radix > 10) {
            lowerCaseBound = (char) ('a' + radix - 10);
            upperCaseBound = (char) ('A' + radix - 10);
        }
        int end;
        double sum = 0.0;
        for (end=start; end < len; end++) {
            char c = s.charAt(end);
            int newDigit;
            if ('0' <= c && c <= digitMax)
                newDigit = c - '0';
            else if ('a' <= c && c < lowerCaseBound)
                newDigit = c - 'a' + 10;
            else if ('A' <= c && c < upperCaseBound)
                newDigit = c - 'A' + 10;
            else
                break;
            sum = sum*radix + newDigit;
        }
        if (start == end) {
            return NaN;
        }
        if (sum >= 9007199254740992.0) {
            if (radix == 10) {
                /* If we're accumulating a decimal number and the number
                 * is >= 2^53, then the result from the repeated multiply-add
                 * above may be inaccurate.  Call Java to get the correct
                 * answer.
                 */
                try {
                    return Double.valueOf(s.substring(start, end)).doubleValue();
                } catch (NumberFormatException nfe) {
                    return NaN;
                }
            } else if (radix == 2 || radix == 4 || radix == 8 ||
                       radix == 16 || radix == 32)
            {
                /* The number may also be inaccurate for one of these bases.
                 * This happens if the addition in value*radix + digit causes
                 * a round-down to an even least significant mantissa bit
                 * when the first dropped bit is a one.  If any of the
                 * following digits in the number (which haven't been added
                 * in yet) are nonzero then the correct action would have
                 * been to round up instead of down.  An example of this
                 * occurs when reading the number 0x1000000000000081, which
                 * rounds to 0x1000000000000000 instead of 0x1000000000000100.
                 */
                BinaryDigitReader bdr = new BinaryDigitReader(radix, s, start, end);
                int bit;
                sum = 0.0;

                /* Skip leading zeros. */
                do {
                    bit = bdr.getNextBinaryDigit();
                } while (bit == 0);

                if (bit == 1) {
                    /* Gather the 53 significant bits (including the leading 1) */
                    sum = 1.0;
                    for (int j = 52; j != 0; j--) {
                        bit = bdr.getNextBinaryDigit();
                        if (bit < 0)
                            return sum;
                        sum = sum*2 + bit;
                    }
                    /* bit54 is the 54th bit (the first dropped from the mantissa) */
                    int bit54 = bdr.getNextBinaryDigit();
                    if (bit54 >= 0) {
                        double factor = 2.0;
                        int sticky = 0;  /* sticky is 1 if any bit beyond the 54th is 1 */
                        int bit3;

                        while ((bit3 = bdr.getNextBinaryDigit()) >= 0) {
                            sticky |= bit3;
                            factor *= 2;
                        }
                        sum += bit54 & (bit | sticky);
                        sum *= factor;
                    }
                }
            }
            /* We don't worry about inaccurate numbers for any other base. */
        }
        return sum;
    }


    /**
     * ToNumber applied to the String type
     *
     * See ECMA 9.3.1
     */
    public static double toNumber(String s) {
        int len = s.length();
        int start = 0;
        char startChar;
        for (;;) {
            if (start == len) {
                // Empty or contains only whitespace
                return +0.0;
            }
            startChar = s.charAt(start);
            if (!Character.isWhitespace(startChar))
                break;
            start++;
        }

        if (startChar == '0' && start+2 < len &&
            Character.toLowerCase(s.charAt(start+1)) == 'x')
            // A hexadecimal number
            return stringToNumber(s, start + 2, 16);

        if ((startChar == '+' || startChar == '-') && start+3 < len &&
            s.charAt(start+1) == '0' &&
            Character.toLowerCase(s.charAt(start+2)) == 'x') {
            // A hexadecimal number
            double val = stringToNumber(s, start + 3, 16);
            return startChar == '-' ? -val : val;
        }

        int end = len - 1;
        char endChar;
        while (Character.isWhitespace(endChar = s.charAt(end)))
            end--;
        if (endChar == 'y') {
            // check for "Infinity"
            if (startChar == '+' || startChar == '-')
                start++;
            if (start + 7 == end && s.regionMatches(start, "Infinity", 0, 8))
                return startChar == '-'
                    ? Double.NEGATIVE_INFINITY
                    : Double.POSITIVE_INFINITY;
            return NaN;
        }
        // A non-hexadecimal, non-infinity number:
        // just try a normal floating point conversion
        String sub = s.substring(start, end+1);
        if (MSJVM_BUG_WORKAROUNDS) {
            // The MS JVM will accept non-conformant strings
            // rather than throwing a NumberFormatException
            // as it should.
            for (int i=sub.length()-1; i >= 0; i--) {
                char c = sub.charAt(i);
                if (('0' <= c && c <= '9') || c == '.' ||
                    c == 'e' || c == 'E'  ||
                    c == '+' || c == '-')
                    continue;
                return NaN;
            }
        }
        try {
            return Double.valueOf(sub).doubleValue();
        } catch (NumberFormatException ex) {
            return NaN;
        }
    }

    /**
     * Helper function for builtin objects that use the varargs form.
     * ECMA function formal arguments are undefined if not supplied;
     * this function pads the argument array out to the expected
     * length, if necessary.
     */
    public static Object[] padArguments(Object[] args, int count) {
        if (count < args.length)
            return args;

        int i;
        Object[] result = new Object[count];
        for (i = 0; i < args.length; i++) {
            result[i] = args[i];
        }

        for (; i < count; i++) {
            result[i] = Undefined.instance;
        }

        return result;
    }

    /* Work around Microsoft Java VM bugs. */
    private final static boolean MSJVM_BUG_WORKAROUNDS = true;

    /**
     * For escaping strings printed by object and array literals; not quite
     * the same as 'escape.'
     */
    public static String escapeString(String s) {
        // ack!  Java lacks \v.
        String escapeMap = "\bb\ff\nn\rr\tt\u000bv\"\"''";
        StringBuffer result = new StringBuffer(s.length());

        for(int i=0; i < s.length(); i++) {
            char c = s.charAt(i);

            // an ordinary print character
            if (c >= ' ' && c <= '~'     // string.h isprint()
                && c != '"')
            {
                result.append(c);
                continue;
            }

            // an \escaped sort of character
            int index;
            if ((index = escapeMap.indexOf(c)) >= 0) {
                result.append("\\");
                result.append(escapeMap.charAt(index + 1));
                continue;
            }

            // 2-digit hex?
            if (c < 256) {
                String hex = Integer.toHexString((int) c);
                if (hex.length() == 1) {
                    result.append("\\x0");
                    result.append(hex);
                } else {
                    result.append("\\x");
                    result.append(hex);
                }
                continue;
            }

            // nope.  Unicode.
            String hex = Integer.toHexString((int) c);
            // cool idiom courtesy Shaver.
            result.append("\\u");
            for (int l = hex.length(); l < 4; l++)
                result.append("0");
            result.append(hex);
        }

        return result.toString();
    }


    /**
     * Convert the value to a string.
     *
     * See ECMA 9.8.
     */
    public static String toString(Object val) {
        for (;;) {
            if (val == null)
                return "null";
            if (val instanceof Scriptable) {
                val = ((Scriptable) val).getDefaultValue(StringClass);
                if (val != Undefined.instance && val instanceof Scriptable) {
                    throw errorWithClassName("msg.primitive.expected", val);
                }
                continue;
            }
            if (val instanceof Number) {
                // XXX should we just teach NativeNumber.stringValue()
                // about Numbers?
                return numberToString(((Number) val).doubleValue(), 10);
            }
            return val.toString();
        }
    }

    public static String toString(Object[] args, int index) {
        return (index < args.length) ? toString(args[index]) : "undefined";
    }

    public static String numberToString(double d, int base) {
        if (d != d)
            return "NaN";
        if (d == Double.POSITIVE_INFINITY)
            return "Infinity";
        if (d == Double.NEGATIVE_INFINITY)
            return "-Infinity";
        if (d == 0.0)
            return "0";

        if ((base < 2) || (base > 36)) {
            throw Context.reportRuntimeError1(
                "msg.bad.radix", Integer.toString(base));
        }

        if (base != 10) {
            return DToA.JS_dtobasestr(base, d);
        } else {
            StringBuffer result = new StringBuffer();
            DToA.JS_dtostr(result, DToA.DTOSTR_STANDARD, 0, d);
            return result.toString();
        }
    
    }

    /**
     * Convert the value to an object.
     *
     * See ECMA 9.9.
     */
    public static Scriptable toObject(Scriptable scope, Object val) {
        return toObject(scope, val, null);
    }

    public static Scriptable toObject(Scriptable scope, Object val,
                                      Class staticClass)
    {
        if (val == null) {
            throw NativeGlobal.typeError0("msg.null.to.object", scope);
        }
        if (val instanceof Scriptable) {
            if (val == Undefined.instance) {
                throw NativeGlobal.typeError0("msg.undef.to.object", scope);
            }
            return (Scriptable) val;
        }
        String className = val instanceof String ? "String" :
                           val instanceof Number ? "Number" :
                           val instanceof Boolean ? "Boolean" :
                           null;

        if (className == null) {
            // Extension: Wrap as a LiveConnect object.
            Object wrapped = NativeJavaObject.wrap(scope, val, staticClass);
            if (wrapped instanceof Scriptable)
                return (Scriptable) wrapped;
            throw errorWithClassName("msg.invalid.type", val);
        }

        Object[] args = { val };
        scope = ScriptableObject.getTopLevelScope(scope);
        Scriptable result = newObject(Context.getContext(), scope,
                                      className, args);
        return result;
    }

    public static Scriptable newObject(Context cx, Scriptable scope,
                                       String constructorName, Object[] args)
    {
        Exception re = null;
        try {
            return cx.newObject(scope, constructorName, args);
        }
        catch (NotAFunctionException e) {
            re = e;
        }
        catch (PropertyException e) {
            re = e;
        }
        catch (JavaScriptException e) {
            re = e;
        }
        throw cx.reportRuntimeError(re.getMessage());
    }

    /**
     *
     * See ECMA 9.4.
     */
    public static double toInteger(Object val) {
        return toInteger(toNumber(val));
    }

    // convenience method
    public static double toInteger(double d) {
        // if it's NaN
        if (d != d)
            return +0.0;

        if (d == 0.0 ||
            d == Double.POSITIVE_INFINITY ||
            d == Double.NEGATIVE_INFINITY)
            return d;

        if (d > 0.0)
            return Math.floor(d);
        else
            return Math.ceil(d);
    }

    public static double toInteger(Object[] args, int index) {
        return (index < args.length) ? toInteger(args[index]) : +0.0;
    }

    /**
     *
     * See ECMA 9.5.
     */
    public static int toInt32(Object val) {
        // 0x100000000 gives me a numeric overflow...
        double two32 = 4294967296.0;
        double two31 = 2147483648.0;

        // short circuit for common small values; TokenStream
        // returns them as Bytes.
        if (val instanceof Byte)
            return ((Number)val).intValue();

        double d = toNumber(val);
        if (d != d || d == 0.0 ||
            d == Double.POSITIVE_INFINITY ||
            d == Double.NEGATIVE_INFINITY)
            return 0;

        d = Math.IEEEremainder(d, two32);

        d = d >= 0
            ? d
            : d + two32;

        if (d >= two31)
            return (int)(d - two32);
        else
            return (int)d;
    }

    public static int toInt32(Object[] args, int index) {
        return (index < args.length) ? toInt32(args[index]) : 0;
    }

    public static int toInt32(double d) {
        // 0x100000000 gives me a numeric overflow...
        double two32 = 4294967296.0;
        double two31 = 2147483648.0;

        if (d != d || d == 0.0 ||
            d == Double.POSITIVE_INFINITY ||
            d == Double.NEGATIVE_INFINITY)
            return 0;

        d = Math.IEEEremainder(d, two32);

        d = d >= 0
            ? d
            : d + two32;

        if (d >= two31)
            return (int)(d - two32);
        else
            return (int)d;
    }

    /**
     *
     * See ECMA 9.6.
     */

    // must return long to hold an _unsigned_ int
    public static long toUint32(double d) {
        // 0x100000000 gives me a numeric overflow...
        double two32 = 4294967296.0;

        if (d != d || d == 0.0 ||
            d == Double.POSITIVE_INFINITY ||
            d == Double.NEGATIVE_INFINITY)
            return 0;

        if (d > 0.0)
            d = Math.floor(d);
        else
            d = Math.ceil(d);

    d = Math.IEEEremainder(d, two32);

    d = d >= 0
            ? d
            : d + two32;

        return (long) Math.floor(d);
    }

    public static long toUint32(Object val) {
        return toUint32(toNumber(val));
    }

    /**
     *
     * See ECMA 9.7.
     */
    public static char toUint16(Object val) {
    long int16 = 0x10000;

    double d = toNumber(val);
    if (d != d || d == 0.0 ||
        d == Double.POSITIVE_INFINITY ||
        d == Double.NEGATIVE_INFINITY)
    {
        return 0;
    }

    d = Math.IEEEremainder(d, int16);

    d = d >= 0
            ? d
        : d + int16;

    return (char) Math.floor(d);
    }

    /**
     * Unwrap a JavaScriptException.  Sleight of hand so that we don't
     * javadoc JavaScriptException.getRuntimeValue().
     */
    public static Object unwrapJavaScriptException(JavaScriptException jse) {
        return jse.value;
    }

    public static Object getProp(Object obj, String id, Scriptable scope) {
        Scriptable start;
        if (obj instanceof Scriptable) {
            start = (Scriptable) obj;
        } else {
            start = toObject(scope, obj);
        }
        if (start == null || start == Undefined.instance) {
            String msg = start == null ? "msg.null.to.object"
                                       : "msg.undefined";
            throw NativeGlobal.constructError(
                        Context.getContext(), "ConversionError",
                        ScriptRuntime.getMessage0(msg),
                        scope);
        }
        Scriptable m = start;
        do {
            Object result = m.get(id, start);
            if (result != Scriptable.NOT_FOUND)
                return result;
            m = m.getPrototype();
        } while (m != null);
        return Undefined.instance;
    }

    public static Object getTopLevelProp(Scriptable scope, String id) {
        Scriptable s = ScriptableObject.getTopLevelScope(scope);
        Object v;
        do {
            v = s.get(id, s);
            if (v != Scriptable.NOT_FOUND)
                return v;
            s = s.getPrototype();
        } while (s != null);
        return v;
    }



/***********************************************************************/

    public static Scriptable getProto(Object obj, Scriptable scope) {
        Scriptable s;
        if (obj instanceof Scriptable) {
            s = (Scriptable) obj;
        } else {
            s = toObject(scope, obj);
        }
        if (s == null) {
            throw NativeGlobal.typeError0("msg.null.to.object", scope);
        }
        return s.getPrototype();
    }

    public static Scriptable getParent(Object obj) {
        Scriptable s;
        try {
            s = (Scriptable) obj;
        }
        catch (ClassCastException e) {
            return null;
        }
        if (s == null) {
            return null;
        }
        return getThis(s.getParentScope());
    }

   public static Scriptable getParent(Object obj, Scriptable scope) {
        Scriptable s;
        if (obj instanceof Scriptable) {
            s = (Scriptable) obj;
        } else {
            s = toObject(scope, obj);
        }
        if (s == null) {
            throw NativeGlobal.typeError0("msg.null.to.object", scope);
        }
        return s.getParentScope();
    }

    public static Object setProto(Object obj, Object value, Scriptable scope) {
        Scriptable start;
        if (obj instanceof Scriptable) {
            start = (Scriptable) obj;
        } else {
            start = toObject(scope, obj);
        }
        Scriptable result = value == null ? null : toObject(scope, value);
        Scriptable s = result;
        while (s != null) {
            if (s == start) {
                throw Context.reportRuntimeError1(
                    "msg.cyclic.value", "__proto__");
            }
            s = s.getPrototype();
        }
        if (start == null) {
            throw NativeGlobal.typeError0("msg.null.to.object", scope);
        }
        start.setPrototype(result);
        return result;
    }

    public static Object setParent(Object obj, Object value, Scriptable scope) {
        Scriptable start;
        if (obj instanceof Scriptable) {
            start = (Scriptable) obj;
        } else {
            start = toObject(scope, obj);
        }
        Scriptable result = value == null ? null : toObject(scope, value);
        Scriptable s = result;
        while (s != null) {
            if (s == start) {
                throw Context.reportRuntimeError1(
                    "msg.cyclic.value", "__parent__");
            }
            s = s.getParentScope();
        }
        if (start == null) {
            throw NativeGlobal.typeError0("msg.null.to.object", scope);
        }
        start.setParentScope(result);
        return result;
    }

    public static Object setProp(Object obj, String id, Object value,
                                 Scriptable scope)
    {
        Scriptable start;
        if (obj instanceof Scriptable) {
            start = (Scriptable) obj;
        } else {
            start = toObject(scope, obj);
        }
        if (start == null) {
            throw NativeGlobal.typeError0("msg.null.to.object", scope);
        }
        Scriptable m = start;
        do {
            if (m.has(id, start)) {
                m.put(id, start, value);
                return value;
            }
            m = m.getPrototype();
        } while (m != null);

        start.put(id, start, value);
        return value;
    }

    // Return -1L if str is not an index or the index value as lower 32 
    // bits of the result
    private static long indexFromString(String str) {
        // It must be a string.

        // The length of the decimal string representation of 
        //  Integer.MAX_VALUE, 2147483647
        final int MAX_VALUE_LENGTH = 10;
        
        int len = str.length();
        if (len > 0) {
            int i = 0;
            boolean negate = false;
            int c = str.charAt(0);
            if (c == '-') {
                if (len > 1) {
                    c = str.charAt(1); 
                    i = 1; 
                    negate = true;
                }
            }
            c -= '0';
            if (0 <= c && c <= 9 
                && len <= (negate ? MAX_VALUE_LENGTH + 1 : MAX_VALUE_LENGTH))
            {
                // Use negative numbers to accumulate index to handle
                // Integer.MIN_VALUE that is greater by 1 in absolute value
                // then Integer.MAX_VALUE
                int index = -c;
                int oldIndex = 0;
                i++;
                if (index != 0) {
                    // Note that 00, 01, 000 etc. are not indexes
                    while (i != len && 0 <= (c = str.charAt(i) - '0') && c <= 9)
                    {
                        oldIndex = index;
                        index = 10 * index - c;
                        i++;
                    }
                }
                // Make sure all characters were consumed and that it couldn't
                // have overflowed.
                if (i == len &&
                    (oldIndex > (Integer.MIN_VALUE / 10) ||
                     (oldIndex == (Integer.MIN_VALUE / 10) &&
                      c <= (negate ? -(Integer.MIN_VALUE % 10) 
                                   : (Integer.MAX_VALUE % 10)))))
                {
                    return 0xFFFFFFFFL & (negate ? index : -index);
                }
            }
        }
        return -1L;
    }

    static String getStringId(Object id) {
        if (id instanceof Number) {
            double d = ((Number) id).doubleValue();
            int index = (int) d;
            if (((double) index) == d)
                return null;
            return toString(id);
        }
        String s = toString(id);
        long indexTest = indexFromString(s);
        if (indexTest >= 0)
            return null;
        return s;
    }

    static int getIntId(Object id) {
        if (id instanceof Number) {
            double d = ((Number) id).doubleValue();
            int index = (int) d;
            if (((double) index) == d)
                return index;
            return 0;
        }
        String s = toString(id);
        long indexTest = indexFromString(s);
        if (indexTest >= 0)
            return (int)indexTest;
        return 0;
    }

    public static Object getElem(Object obj, Object id, Scriptable scope) {
        int index;
        String s;
        if (id instanceof Number) {
            double d = ((Number) id).doubleValue();
            index = (int) d;
            s = ((double) index) == d ? null : toString(id);
        } else {
            s = toString(id);
            long indexTest = indexFromString(s);
            if (indexTest >= 0) {
                index = (int)indexTest;
                s = null;
            } else {
                index = 0;
            }                
        }
        Scriptable start = obj instanceof Scriptable
                           ? (Scriptable) obj
                           : toObject(scope, obj);
        Scriptable m = start;
        if (s != null) {
            if (s.equals("__proto__"))
                return start.getPrototype();
            if (s.equals("__parent__"))
                return start.getParentScope();
            while (m != null) {
                Object result = m.get(s, start);
                if (result != Scriptable.NOT_FOUND)
                    return result;
                m = m.getPrototype();
            }
            return Undefined.instance;
        }
        while (m != null) {
            Object result = m.get(index, start);
            if (result != Scriptable.NOT_FOUND)
                return result;
            m = m.getPrototype();
        }
        return Undefined.instance;
    }


    /*
     * A cheaper and less general version of the above for well-known argument
     * types.
     */
    public static Object getElem(Scriptable obj, int index)
    {
        Scriptable m = obj;
        while (m != null) {
            Object result = m.get(index, obj);
            if (result != Scriptable.NOT_FOUND)
                return result;
            m = m.getPrototype();
        }
        return Undefined.instance;
    }

    public static Object setElem(Object obj, Object id, Object value,
                                 Scriptable scope)
    {
        int index;
        String s;
        if (id instanceof Number) {
            double d = ((Number) id).doubleValue();
            index = (int) d;
            s = ((double) index) == d ? null : toString(id);
        } else {
            s = toString(id);
            long indexTest = indexFromString(s);
            if (indexTest >= 0) {
                index = (int)indexTest;
                s = null;
            } else {
                index = 0;
            }
        }

        Scriptable start = obj instanceof Scriptable
                     ? (Scriptable) obj
                     : toObject(scope, obj);
        Scriptable m = start;
        if (s != null) {
            if (s.equals("__proto__"))
                return setProto(obj, value, scope);
            if (s.equals("__parent__"))
                return setParent(obj, value, scope);

            do {
                if (m.has(s, start)) {
                    m.put(s, start, value);
                    return value;
                }
                m = m.getPrototype();
            } while (m != null);
            start.put(s, start, value);
            return value;
       }

        do {
            if (m.has(index, start)) {
                m.put(index, start, value);
                return value;
            }
            m = m.getPrototype();
        } while (m != null);
        start.put(index, start, value);
        return value;
    }

    /*
     * A cheaper and less general version of the above for well-known argument
     * types.
     */
    public static Object setElem(Scriptable obj, int index, Object value)
    {
        Scriptable m = obj;
        do {
            if (m.has(index, obj)) {
                m.put(index, obj, value);
                return value;
            }
            m = m.getPrototype();
        } while (m != null);
        obj.put(index, obj, value);
        return value;
    }

    /**
     * The delete operator
     *
     * See ECMA 11.4.1
     *
     * In ECMA 0.19, the description of the delete operator (11.4.1)
     * assumes that the [[Delete]] method returns a value. However,
     * the definition of the [[Delete]] operator (8.6.2.5) does not
     * define a return value. Here we assume that the [[Delete]]
     * method doesn't return a value.
     */
    public static Object delete(Object obj, Object id) {
        String s = getStringId(id);
        boolean result = s != null
            ? ScriptableObject.deleteProperty((Scriptable) obj, s)
            : ScriptableObject.deleteProperty((Scriptable) obj, getIntId(id));
        return result ? Boolean.TRUE : Boolean.FALSE;
    }

    /**
     * Looks up a name in the scope chain and returns its value.
     */
    public static Object name(Scriptable scopeChain, String id) {
        Scriptable obj = scopeChain;
        Object prop;
        while (obj != null) {
            Scriptable m = obj;
            do {
                Object result = m.get(id, obj);
                if (result != Scriptable.NOT_FOUND)
                    return result;
                m = m.getPrototype();
            } while (m != null);
            obj = obj.getParentScope();
        }
        throw NativeGlobal.constructError
            (Context.getContext(), "ReferenceError",
             ScriptRuntime.getMessage1("msg.is.not.defined", id.toString()),
                        scopeChain);
    }

    /**
     * Returns the object in the scope chain that has a given property.
     *
     * The order of evaluation of an assignment expression involves
     * evaluating the lhs to a reference, evaluating the rhs, and then
     * modifying the reference with the rhs value. This method is used
     * to 'bind' the given name to an object containing that property
     * so that the side effects of evaluating the rhs do not affect
     * which property is modified.
     * Typically used in conjunction with setName.
     *
     * See ECMA 10.1.4
     */
    public static Scriptable bind(Scriptable scope, String id) {
        Scriptable obj = scope;
        Object prop;
        while (obj != null) {
            Scriptable m = obj;
            do {
                if (m.has(id, obj))
                    return obj;
                m = m.getPrototype();
            } while (m != null);
            obj = obj.getParentScope();
        }
        return null;
    }

    public static Scriptable getBase(Scriptable scope, String id) {
        Scriptable obj = scope;
        Object prop;
        while (obj != null) {
            Scriptable m = obj;
            do {
                if (m.get(id, obj) != Scriptable.NOT_FOUND)
                    return obj;
                m = m.getPrototype();
            } while (m != null);
            obj = obj.getParentScope();
        }
        throw NativeGlobal.constructError(
                    Context.getContext(), "ReferenceError",
                    ScriptRuntime.getMessage1("msg.is.not.defined", id),
                    scope);
    }

    public static Scriptable getThis(Scriptable base) {
        while (base instanceof NativeWith)
            base = base.getPrototype();
        if (base instanceof NativeCall)
            base = ScriptableObject.getTopLevelScope(base);
        return base;
    }

    public static Object setName(Scriptable bound, Object value,
                                 Scriptable scope, String id)
    {
        if (bound == null) {
            // "newname = 7;", where 'newname' has not yet
            // been defined, creates a new property in the
            // global object. Find the global object by
            // walking up the scope chain.
            Scriptable next = scope;
            do {
                bound = next;
                next = bound.getParentScope();
            } while (next != null);

            bound.put(id, bound, value);
            /*
            This code is causing immense performance problems in
            scripts that assign to the variables as a way of creating them.
            XXX need strict mode
            String message = getMessage1("msg.assn.create", id);
            Context.reportWarning(message);
            */
            return value;
        }
        return setProp(bound, id, value, scope);
    }

    public static Enumeration initEnum(Object value, Scriptable scope) {
        Scriptable m = toObject(scope, value);
        return new IdEnumeration(m);
    }

    public static Object nextEnum(Enumeration enum) {
        // OPT this could be more efficient; should junk the Enumeration
        // interface
        if (!enum.hasMoreElements())
            return null;
        return enum.nextElement();
    }

    // Form used by class files generated by 1.4R3 and earlier.
    public static Object call(Context cx, Object fun, Object thisArg,
                              Object[] args)
        throws JavaScriptException
    {
        Scriptable scope = null;
        if (fun instanceof Scriptable) 
            scope = ((Scriptable) fun).getParentScope();
        return call(cx, fun, thisArg, args, scope);
    }
    
    public static Object call(Context cx, Object fun, Object thisArg,
                              Object[] args, Scriptable scope)
        throws JavaScriptException
    {
        Function function;
        try {
            function = (Function) fun;
        }
        catch (ClassCastException e) {
            throw NativeGlobal.typeError1
                ("msg.isnt.function", toString(fun), scope);
        }

        Scriptable thisObj;
        if (thisArg instanceof Scriptable || thisArg == null) {
            thisObj = (Scriptable) thisArg;
        } else {
            thisObj = ScriptRuntime.toObject(scope, thisArg);
        }
        return function.call(cx, scope, thisObj, args);
    }

    private static Object callOrNewSpecial(Context cx, Scriptable scope,
                                           Object fun, Object jsThis, Object thisArg,
                                           Object[] args, boolean isCall,
                                           String filename, int lineNumber)
        throws JavaScriptException
    {
        if (fun instanceof IdFunction) {
            IdFunction f = (IdFunction)fun;
            String name = f.methodName;
            Class cl = f.master.getClass();
            if (name.equals("eval") && cl == NativeGlobal.class) {
                return NativeGlobal.evalSpecial(cx, scope, thisArg, args,
                                                filename, lineNumber);
            }
            if (name.equals("With") && cl == NativeWith.class) {
                return NativeWith.newWithSpecial(cx, args, f, !isCall);
            }
        }
        else if (fun instanceof FunctionObject) {
            FunctionObject fo = (FunctionObject) fun;
            Member m = fo.method;
            Class cl = m.getDeclaringClass();
            String name = m.getName();
            if (name.equals("jsFunction_exec") && cl == NativeScript.class)
                return ((NativeScript)jsThis).exec(cx, ScriptableObject.getTopLevelScope(scope));
            if (name.equals("exec")
                        && (cx.getRegExpProxy() != null)
                        && (cx.getRegExpProxy().isRegExp(jsThis)))
                return call(cx, fun, jsThis, args, scope);
        }
        else    // could've been <java>.XXX.exec() that was re-directed here
            if (fun instanceof NativeJavaMethod)
                return call(cx, fun, jsThis, args, scope);

        if (isCall)
            return call(cx, fun, thisArg, args, scope);
        return newObject(cx, fun, args, scope);
    }

    public static Object callSpecial(Context cx, Object fun,
                                     Object thisArg, Object[] args,
                                     Scriptable enclosingThisArg,
                                     Scriptable scope, String filename,
                                     int lineNumber)
        throws JavaScriptException
    {
        return callOrNewSpecial(cx, scope, fun, thisArg,
                                enclosingThisArg, args, true,
                                filename, lineNumber);
    }

    /**
     * Operator new.
     *
     * See ECMA 11.2.2
     */
    public static Scriptable newObject(Context cx, Object fun,
                                       Object[] args, Scriptable scope)
        throws JavaScriptException
    {
        Function f;
        try {
            f = (Function) fun;
            if (f != null) {
                return f.construct(cx, scope, args);
           }
            // else fall through to error
        } catch (ClassCastException e) {
            // fall through to error
        }
        throw NativeGlobal.typeError1
            ("msg.isnt.function", toString(fun), scope);
    }

    public static Scriptable newObjectSpecial(Context cx, Object fun,
                                              Object[] args, Scriptable scope)
        throws JavaScriptException
    {
        return (Scriptable) callOrNewSpecial(cx, scope, fun, null, null, args,
                                             false, null, -1);
    }

    /**
     * The typeof operator
     */
    public static String typeof(Object value) {
        if (value == Undefined.instance)
            return "undefined";
        if (value == null)
            return "object";
        if (value instanceof Scriptable)
            return (value instanceof Function) ? "function" : "object";
        if (value instanceof String)
            return "string";
        if (value instanceof Number)
            return "number";
        if (value instanceof Boolean)
            return "boolean";
        throw errorWithClassName("msg.invalid.type", value);
    }

    /**
     * The typeof operator that correctly handles the undefined case
     */
    public static String typeofName(Scriptable scope, String id) {
        Object val = bind(scope, id);
        if (val == null)
            return "undefined";
        return typeof(getProp(val, id, scope));
    }

    // neg:
    // implement the '-' operator inline in the caller
    // as "-toNumber(val)"

    // not:
    // implement the '!' operator inline in the caller
    // as "!toBoolean(val)"

    // bitnot:
    // implement the '~' operator inline in the caller
    // as "~toInt32(val)"

    public static Object add(Object val1, Object val2) {
        if (val1 instanceof Scriptable)
            val1 = ((Scriptable) val1).getDefaultValue(null);
        if (val2 instanceof Scriptable)
            val2 = ((Scriptable) val2).getDefaultValue(null);
        if (!(val1 instanceof String) && !(val2 instanceof String))
            if ((val1 instanceof Number) && (val2 instanceof Number))
                return new Double(((Number)val1).doubleValue() +
                                  ((Number)val2).doubleValue());
            else
                return new Double(toNumber(val1) + toNumber(val2));
        return toString(val1) + toString(val2);
    }

    public static Object postIncrement(Object value) {
        if (value instanceof Number)
            value = new Double(((Number)value).doubleValue() + 1.0);
        else
            value = new Double(toNumber(value) + 1.0);
        return value;
    }

    public static Object postIncrement(Scriptable scopeChain, String id) {
        Scriptable obj = scopeChain;
        Object prop;
        while (obj != null) {
            Scriptable m = obj;
            do {
                Object result = m.get(id, obj);
                if (result != Scriptable.NOT_FOUND) {
                    Object newValue = result;
                    if (newValue instanceof Number) {
                        newValue = new Double(
                                    ((Number)newValue).doubleValue() + 1.0);
                        m.put(id, obj, newValue);
                        return result;
                    }
                    else {
                        newValue = new Double(toNumber(newValue) + 1.0);
                        m.put(id, obj, newValue);
                        return new Double(toNumber(result));
                    }
                }
                m = m.getPrototype();
            } while (m != null);
            obj = obj.getParentScope();
        }
        throw NativeGlobal.constructError
            (Context.getContext(), "ReferenceError",
             ScriptRuntime.getMessage1("msg.is.not.defined", id),
                    scopeChain);
    }

    public static Object postIncrement(Object obj, String id, Scriptable scope) {
        Scriptable start;
        if (obj instanceof Scriptable) {
            start = (Scriptable) obj;
        } else {
            start = toObject(scope, obj);
        }
        if (start == null) {
            throw NativeGlobal.typeError0("msg.null.to.object", scope);
        }
        Scriptable m = start;
        do {
            Object result = m.get(id, start);
            if (result != Scriptable.NOT_FOUND) {
                Object newValue = result;
                if (newValue instanceof Number) {
                    newValue = new Double(
                                ((Number)newValue).doubleValue() + 1.0);
                    m.put(id, start, newValue);
                    return result;
                }
                else {
                    newValue = new Double(toNumber(newValue) + 1.0);
                    m.put(id, start, newValue);
                    return new Double(toNumber(result));
                }
            }
            m = m.getPrototype();
        } while (m != null);
        return Undefined.instance;
    }

    public static Object postIncrementElem(Object obj,
                                            Object index, Scriptable scope) {
        Object oldValue = getElem(obj, index, scope);
        if (oldValue == Undefined.instance)
            return Undefined.instance;
        double resultValue = toNumber(oldValue);
        Double newValue = new Double(resultValue + 1.0);
        setElem(obj, index, newValue, scope);
        return new Double(resultValue);
    }

    public static Object postDecrementElem(Object obj,
                                            Object index, Scriptable scope) {
        Object oldValue = getElem(obj, index, scope);
        if (oldValue == Undefined.instance)
            return Undefined.instance;
        double resultValue = toNumber(oldValue);
        Double newValue = new Double(resultValue - 1.0);
        setElem(obj, index, newValue, scope);
        return new Double(resultValue);
    }

    public static Object postDecrement(Object value) {
        if (value instanceof Number)
            value = new Double(((Number)value).doubleValue() - 1.0);
        else
            value = new Double(toNumber(value) - 1.0);
        return value;
    }

    public static Object postDecrement(Scriptable scopeChain, String id) {
        Scriptable obj = scopeChain;
        Object prop;
        while (obj != null) {
            Scriptable m = obj;
            do {
                Object result = m.get(id, obj);
                if (result != Scriptable.NOT_FOUND) {
                    Object newValue = result;
                    if (newValue instanceof Number) {
                        newValue = new Double(
                                    ((Number)newValue).doubleValue() - 1.0);
                        m.put(id, obj, newValue);
                        return result;
                    }
                    else {
                        newValue = new Double(toNumber(newValue) - 1.0);
                        m.put(id, obj, newValue);
                        return new Double(toNumber(result));
                    }
                }
                m = m.getPrototype();
            } while (m != null);
            obj = obj.getParentScope();
        }
        throw NativeGlobal.constructError
            (Context.getContext(), "ReferenceError",
             ScriptRuntime.getMessage1("msg.is.not.defined", id),
                    scopeChain);
    }

    public static Object postDecrement(Object obj, String id, Scriptable scope) {
        Scriptable start;
        if (obj instanceof Scriptable) {
            start = (Scriptable) obj;
        } else {
            start = toObject(scope, obj);
        }
        if (start == null) {
            throw NativeGlobal.typeError0("msg.null.to.object", scope);
        }
        Scriptable m = start;
        do {
            Object result = m.get(id, start);
            if (result != Scriptable.NOT_FOUND) {
                Object newValue = result;
                if (newValue instanceof Number) {
                    newValue = new Double(
                                ((Number)newValue).doubleValue() - 1.0);
                    m.put(id, start, newValue);
                    return result;
                }
                else {
                    newValue = new Double(toNumber(newValue) - 1.0);
                    m.put(id, start, newValue);
                    return new Double(toNumber(result));
                }
            }
            m = m.getPrototype();
        } while (m != null);
        return Undefined.instance;
    }

    public static Object toPrimitive(Object val) {
        if (val == null || !(val instanceof Scriptable)) {
            return val;
        }
        Object result = ((Scriptable) val).getDefaultValue(null);
        if (result != null && result instanceof Scriptable)
            throw NativeGlobal.typeError0("msg.bad.default.value", val);
        return result;
    }

    private static Class getTypeOfValue(Object obj) {
        if (obj == null)
            return ScriptableClass;
        if (obj == Undefined.instance)
            return UndefinedClass;
        if (obj instanceof Scriptable)
            return ScriptableClass;
        if (obj instanceof Number)
            return NumberClass;
        return obj.getClass();
    }

    /**
     * Equality
     *
     * See ECMA 11.9
     */
    public static boolean eq(Object x, Object y) {
        Object xCopy = x;                                       // !!! JIT bug in Cafe 2.1
        Object yCopy = y;                                       // need local copies, otherwise their values get blown below
        for (;;) {
            Class typeX = getTypeOfValue(x);
            Class typeY = getTypeOfValue(y);
            if (typeX == typeY) {
                if (typeX == UndefinedClass)
                    return true;
                if (typeX == NumberClass)
                    return ((Number) x).doubleValue() ==
                           ((Number) y).doubleValue();
                if (typeX == StringClass || typeX == BooleanClass)
                    return xCopy.equals(yCopy);                                 // !!! JIT bug in Cafe 2.1
                if (typeX == ScriptableClass) {
                    if (x == y)
                        return true;
                    if (x instanceof Wrapper &&
                        y instanceof Wrapper)
                    {
                        return ((Wrapper) x).unwrap() ==
                               ((Wrapper) y).unwrap();
                    }
                    return false;
                }
                throw new RuntimeException(); // shouldn't get here
            }
            if (x == null && y == Undefined.instance)
                return true;
            if (x == Undefined.instance && y == null)
                return true;
            if (typeX == NumberClass &&
                typeY == StringClass)
            {
                return ((Number) x).doubleValue() == toNumber(y);
            }
            if (typeX == StringClass &&
                typeY == NumberClass)
            {
                return toNumber(x) == ((Number) y).doubleValue();
            }
            if (typeX == BooleanClass) {
                x = new Double(toNumber(x));
                xCopy = x;                                 // !!! JIT bug in Cafe 2.1
                continue;
            }
            if (typeY == BooleanClass) {
                y = new Double(toNumber(y));
                yCopy = y;                                 // !!! JIT bug in Cafe 2.1
                continue;
            }
            if ((typeX == StringClass ||
                 typeX == NumberClass) &&
                typeY == ScriptableClass && y != null)
            {
                y = toPrimitive(y);
                yCopy = y;                                 // !!! JIT bug in Cafe 2.1
                continue;
            }
            if (typeX == ScriptableClass && x != null &&
                (typeY == StringClass ||
                 typeY == NumberClass))
            {
                x = toPrimitive(x);
                xCopy = x;                                 // !!! JIT bug in Cafe 2.1
                continue;
            }
            return false;
        }
    }

    public static Boolean eqB(Object x, Object y) {
        if (eq(x,y))
            return Boolean.TRUE;
        else
            return Boolean.FALSE;
    }

    public static Boolean neB(Object x, Object y) {
        if (eq(x,y))
            return Boolean.FALSE;
        else
            return Boolean.TRUE;
    }

    public static boolean shallowEq(Object x, Object y) {
        Class type = getTypeOfValue(x);
        if (type != getTypeOfValue(y))
            return false;
        if (type == StringClass || type == BooleanClass)
            return x.equals(y);
        if (type == NumberClass)
            return ((Number) x).doubleValue() ==
                   ((Number) y).doubleValue();
        if (type == ScriptableClass) {
            if (x == y)
                return true;
            if (x instanceof Wrapper && y instanceof Wrapper)
                return ((Wrapper) x).unwrap() ==
                       ((Wrapper) y).unwrap();
            return false;
        }
        if (type == UndefinedClass)
            return true;
        return false;
    }

    public static Boolean seqB(Object x, Object y) {
        if (shallowEq(x,y))
            return Boolean.TRUE;
        else
            return Boolean.FALSE;
    }

    public static Boolean sneB(Object x, Object y) {
        if (shallowEq(x,y))
            return Boolean.FALSE;
        else
            return Boolean.TRUE;
    }

    /**
     * The instanceof operator.
     *
     * @return a instanceof b
     */
    public static boolean instanceOf(Scriptable scope, Object a, Object b) {
        // Check RHS is an object
        if (! (b instanceof Scriptable)) {
            throw NativeGlobal.typeError0("msg.instanceof.not.object", scope);
        }

        // for primitive values on LHS, return false
        // XXX we may want to change this so that
        // 5 instanceof Number == true
        if (! (a instanceof Scriptable))
            return false;

        return ((Scriptable)b).hasInstance((Scriptable)a);
    }

    /**
     * Delegates to
     *
     * @return true iff rhs appears in lhs' proto chain
     */
    protected static boolean jsDelegatesTo(Scriptable lhs, Scriptable rhs) {
        Scriptable proto = lhs.getPrototype();

        while (proto != null) {
            if (proto.equals(rhs)) return true;
            proto = proto.getPrototype();
        }

        return false;
    }

    /**
     * The in operator.
     *
     * This is a new JS 1.3 language feature.  The in operator mirrors
     * the operation of the for .. in construct, and tests whether the
     * rhs has the property given by the lhs.  It is different from the 
     * for .. in construct in that:
     * <BR> - it doesn't perform ToObject on the right hand side
     * <BR> - it returns true for DontEnum properties.
     * @param a the left hand operand
     * @param b the right hand operand
     *
     * @return true if property name or element number a is a property of b
     */
    public static boolean in(Object a, Object b, Scriptable scope) {
        if (!(b instanceof Scriptable)) {
            throw NativeGlobal.typeError0("msg.instanceof.not.object", scope);
        }
        String s = getStringId(a);
        return s != null
            ? ScriptableObject.hasProperty((Scriptable) b, s)
            : ScriptableObject.hasProperty((Scriptable) b, getIntId(a));
    }

    public static Boolean cmp_LTB(Object val1, Object val2) {
        if (cmp_LT(val1, val2) == 1)
            return Boolean.TRUE;
        else
            return Boolean.FALSE;
    }

    public static int cmp_LT(Object val1, Object val2) {
        if (val1 instanceof Scriptable)
            val1 = ((Scriptable) val1).getDefaultValue(NumberClass);
        if (val2 instanceof Scriptable)
            val2 = ((Scriptable) val2).getDefaultValue(NumberClass);
        if (!(val1 instanceof String) || !(val2 instanceof String)) {
            double d1 = toNumber(val1);
            if (d1 != d1)
                return 0;
            double d2 = toNumber(val2);
            if (d2 != d2)
                return 0;
            return d1 < d2 ? 1 : 0;
        }
        return toString(val1).compareTo(toString(val2)) < 0 ? 1 : 0;
    }

    public static Boolean cmp_LEB(Object val1, Object val2) {
        if (cmp_LE(val1, val2) == 1)
            return Boolean.TRUE;
        else
            return Boolean.FALSE;
    }

    public static int cmp_LE(Object val1, Object val2) {
        if (val1 instanceof Scriptable)
            val1 = ((Scriptable) val1).getDefaultValue(NumberClass);
        if (val2 instanceof Scriptable)
            val2 = ((Scriptable) val2).getDefaultValue(NumberClass);
        if (!(val1 instanceof String) || !(val2 instanceof String)) {
            double d1 = toNumber(val1);
            if (d1 != d1)
                return 0;
            double d2 = toNumber(val2);
            if (d2 != d2)
                return 0;
            return d1 <= d2 ? 1 : 0;
        }
        return toString(val1).compareTo(toString(val2)) <= 0 ? 1 : 0;
    }

    // lt:
    // implement the '<' operator inline in the caller
    // as "compare(val1, val2) == 1"

    // le:
    // implement the '<=' operator inline in the caller
    // as "compare(val2, val1) == 0"

    // gt:
    // implement the '>' operator inline in the caller
    // as "compare(val2, val1) == 1"

    // ge:
    // implement the '>=' operator inline in the caller
    // as "compare(val1, val2) == 0"

    // ------------------
    // Statements
    // ------------------

    private static final String GLOBAL_CLASS = 
        "org.mozilla.javascript.tools.shell.Global";

    private static ScriptableObject getGlobal(Context cx) {
        try {
            Class globalClass = loadClassName(GLOBAL_CLASS);
            Class[] parm = { Context.class };
            Constructor globalClassCtor = globalClass.getConstructor(parm);
            Object[] arg = { cx };
            return (ScriptableObject) globalClassCtor.newInstance(arg);
        } catch (ClassNotFoundException e) {
            // fall through...
        } catch (NoSuchMethodException e) {
            // fall through...
        } catch (InvocationTargetException e) {
            // fall through...
        } catch (IllegalAccessException e) {
            // fall through...
        } catch (InstantiationException e) {
            // fall through...
        }
        return new ImporterTopLevel(cx);
    }

    public static void main(String scriptClassName, String[] args)
        throws JavaScriptException
    {
        Context cx = Context.enter();
        ScriptableObject global = getGlobal(cx);

        // get the command line arguments and define "arguments" 
        // array in the top-level object
        Scriptable argsObj = cx.newArray(global, args);
        global.defineProperty("arguments", argsObj,
                              ScriptableObject.DONTENUM);
        
        try {
            Class cl = loadClassName(scriptClassName);
            Script script = (Script) cl.newInstance();
            script.exec(cx, global);
            return;
        }
        catch (ClassNotFoundException e) {
        }
        catch (InstantiationException e) {
        }
        catch (IllegalAccessException e) {
        }
        finally {
            Context.exit();
        }
        throw new RuntimeException("Error creating script object");
    }

    public static Scriptable initScript(Context cx, Scriptable scope,
                                        NativeFunction funObj,
                                        Scriptable thisObj,
                                        boolean fromEvalCode)
    {
        String[] argNames = funObj.argNames;
        if (argNames != null) {
            ScriptableObject so;
            try {
                /* Global var definitions are supposed to be DONTDELETE
                 * so we try to create them that way by hoping that the
                 * scope is a ScriptableObject which provides access to
                 * setting the attributes.
                 */
                so = (ScriptableObject) scope;
            } catch (ClassCastException x) {
                // oh well, we tried.
                so = null;
            }

            Scriptable varScope = scope;
            if (fromEvalCode) {
                // When executing an eval() inside a with statement,
                // define any variables resulting from var statements
                // in the first non-with scope. See bug 38590.
                varScope = scope;
                while (varScope instanceof NativeWith)
                    varScope = varScope.getParentScope();
            }
            for (int i = argNames.length; i-- != 0;) {
                String name = argNames[i];
                // Don't overwrite existing def if already defined in object
                // or prototypes of object.
                if (!hasProp(scope, name)) {
                    if (so != null && !fromEvalCode)
                        so.defineProperty(name, Undefined.instance,
                                          ScriptableObject.PERMANENT);
                    else 
                        varScope.put(name, varScope, Undefined.instance);
                }
            }
        }

        return scope;
    }

    public static Scriptable runScript(Script script) {
        Context cx = Context.enter();
        ScriptableObject global = getGlobal(cx);
        try {
            script.exec(cx, global);
        } catch (JavaScriptException e) {
            throw new Error(e.toString());
        }
        Context.exit();
        return global;
    }

    public static Scriptable initVarObj(Context cx, Scriptable scope,
                                        NativeFunction funObj,
                                        Scriptable thisObj, Object[] args)
    {
        NativeCall result = new NativeCall(cx, scope, funObj, thisObj, args);
        String[] argNames = funObj.argNames;
        if (argNames != null) {
            for (int i = funObj.argCount; i != argNames.length; i++) {
                String name = argNames[i];
                result.put(name, result, Undefined.instance);
            }
        }
        return result;
    }

    public static void popActivation(Context cx) {
        NativeCall current = cx.currentActivation;
        if (current != null) {
            cx.currentActivation = current.caller;
            current.caller = null;
        }
    }

    public static Scriptable newScope() {
        return new NativeObject();
    }

    public static Scriptable enterWith(Object value, Scriptable scope) {
        return new NativeWith(scope, toObject(scope, value));
    }

    public static Scriptable leaveWith(Scriptable scope) {
        return scope.getParentScope();
    }

    public static NativeFunction initFunction(NativeFunction fn,
                                              Scriptable scope,
                                              String fnName,
                                              Context cx,
                                              boolean doSetName)
    {
        fn.setPrototype(ScriptableObject.getClassPrototype(scope, "Function"));
        fn.setParentScope(scope);
        if (doSetName)
            setName(scope, fn, scope, fnName);
        return fn;
    }

    public static NativeFunction createFunctionObject(Scriptable scope,
                                                      Class functionClass,
                                                      Context cx,
                                                      boolean setName)
    {
        Constructor[] ctors = functionClass.getConstructors();

        NativeFunction result = null;
        Object[] initArgs = { scope, cx };
        try {
            result = (NativeFunction) ctors[0].newInstance(initArgs);
        }
        catch (InstantiationException e) {
            throw WrappedException.wrapException(e);
        }
        catch (IllegalAccessException e) {
            throw WrappedException.wrapException(e);
        }
        catch (IllegalArgumentException e) {
            throw WrappedException.wrapException(e);
        }
        catch (InvocationTargetException e) {
            throw WrappedException.wrapException(e);
        }

        result.setPrototype(ScriptableObject.getClassPrototype(scope, "Function"));
        result.setParentScope(scope);

        String fnName = result.jsGet_name();
        if (setName && fnName != null && fnName.length() != 0 && 
            !fnName.equals("anonymous"))
        {
            setProp(scope, fnName, result, scope);
        }

         return result;
    }

    static void checkDeprecated(Context cx, String name) {
        int version = cx.getLanguageVersion();
        if (version >= Context.VERSION_1_4 || version == Context.VERSION_DEFAULT) {
            String msg = getMessage1("msg.deprec.ctor", name);
            if (version == Context.VERSION_DEFAULT)
                Context.reportWarning(msg);
            else
                throw Context.reportRuntimeError(msg);
        }
    }

    public static String getMessage0(String messageId) {
        return Context.getMessage0(messageId);
    }

    public static String getMessage1(String messageId, Object arg1) {
        return Context.getMessage1(messageId, arg1);
    }

    public static String getMessage2
        (String messageId, Object arg1, Object arg2) 
    {
        return Context.getMessage2(messageId, arg1, arg2);
    }

    public static String getMessage(String messageId, Object[] arguments) {
        return Context.getMessage(messageId, arguments);
    }

    public static RegExpProxy getRegExpProxy(Context cx) {
        return cx.getRegExpProxy();
    }

    public static NativeCall getCurrentActivation(Context cx) {
        return cx.currentActivation;
    }

    public static void setCurrentActivation(Context cx,
                                            NativeCall activation)
    {
        cx.currentActivation = activation;
    }

    private static Method getContextClassLoaderMethod;
    static {
        try {
            // We'd like to use "getContextClassLoader", but 
            // that's only available on Java2. 
            getContextClassLoaderMethod = 
                Thread.class.getDeclaredMethod("getContextClassLoader", 
                                               new Class[0]);
        } catch (NoSuchMethodException e) {
            // ignore exceptions; we'll use Class.forName instead.
        } catch (SecurityException e) {
            // ignore exceptions; we'll use Class.forName instead.
        }
    }
        
    public static Class loadClassName(String className) 
        throws ClassNotFoundException
    {
        try {
            if (getContextClassLoaderMethod != null) {
                ClassLoader cl = (ClassLoader) 
                                  getContextClassLoaderMethod.invoke(
                                    Thread.currentThread(), 
                                    new Object[0]);
                if (cl != null)
                    return cl.loadClass(className);
            } else {
                ClassLoader cl = ScriptRuntime.class.getClassLoader();
                if (cl != null)
                    return cl.loadClass(className);
            }
        } catch (SecurityException e) {
            // fall through...
        } catch (IllegalAccessException e) {
            // fall through...
        } catch (InvocationTargetException e) {
            // fall through...
        } catch (ClassNotFoundException e) {
            // Rather than just letting the exception propagate
            // we'll try Class.forName as well. The results could be
            // different if this class was loaded on a different
            // thread than the current thread.
            // So fall through...
        }
        return Class.forName(className);                
    }

    static boolean hasProp(Scriptable start, String name) {
        Scriptable m = start;
        do {
            if (m.has(name, start))
                return true;
            m = m.getPrototype();
        } while (m != null);
        return false;
    }

    private static RuntimeException errorWithClassName(String msg, Object val)
    {
        return Context.reportRuntimeError1(msg, val.getClass().getName());
    }

    public static final Object[] emptyArgs = new Object[0];

}


/**
 * This is the enumeration needed by the for..in statement.
 *
 * See ECMA 12.6.3.
 *
 * IdEnumeration maintains a Hashtable to make sure a given
 * id is enumerated only once across multiple objects in a
 * prototype chain.
 *
 * XXX - ECMA delete doesn't hide properties in the prototype,
 * but js/ref does. This means that the js/ref for..in can
 * avoid maintaining a hash table and instead perform lookups
 * to see if a given property has already been enumerated.
 *
 */
class IdEnumeration implements Enumeration {
    IdEnumeration(Scriptable m) {
        used = new Hashtable(27);
        changeObject(m);
        next = getNext();
    }

    public boolean hasMoreElements() {
        return next != null;
    }

    public Object nextElement() {
        Object result = next;

        // only key used; 'next' as value for convenience
        used.put(next, next);

        next = getNext();
        return result;
    }

    private void changeObject(Scriptable m) {
        obj = m;
        if (obj != null) {
            array = m.getIds();
            if (array.length == 0)
                changeObject(obj.getPrototype());
        }
        index = 0;
    }

    private Object getNext() {
        if (obj == null)
            return null;
        Object result;
        for (;;) {
            if (index == array.length) {
                changeObject(obj.getPrototype());
                if (obj == null)
                    return null;
            }
            result = array[index++];
            if (result instanceof String) {
                if (!obj.has((String) result, obj))
                    continue;   // must have been deleted
            } else {
                if (!obj.has(((Number) result).intValue(), obj))
                    continue;   // must have been deleted
            }
            if (!used.containsKey(result)) {
                break;
            }
        }
        return ScriptRuntime.toString(result);
    }

    private Object next;
    private Scriptable obj;
    private int index;
    private Object[] array;
    private Hashtable used;
}
