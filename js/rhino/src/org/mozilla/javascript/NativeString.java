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
 * Tom Beauvais
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
public class NativeString extends IdScriptable {

    /**
     * Zero-parameter constructor: just used to create String.prototype
     */
    public NativeString() {
        string = defaultValue;
    }

    public NativeString(String s) {
        string = s;
    }

    public String getClassName() {
        return "String";
    }

    protected void fillConstructorProperties
        (Context cx, IdFunction ctor, boolean sealed)
    {
        addIdFunctionProperty(ctor, ConstructorId_fromCharCode, sealed);
        super.fillConstructorProperties(cx, ctor, sealed);
    }

    protected int getIdDefaultAttributes(int id) {
        if (id == Id_length) {
            return DONTENUM | READONLY | PERMANENT;
        }
        return super.getIdDefaultAttributes(id);
    }

    protected Object getIdValue(int id, Scriptable start) {
        if (id == Id_length) {
            return wrap_int(realInstance(start).string.length());
        }
        return super.getIdValue(id, start);
    }

    private NativeString realInstance(Scriptable start) {
        for (; start != null; start = start.getPrototype()) {
            if (start instanceof NativeString) { return (NativeString)start; }
        }
        return this;
    }

    public int methodArity(int methodId) {
        switch (methodId) {
        case ConstructorId_fromCharCode:   return 1;

        case Id_constructor:               return 1;
        case Id_toString:                  return 0;
        case Id_valueOf:                   return 0;
        case Id_charAt:                    return 1;
        case Id_charCodeAt:                return 1;
        case Id_indexOf:                   return 2;
        case Id_lastIndexOf:               return 2;
        case Id_split:                     return 1;
        case Id_substring:                 return 2;
        case Id_toLowerCase:               return 0;
        case Id_toUpperCase:               return 0;
        case Id_substr:                    return 2;
        case Id_concat:                    return 1;
        case Id_slice:                     return 2;
        case Id_bold:                      return 0;
        case Id_italics:                   return 0;
        case Id_fixed:                     return 0;
        case Id_strike:                    return 0;
        case Id_small:                     return 0;
        case Id_big:                       return 0;
        case Id_blink:                     return 0;
        case Id_sup:                       return 0;
        case Id_sub:                       return 0;
        case Id_fontsize:                  return 0;
        case Id_fontcolor:                 return 0;
        case Id_link:                      return 0;
        case Id_anchor:                    return 0;
        case Id_equals:                    return 1;
        case Id_equalsIgnoreCase:          return 1;
        case Id_match:                     return 1;
        case Id_search:                    return 1;
        case Id_replace:                   return 1;
        }
        return super.methodArity(methodId);
    }

    public Object execMethod
        (int methodId, IdFunction f,
         Context cx, Scriptable scope, Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        switch (methodId) {

        case ConstructorId_fromCharCode: return
            jsStaticFunction_fromCharCode(args);

        case Id_constructor: return
            jsConstructor(args, thisObj == null);

        case Id_toString: return realThis(thisObj, f).
            jsFunction_toString();

        case Id_valueOf: return realThis(thisObj, f).
            jsFunction_valueOf();

        case Id_charAt: return
            jsFunction_charAt(ScriptRuntime.toString(thisObj), args);

        case Id_charCodeAt: return wrap_double(
            jsFunction_charCodeAt(ScriptRuntime.toString(thisObj), args));

        case Id_indexOf: return wrap_int(
            jsFunction_indexOf(ScriptRuntime.toString(thisObj), args));

        case Id_lastIndexOf: return wrap_int(
            jsFunction_lastIndexOf(ScriptRuntime.toString(thisObj), args));

        case Id_split: return
            jsFunction_split(cx, ScriptRuntime.toString(thisObj), args, f);

        case Id_substring: return
            jsFunction_substring(cx, ScriptRuntime.toString(thisObj), args);

        case Id_toLowerCase: return
            jsFunction_toLowerCase(ScriptRuntime.toString(thisObj));

        case Id_toUpperCase: return
            jsFunction_toUpperCase(ScriptRuntime.toString(thisObj));

        case Id_substr: return
            jsFunction_substr(ScriptRuntime.toString(thisObj), args);

        case Id_concat: return
            jsFunction_concat(ScriptRuntime.toString(thisObj), args);

        case Id_slice: return
            jsFunction_slice(ScriptRuntime.toString(thisObj), args);

        case Id_bold: return realThis(thisObj, f).
            jsFunction_bold();

        case Id_italics: return realThis(thisObj, f).
            jsFunction_italics();

        case Id_fixed: return realThis(thisObj, f).
            jsFunction_fixed();

        case Id_strike: return realThis(thisObj, f).
            jsFunction_strike();

        case Id_small: return realThis(thisObj, f).
            jsFunction_small();

        case Id_big: return realThis(thisObj, f).
            jsFunction_big();

        case Id_blink: return realThis(thisObj, f).
            jsFunction_blink();

        case Id_sup: return realThis(thisObj, f).
            jsFunction_sup();

        case Id_sub: return realThis(thisObj, f).
            jsFunction_sub();

        case Id_fontsize: return realThis(thisObj, f).
            jsFunction_fontsize(ScriptRuntime.toString(args, 0));

        case Id_fontcolor: return realThis(thisObj, f).
            jsFunction_fontcolor(ScriptRuntime.toString(args, 0));

        case Id_link: return realThis(thisObj, f).
            jsFunction_link(ScriptRuntime.toString(args, 0));

        case Id_anchor: return realThis(thisObj, f).
            jsFunction_anchor(ScriptRuntime.toString(args, 0));

        case Id_equals: return wrap_boolean(
            jsFunction_equals(ScriptRuntime.toString(thisObj),
                              ScriptRuntime.toString(args, 0)));

        case Id_equalsIgnoreCase: return wrap_boolean(
            jsFunction_equalsIgnoreCase(ScriptRuntime.toString(thisObj),
                                        ScriptRuntime.toString(args, 0)));

        case Id_match: return
            jsFunction_match(cx, thisObj, args, f);

        case Id_search: return
            jsFunction_search(cx, thisObj, args, f);

        case Id_replace: return
            jsFunction_replace(cx, thisObj, args, f);

        }

        return super.execMethod(methodId, f, cx, scope, thisObj, args);
    }

