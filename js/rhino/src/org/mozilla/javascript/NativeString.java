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

import java.lang.reflect.Method;
import java.util.Vector;

/**
 * This class implements the String native object.
 *
 * See ECMA 15.5.
 *
 * String methods for dealing with regular expressions are
 * ported directly from C. Latest port is from version 1.40.12.19
 * in the JSFUN13_BRANCH.
 *
 * @author Mike McCabe
 * @author Norris Boyd
 */
public class NativeString extends ScriptableObject implements Wrapper {

    /**
     * Zero-parameter constructor: just used to create String.prototype
     */
    public NativeString() {
        string = defaultValue;
    }

    public static void finishInit(Scriptable scope, FunctionObject ctor,
                                  Scriptable proto)
    {
        // Most of the methods of String.prototype are "vararg" form
        // so that they can convert the "this" value to string, rather
        // than being restricted to just operating on NativeString
        // objects. However, this means that the values of the "length"
        // properties of these methods are set to 1 by the constructor
        // for FunctionObject. We must therefore fetch the function
        // objects and set the length to the appropriate value.

        String[] specialLengthNames = { "indexOf",
                                        "lastIndexOf",
                                        "substring",
                                        "toUpperCase",
                                        "toLowerCase",
                                        "toString",
                                      };

        short[] specialLengthValues = { 2,
                                        2,
                                        2,
                                        0,
                                        0,
                                        0,
                                      };

        for (int i=0; i < specialLengthNames.length; i++) {
            Object obj = proto.get(specialLengthNames[i], proto);
            ((FunctionObject) obj).setLength(specialLengthValues[i]);
        }
    }

    public NativeString(String s) {
        string = s;
    }

    public String getClassName() {
        return "String";
    }

    public static String jsStaticFunction_fromCharCode(Context cx, 
                                                       Scriptable thisObj,
                                                       Object[] args, 
                                                       Function funObj)
    {
        StringBuffer s = new java.lang.StringBuffer();
        if (args.length < 1)
            return "";
        for (int i=0; i < args.length; i++) {
            s.append(ScriptRuntime.toUint16(args[i]));
        }
        return s.toString();
    }

    public static Object js_String(Context cx, Object[] args, Function ctorObj,
                                   boolean inNewExpr)
    {
        String s = args.length >= 1
            ? ScriptRuntime.toString(args[0])
            : defaultValue;
        if (inNewExpr) {
            // new String(val) creates a new String object.
            return new NativeString(s);
        }
        // String(val) converts val to a string value.
        return s;
    }

    public String toString() {
        return string;
    }

    /* ECMA 15.5.4.2: 'the toString function is not generic.' */
    public String js_toString() {
        return string;
    }

    public String js_valueOf() {
        return string;
    }

    /* Make array-style property lookup work for strings.
     * XXX is this ECMA?  A version check is probably needed. In js too.
     */
    public Object get(int index, Scriptable start) {
        if (index >= 0 && index < string.length())
            return string.substring(index, index + 1);
        return super.get(index, start);
    }

    public void put(int index, Scriptable start, Object value) {
        if (index >= 0 && index < string.length())
            return;
        super.put(index, start, value);
    }

    /**
     *
     * See ECMA 15.5.4.[4,5]
     */
    public static String js_charAt(Context cx, Scriptable thisObj,
                                   Object[] args, Function funObj)
    {
        if (args.length < 1)
            args = ScriptRuntime.padArguments(args, 1);

        // this'll return 0 if undefined... seems
        // to be ECMA.
        String target = ScriptRuntime.toString(thisObj);
        double pos = ScriptRuntime.toInteger(args[0]);

        if (pos < 0 || pos >= target.length())
            return "";

        return target.substring((int)pos, (int)pos + 1);
    }

    public static double js_charCodeAt(Context cx, Scriptable thisObj,
                                       Object[] args, Function funObj)
    {
        if (args.length < 1)
            args = ScriptRuntime.padArguments(args, 1);

        String target = ScriptRuntime.toString(thisObj);
        double pos = ScriptRuntime.toInteger(args[0]);

        if (pos < 0 || pos >= target.length()) {
            return ScriptRuntime.NaN;
        }

        return target.charAt((int)pos);
    }

    /**
     *
     * See ECMA 15.5.4.6.  Uses Java String.indexOf()
     * OPT to add - BMH searching from jsstr.c.
     */
    public static int js_indexOf(Context cx, Scriptable thisObj,
                                 Object[] args, Function funObj)
    {
        if (args.length < 2)
            args = ScriptRuntime.padArguments(args, 2);

        String target = ScriptRuntime.toString(thisObj);
        String search = ScriptRuntime.toString(args[0]);
        double begin = ScriptRuntime.toInteger(args[1]);

        if (begin > target.length()) {
            return -1;
        } else {
            if (begin < 0)
                begin = 0;
            return target.indexOf(search, (int)begin);
        }
    }

    /**
     *
     * See ECMA 15.5.4.7
     *
     */
    public static int js_lastIndexOf(Context cx, Scriptable thisObj,
                                     Object[] args, Function funObj)
    {
        if (args.length < 2)
            args = ScriptRuntime.padArguments(args, 2);

        String target = ScriptRuntime.toString(thisObj);
        String search = ScriptRuntime.toString(args[0]);
        double end = ScriptRuntime.toNumber(args[1]);

        if (end != end || end > target.length())
            end = target.length();
        else if (end < 0)
            end = 0;

        return target.lastIndexOf(search, (int)end);
    }

    /*
     * Used by js_split to find the next split point in target,
     * starting at offset ip and looking either for the given
     * separator substring, or for the next re match.  ip and
     * matchlen must be reference variables (assumed to be arrays of
     * length 1) so they can be updated in the leading whitespace or
     * re case.
     *
     * Return -1 on end of string, >= 0 for a valid index of the next
     * separator occurrence if found, or the string length if no
     * separator is found.
     */
    private static int find_split(Function funObj, String target, 
                                  String separator, Object re, 
                                  int[] ip, int[] matchlen, boolean[] matched,
                                  String[][] parensp)
    {
        int i = ip[0];
        int length = target.length();
        Context cx = Context.getContext();
        int version = cx.getLanguageVersion();

        /*
         * Perl4 special case for str.split(' '), only if the user has selected
         * JavaScript1.2 explicitly.  Split on whitespace, and skip leading w/s.
         * Strange but true, apparently modeled after awk.
         */
        if (version == Context.VERSION_1_2 &&
            re == null && separator.length() == 1 && separator.charAt(0) == ' ')
        {
            /* Skip leading whitespace if at front of str. */
            if (i == 0) {
                while (i < length && Character.isWhitespace(target.charAt(i)))
                    i++;
                ip[0] = i;
            }

            /* Don't delimit whitespace at end of string. */
            if (i == length)
                return -1;

            /* Skip over the non-whitespace chars. */
            while (i < length
                   && !Character.isWhitespace(target.charAt(i)))
                i++;

            /* Now skip the next run of whitespace. */
            int j = i;
            while (j < length && Character.isWhitespace(target.charAt(j)))
                j++;

            /* Update matchlen to count delimiter chars. */
            matchlen[0] = j - i;
            return i;
        }

        /*
         * Stop if past end of string.  If at end of string, we will
         * return target length, so that
         *
         *  "ab,".split(',') => new Array("ab", "")
         *
         * and the resulting array converts back to the string "ab,"
         * for symmetry.  NB: This differs from perl, which drops the
         * trailing empty substring if the LIMIT argument is omitted.
         */
        if (i > length)
            return -1;

        /*
         * Match a regular expression against the separator at or
         * above index i.  Return -1 at end of string instead of
         * trying for a match, so we don't get stuck in a loop.
         */
        if (re != null) {
            return cx.getRegExpProxy().find_split(funObj, target, 
                                                  separator, re, 
                                                  ip, matchlen, matched,
                                                  parensp);
        }

        /*
         * Deviate from ECMA by never splitting an empty string by any separator
         * string into a non-empty array (an array of length 1 that contains the
         * empty string).
         */
        if (version != Context.VERSION_DEFAULT && version < Context.VERSION_1_3
            && length == 0)
            return -1;

        /*
         * Special case: if sep is the empty string, split str into
         * one character substrings.  Let our caller worry about
         * whether to split once at end of string into an empty
         * substring.
         *
         * For 1.2 compatibility, at the end of the string, we return the length as
         * the result, and set the separator length to 1 -- this allows the caller
         * to include an additional null string at the end of the substring list.
         */
        if (separator.length() == 0) {
            if (version == Context.VERSION_1_2) {
                if (i == length) {
                    matchlen[0] = 1;
                    return i;
                }
    	        return i + 1;
            }
    	    return (i == length) ? -1 : i + 1;
        }

        /* Punt to j.l.s.indexOf; return target length if seperator is
         * not found.
         */
        if (ip[0] >= length)
            return length;

        i = target.indexOf(separator, ip[0]);

        return (i != -1) ? i : length;
    }