    private NativeString realThis(Scriptable thisObj, IdFunction f) {
        while (!(thisObj instanceof NativeString)) {
            thisObj = nextInstanceCheck(thisObj, f, true);
        }
        return (NativeString)thisObj;
    }

    private static String jsStaticFunction_fromCharCode(Object[] args) {
        int N = args.length;
        if (N < 1)
            return "";
        StringBuffer s = new java.lang.StringBuffer(N);
        for (int i=0; i < N; i++) {
            s.append(ScriptRuntime.toUint16(args[i]));
        }
        return s.toString();
    }

    private static Object jsConstructor(Object[] args, boolean inNewExpr) {
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
    private String jsFunction_toString() {
        return string;
    }

    private String jsFunction_valueOf() {
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

    /*
     *
     * See ECMA 15.5.4.[4,5]
     */
    private static String jsFunction_charAt(String target, Object[] args)
    {
        // this'll return 0 if undefined... seems
        // to be ECMA.
        double pos = ScriptRuntime.toInteger(args, 0);

        if (pos < 0 || pos >= target.length())
            return "";

        return target.substring((int)pos, (int)pos + 1);
    }

    private static double jsFunction_charCodeAt(String target, Object[] args)
    {
        double pos = ScriptRuntime.toInteger(args, 0);

        if (pos < 0 || pos >= target.length()) {
            return ScriptRuntime.NaN;
        }

        return target.charAt((int)pos);
    }

    /*
     *
     * See ECMA 15.5.4.6.  Uses Java String.indexOf()
     * OPT to add - BMH searching from jsstr.c.
     */
    private static int jsFunction_indexOf(String target, Object[] args) {
        String search = ScriptRuntime.toString(args, 0);
        double begin = ScriptRuntime.toInteger(args, 1);

        if (begin > target.length()) {
            return -1;
        } else {
            if (begin < 0)
                begin = 0;
            return target.indexOf(search, (int)begin);
        }
    }

    /*
     *
     * See ECMA 15.5.4.7
     *
     */
    private static int jsFunction_lastIndexOf(String target, Object[] args) {
        String search = ScriptRuntime.toString(args, 0);
        double end = ScriptRuntime.toNumber(args, 1);

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

    /*
     * See ECMA 15.5.4.8.  Modified to match JS 1.2 - optionally takes
     * a limit argument and accepts a regular expression as the split
     * argument.
     */
    private static Object jsFunction_split(Context cx, String target,
                                           Object[] args, Function funObj)
    {
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
        boolean limited = (args.length > 1) && (args[1] != Undefined.instance);
        long limit = 0;  // Initialize to avoid warning.
        if (limited) {
            /* Clamp limit between 0 and 1 + string length. */
            limit = ScriptRuntime.toUint32(args[1]);
            if (limit > target.length())
                limit = 1 + target.length();
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

    /*
     * See ECMA 15.5.4.15
     */
    private static String jsFunction_substring(Context cx, String target,
                                               Object[] args)
    {
        int length = target.length();
        double start = ScriptRuntime.toInteger(args, 0);
        double end;

        if (start < 0)
            start = 0;
        else if (start > length)
            start = length;

        if (args.length <= 1 || args[1] == Undefined.instance) {
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

    /*
     *
     * See ECMA 15.5.4.[11,12]
     */
    private static String jsFunction_toLowerCase(String target) {
        return target.toLowerCase();
    }

    private static String jsFunction_toUpperCase(String target) {
        return target.toUpperCase();
    }

    public double jsGet_length() {
        return (double) string.length();
    }

    /*
     * Non-ECMA methods.
     */
    private static String jsFunction_substr(String target, Object[] args) {
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

    /*
     * Python-esque sequence operations.
     */
    private static String jsFunction_concat(String target, Object[] args) {
        int N = args.length;
        if (N == 0) { return target; }

        StringBuffer result = new StringBuffer();
        result.append(target);

        for (int i = 0; i < N; i++)
            result.append(ScriptRuntime.toString(args[i]));

        return result.toString();
    }

    private static String jsFunction_slice(String target, Object[] args) {
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

    /*
     * HTML composition aids.
     */
    private String tagify(String begin, String end, String value) {
        StringBuffer result = new StringBuffer();
        result.append('<');
        result.append(begin);
        if (value != null) {
            result.append("=\"");
            result.append(value);
            result.append('"');
        }
        result.append('>');
        result.append(this.string);
        result.append("</");
        result.append((end == null) ? begin : end);
        result.append('>');
        return result.toString();
    }

    private String jsFunction_bold() {
        return tagify("B", null, null);
    }

    private String jsFunction_italics() {
        return tagify("I", null, null);
    }

    private String jsFunction_fixed() {
        return tagify("TT", null, null);
    }

    private String jsFunction_strike() {
        return tagify("STRIKE", null, null);
    }

    private String jsFunction_small() {
        return tagify("SMALL", null, null);
    }

    private String jsFunction_big() {
        return tagify("BIG", null, null);
    }

    private String jsFunction_blink() {
        return tagify("BLINK", null, null);
    }

    private String jsFunction_sup() {
        return tagify("SUP", null, null);
    }

    private String jsFunction_sub() {
        return tagify("SUB", null, null);
    }

    private String jsFunction_fontsize(String value) {
        return tagify("FONT SIZE", "FONT", value);
    }

    private String jsFunction_fontcolor(String value) {
        return tagify("FONT COLOR", "FONT", value);
    }

    private String jsFunction_link(String value) {
        return tagify("A HREF", "A", value);
    }

    private String jsFunction_anchor(String value) {
        return tagify("A NAME", "A", value);
    }

    private static boolean jsFunction_equals(String target, String strOther) {
        return target.equals(strOther);
    }


    private static boolean jsFunction_equalsIgnoreCase(String target,
                                                       String strOther)
    {
        return target.equalsIgnoreCase(strOther);
    }

    private static Object jsFunction_match(Context cx, Scriptable thisObj,
                                           Object[] args, Function funObj)
        throws JavaScriptException
    {
        return checkReProxy(cx).match(cx, thisObj, args, funObj);
    }

    private static Object jsFunction_search(Context cx, Scriptable thisObj,
                                            Object[] args, Function funObj)
        throws JavaScriptException
    {
        return checkReProxy(cx).search(cx, thisObj, args, funObj);
    }

    private static Object jsFunction_replace(Context cx, Scriptable thisObj,
                                             Object[] args, Function funObj)
        throws JavaScriptException
    {
        return checkReProxy(cx).replace(cx, thisObj, args, funObj);
    }

    private static RegExpProxy checkReProxy(Context cx) {
        RegExpProxy result = cx.getRegExpProxy();
        if (result == null) {
            throw cx.reportRuntimeError0("msg.no.regexp");
        }
        return result;
    }

    protected int getMinimumId() { return MIN_ID; }

    protected int getMaximumId() { return MAX_ID; }

    protected String getIdName(int id) {
        switch (id) {
        case ConstructorId_fromCharCode:   return "fromCharCode";

        case Id_constructor:               return "constructor";
        case Id_toString:                  return "toString";
        case Id_valueOf:                   return "valueOf";
        case Id_charAt:                    return "charAt";
        case Id_charCodeAt:                return "charCodeAt";
        case Id_indexOf:                   return "indexOf";
        case Id_lastIndexOf:               return "lastIndexOf";
        case Id_split:                     return "split";
        case Id_substring:                 return "substring";
        case Id_toLowerCase:               return "toLowerCase";
        case Id_toUpperCase:               return "toUpperCase";
        case Id_substr:                    return "substr";
        case Id_concat:                    return "concat";
        case Id_slice:                     return "slice";
        case Id_bold:                      return "bold";
        case Id_italics:                   return "italics";
        case Id_fixed:                     return "fixed";
        case Id_strike:                    return "strike";
        case Id_small:                     return "small";
        case Id_big:                       return "big";
        case Id_blink:                     return "blink";
        case Id_sup:                       return "sup";
        case Id_sub:                       return "sub";
        case Id_fontsize:                  return "fontsize";
        case Id_fontcolor:                 return "fontcolor";
        case Id_link:                      return "link";
        case Id_anchor:                    return "anchor";
        case Id_equals:                    return "equals";
        case Id_equalsIgnoreCase:          return "equalsIgnoreCase";
        case Id_match:                     return "match";
        case Id_search:                    return "search";
        case Id_replace:                   return "replace";

        case Id_length:                    return "length";
        }
        return null;
    }

// #string_id_map#

    protected int mapNameToId(String s) {
        int id;
// #generated# Last update: 2001-04-23 12:50:07 GMT+02:00
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 3: c=s.charAt(2);
                if (c=='b') { if (s.charAt(0)=='s' && s.charAt(1)=='u') {id=Id_sub; break L0;} }
                else if (c=='g') { if (s.charAt(0)=='b' && s.charAt(1)=='i') {id=Id_big; break L0;} }
                else if (c=='p') { if (s.charAt(0)=='s' && s.charAt(1)=='u') {id=Id_sup; break L0;} }
                break L;
            case 4: c=s.charAt(0);
                if (c=='b') { X="bold";id=Id_bold; }
                else if (c=='l') { X="link";id=Id_link; }
                break L;
            case 5: switch (s.charAt(4)) {
                case 'd': X="fixed";id=Id_fixed; break L;
                case 'e': X="slice";id=Id_slice; break L;
                case 'h': X="match";id=Id_match; break L;
                case 'k': X="blink";id=Id_blink; break L;
                case 'l': X="small";id=Id_small; break L;
                case 't': X="split";id=Id_split; break L;
                } break L;
            case 6: switch (s.charAt(1)) {
                case 'e': c=s.charAt(0);
                    if (c=='l') { X="length";id=Id_length; }
                    else if (c=='s') { X="search";id=Id_search; }
                    break L;
                case 'h': X="charAt";id=Id_charAt; break L;
                case 'n': X="anchor";id=Id_anchor; break L;
                case 'o': X="concat";id=Id_concat; break L;
                case 'q': X="equals";id=Id_equals; break L;
                case 't': X="strike";id=Id_strike; break L;
                case 'u': X="substr";id=Id_substr; break L;
                } break L;
            case 7: switch (s.charAt(1)) {
                case 'a': X="valueOf";id=Id_valueOf; break L;
                case 'e': X="replace";id=Id_replace; break L;
                case 'n': X="indexOf";id=Id_indexOf; break L;
                case 't': X="italics";id=Id_italics; break L;
                } break L;
            case 8: c=s.charAt(0);
                if (c=='f') { X="fontsize";id=Id_fontsize; }
                else if (c=='t') { X="toString";id=Id_toString; }
                break L;
            case 9: c=s.charAt(0);
                if (c=='f') { X="fontcolor";id=Id_fontcolor; }
                else if (c=='s') { X="substring";id=Id_substring; }
                break L;
            case 10: X="charCodeAt";id=Id_charCodeAt; break L;
            case 11: switch (s.charAt(2)) {
                case 'L': X="toLowerCase";id=Id_toLowerCase; break L;
                case 'U': X="toUpperCase";id=Id_toUpperCase; break L;
                case 'n': X="constructor";id=Id_constructor; break L;
                case 's': X="lastIndexOf";id=Id_lastIndexOf; break L;
                } break L;
            case 16: X="equalsIgnoreCase";id=Id_equalsIgnoreCase; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        MIN_ID                       = -1,

        ConstructorId_fromCharCode   = -1,

        Id_constructor               =  1,
        Id_toString                  =  2,
        Id_valueOf                   =  3,
        Id_charAt                    =  4,
        Id_charCodeAt                =  5,
        Id_indexOf                   =  6,
        Id_lastIndexOf               =  7,
        Id_split                     =  8,
        Id_substring                 =  9,
        Id_toLowerCase               = 10,
        Id_toUpperCase               = 11,
        Id_substr                    = 12,
        Id_concat                    = 13,
        Id_slice                     = 14,
        Id_bold                      = 15,
        Id_italics                   = 16,
        Id_fixed                     = 17,
        Id_strike                    = 18,
        Id_small                     = 19,
        Id_big                       = 20,
        Id_blink                     = 21,
        Id_sup                       = 22,
        Id_sub                       = 23,
        Id_fontsize                  = 24,
        Id_fontcolor                 = 25,
        Id_link                      = 26,
        Id_anchor                    = 27,
        Id_equals                    = 28,
        Id_equalsIgnoreCase          = 29,
        Id_match                     = 30,
        Id_search                    = 31,
        Id_replace                   = 32,

        Id_length                    = 33,

        MAX_ID                       = 33;

// #/string_id_map#

    private static final String defaultValue = "";

    private String string;
}