    /**
     * See ECMA 15.5.4.8.  Modified to match JS 1.2 - optionally takes
     * a limit argument and accepts a regular expression as the split
     * argument.
     */
    public static Object js_split(Context cx, Scriptable thisObj,
                                  Object[] args, Function funObj)
    {
        String target = ScriptRuntime.toString(thisObj);

        // create an empty Array to return;
        Scriptable scope = getTopLevelScope(funObj);
        Scriptable result = ScriptRuntime.newObject(cx, scope, "Array", null);

        // return an array consisting of the target if no separator given
        // don't check against undefined, because we want
        // 'fooundefinedbar'.split(void 0) to split to ['foo', 'bar']
        if (args.length < 1) {
            result.put(0, result, target);
            return result;
        }

        // Use the second argument as the split limit, if given.
        boolean limited = (args.length > 1);
        int limit = 0;  // Initialize to avoid warning.
        if (limited) {
            /* Clamp limit between 0 and 1 + string length. */
            double d = ScriptRuntime.toInteger(args[1]);
            if (d < 0)
                d = 0;
            else if (d > target.length())
                d = 1 + target.length();
            limit = (int)d;
        }

        String separator = null;
        int[] matchlen = { 0 };
        Object re = null;
        RegExpProxy reProxy = cx.getRegExpProxy();
        if (reProxy != null && reProxy.isRegExp(args[0])) {
            re = args[0];
        } else {
            separator = ScriptRuntime.toString(args[0]);
            matchlen[0] = separator.length();
        }

        // split target with separator or re
        int[] ip = { 0 };
        int match;
        int len = 0;
        boolean[] matched = { false };
        String[][] parens = { null };
        while ((match = find_split(funObj, target, separator, re, ip, 
                                   matchlen, matched, parens)) >= 0)
        {
            if ((limited && len >= limit) || (match > target.length()))
                break;

            String substr;
            if (target.length() == 0)
                substr = target;
            else
                substr = target.substring(ip[0], match);

            result.put(len, result, substr);
            len++;
	    /*
	     * Imitate perl's feature of including parenthesized substrings
	     * that matched part of the delimiter in the new array, after the
	     * split substring that was delimited.
	     */
            if (re != null && matched[0] == true) {
                int size = parens[0].length;
                for (int num = 0; num < size; num++) {
                    if (limited && len >= limit)
                        break;
                    result.put(len, result, parens[0][num]);
                    len++;
                }
                matched[0] = false;
            }            
            ip[0] = match + matchlen[0];

            if (cx.getLanguageVersion() < Context.VERSION_1_3
                && cx.getLanguageVersion() != Context.VERSION_DEFAULT)
            {
		/*
		 * Deviate from ECMA to imitate Perl, which omits a final
		 * split unless a limit argument is given and big enough.
		 */
                if (!limited && ip[0] == target.length())
                    break;
            }
        }
        return result;
    }

    /**
     *
     * See ECMA 15.5.4.[9,10]
     */
    public static String js_substring(Context cx, Scriptable thisObj,
                                      Object[] args, Function funObj)
    {
        if (args.length < 1)
            args = ScriptRuntime.padArguments(args, 1);

        String target = ScriptRuntime.toString(thisObj);
        int length = target.length();
        double start = ScriptRuntime.toInteger(args[0]);
        double end;

	if (start < 0)
	    start = 0;
	else if (start > length)
	    start = length;

	if (args.length == 1) {
	    end = length;
	} else {
            end = ScriptRuntime.toInteger(args[1]);
	    if (end < 0)
		end = 0;
	    else if (end > length)
		end = length;

            // swap if end < start
            if (end < start) {
                if (cx.getLanguageVersion() != Context.VERSION_1_2) {
                    double temp = start;
                    start = end;
                    end = temp;
                } else {
                    // Emulate old JDK1.0 java.lang.String.substring()
                    end = start;
                }
            }
	}
        return target.substring((int)start, (int)end);
    }

    /**
     *
     * See ECMA 15.5.4.[11,12]
     */
    public static String js_toLowerCase(Context cx, Scriptable thisObj,
                                        Object[] args, Function funObj)
    {
        String target = ScriptRuntime.toString(thisObj);
        return target.toLowerCase();
    }

    public static String js_toUpperCase(Context cx, Scriptable thisObj,
                                        Object[] args, Function funObj)
    {
        String target = ScriptRuntime.toString(thisObj);
        return target.toUpperCase();
    }

    public double js_getLength() {
        return (double) string.length();
    }

    /**
     * Non-ECMA methods.
     */
    public static String js_substr(Context cx, Scriptable thisObj,
                                   Object[] args, Function funObj)
    {
        String target = ScriptRuntime.toString(thisObj);

        if (args.length < 1)
            return target;

        double begin = ScriptRuntime.toInteger(args[0]);
        double end;
        int length = target.length();

        if (begin < 0) {
            begin += length;
            if (begin < 0)
                begin = 0;
        } else if (begin > length) {
            begin = length;
        }

        if (args.length == 1) {
            end = length;
        } else {
            end = ScriptRuntime.toInteger(args[1]);
            if (end < 0)
                end = 0;
            end += begin;
            if (end > length)
                end = length;
        }

        return target.substring((int)begin, (int)end);
    }

    /**
     * Python-esque sequence operations.
     */
    public static String js_concat(Context cx, Scriptable thisObj,
                                   Object[] args, Function funObj)
    {
        StringBuffer result = new StringBuffer();
        result.append(ScriptRuntime.toString(thisObj));

        for (int i = 0; i < args.length; i++)
            result.append(ScriptRuntime.toString(args[i]));

        return result.toString();
    }

    public static String js_slice(Context cx, Scriptable thisObj,
                                  Object[] args, Function funObj)
    {
        String target = ScriptRuntime.toString(thisObj);

        if (args.length != 0) {
            double begin = ScriptRuntime.toInteger(args[0]);
            double end;
            int length = target.length();
            if (begin < 0) {
                begin += length;
                if (begin < 0)
                    begin = 0;
            } else if (begin > length) {
                begin = length;
            }

            if (args.length == 1) {
                end = length;
            } else {
                end = ScriptRuntime.toInteger(args[1]);
                if (end < 0) {
                    end += length;
                    if (end < 0)
                        end = 0;
                } else if (end > length) {
                    end = length;
                }
                if (end < begin)
                    end = begin;
            }
            return target.substring((int)begin, (int)end);
        }
        return target;
    }

    /**
     * HTML composition aids.
     */
    private String tagify(String begin, String end, String value) {
        StringBuffer result = new StringBuffer();
        result.append('<');
        result.append(begin);
        if (value != null) {
            result.append('=');
            result.append(value);
        }
        result.append('>');
        result.append(this.string);
        result.append("</");
        result.append((end == null) ? begin : end);
        result.append('>');
        return result.toString();
    }

    public String js_bold() {
        return tagify("B", null, null);
    }

    public String js_italics() {
        return tagify("I", null, null);
    }

    public String js_fixed() {
        return tagify("TT", null, null);
    }

    public String js_strike() {
        return tagify("STRIKE", null, null);
    }

    public String js_small() {
        return tagify("SMALL", null, null);
    }

    public String js_big() {
        return tagify("BIG", null, null);
    }

    public String js_blink() {
        return tagify("BLINK", null, null);
    }

    public String js_sup() {
        return tagify("SUP", null, null);
    }

    public String js_sub() {
        return tagify("SUB", null, null);
    }

    public String js_fontsize(String value) {
        return tagify("FONT SIZE", "FONT", value);
    }

    public String js_fontcolor(String value) {
        return tagify("FONT COLOR", "FONT", value);
    }

    public String js_link(String value) {
        return tagify("A HREF", "A", value);
    }

    public String js_anchor(String value) {
        return tagify("A NAME", "A", value);
    }

    /**
     * Unwrap this NativeString as a j.l.String for LiveConnect use.
     */

    public Object unwrap() {
        return string;
    }

    public static Object js_match(Context cx, Scriptable thisObj,
                                  Object[] args, Function funObj)
        throws JavaScriptException
    {
        return checkReProxy(cx).match(cx, thisObj, args, funObj);
    }

    public static Object js_search(Context cx, Scriptable thisObj,
                                   Object[] args, Function funObj)
        throws JavaScriptException
    {
        return checkReProxy(cx).search(cx, thisObj, args, funObj);
    }

    public static Object js_replace(Context cx, Scriptable thisObj,
                                    Object[] args, Function funObj)
        throws JavaScriptException
    {
        return checkReProxy(cx).replace(cx, thisObj, args, funObj);
    }
    
    private static RegExpProxy checkReProxy(Context cx) {
        RegExpProxy result = cx.getRegExpProxy();
        if (result == null) {
            throw cx.reportRuntimeError(cx.getMessage("msg.no.regexp", null));
        }
        return result;
    }

    private static final String defaultValue = "";

    private String string;
}

