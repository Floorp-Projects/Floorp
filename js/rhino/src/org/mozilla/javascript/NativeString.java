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
final class NativeString extends IdScriptable {

    private static final Object STRING_TAG = new Object();

    static void init(Context cx, Scriptable scope, boolean sealed)
    {
        NativeString obj = new NativeString("");
        obj.exportAsJSClass(MAX_PROTOTYPE_ID, scope, sealed);
    }

    private NativeString(String s) {
        string = s;
    }

    public String getClassName() {
        return "String";
    }

    private static final int
        Id_length                    =  1,
        MAX_INSTANCE_ID              =  1;

    {
        setMaxInstanceId(0, MAX_INSTANCE_ID);
    }

    protected int findInstanceIdInfo(String s)
    {
        if (s.equals("length")) {
            return instanceIdInfo(DONTENUM | READONLY | PERMANENT, Id_length);
        }
        return super.findInstanceIdInfo(s);
    }

    protected String getInstanceIdName(int id)
    {
        if (id == Id_length) { return "length"; }
        return super.getInstanceIdName(id);
    }

    protected Object getInstanceIdValue(int id)
    {
        if (id == Id_length) {
            return wrap_int(string.length());
        }
        return super.getInstanceIdValue(id);
    }

    protected void fillConstructorProperties(IdFunction ctor)
    {
        addIdFunctionProperty(ctor, STRING_TAG, ConstructorId_fromCharCode,
                              "fromCharCode", 1);
        super.fillConstructorProperties(ctor);
    }

    protected void initPrototypeId(int id)
    {
        String s;
        int arity;
        switch (id) {
          case Id_constructor:      arity=1; s="constructor";      break;
          case Id_toString:         arity=0; s="toString";         break;
          case Id_toSource:         arity=0; s="toSource";         break;
          case Id_valueOf:          arity=0; s="valueOf";          break;
          case Id_charAt:           arity=1; s="charAt";           break;
          case Id_charCodeAt:       arity=1; s="charCodeAt";       break;
          case Id_indexOf:          arity=1; s="indexOf";          break;
          case Id_lastIndexOf:      arity=1; s="lastIndexOf";      break;
          case Id_split:            arity=2; s="split";            break;
          case Id_substring:        arity=2; s="substring";        break;
          case Id_toLowerCase:      arity=0; s="toLowerCase";      break;
          case Id_toUpperCase:      arity=0; s="toUpperCase";      break;
          case Id_substr:           arity=2; s="substr";           break;
          case Id_concat:           arity=1; s="concat";           break;
          case Id_slice:            arity=2; s="slice";            break;
          case Id_bold:             arity=0; s="bold";             break;
          case Id_italics:          arity=0; s="italics";          break;
          case Id_fixed:            arity=0; s="fixed";            break;
          case Id_strike:           arity=0; s="strike";           break;
          case Id_small:            arity=0; s="small";            break;
          case Id_big:              arity=0; s="big";              break;
          case Id_blink:            arity=0; s="blink";            break;
          case Id_sup:              arity=0; s="sup";              break;
          case Id_sub:              arity=0; s="sub";              break;
          case Id_fontsize:         arity=0; s="fontsize";         break;
          case Id_fontcolor:        arity=0; s="fontcolor";        break;
          case Id_link:             arity=0; s="link";             break;
          case Id_anchor:           arity=0; s="anchor";           break;
          case Id_equals:           arity=1; s="equals";           break;
          case Id_equalsIgnoreCase: arity=1; s="equalsIgnoreCase"; break;
          case Id_match:            arity=1; s="match";            break;
          case Id_search:           arity=1; s="search";           break;
          case Id_replace:          arity=1; s="replace";          break;
          default: throw new IllegalArgumentException(String.valueOf(id));
        }
        initPrototypeMethod(STRING_TAG, id, s, arity);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (!f.hasTag(STRING_TAG)) {
            return super.execMethod(f, cx, scope, thisObj, args);
        }
        int id = f.methodId();
        switch (id) {
          case ConstructorId_fromCharCode: {
            int N = args.length;
            if (N < 1)
                return "";
            StringBuffer sb = new StringBuffer(N);
            for (int i = 0; i != N; ++i) {
                sb.append(ScriptRuntime.toUint16(args[i]));
            }
            return sb.toString();
          }

          case Id_constructor: {
            String s = (args.length >= 1)
                ? ScriptRuntime.toString(args[0]) : "";
            if (thisObj == null) {
                // new String(val) creates a new String object.
                return new NativeString(s);
            }
            // String(val) converts val to a string value.
            return s;
          }

          case Id_toString:
          case Id_valueOf:
            // ECMA 15.5.4.2: 'the toString function is not generic.
            return realThis(thisObj, f).string;

          case Id_toSource: {
            String s = realThis(thisObj, f).string;
            return "(new String(\""+ScriptRuntime.escapeString(s)+"\"))";
          }

          case Id_charAt:
          case Id_charCodeAt: {
             // See ECMA 15.5.4.[4,5]
            String target = ScriptRuntime.toString(thisObj);
            double pos = ScriptRuntime.toInteger(args, 0);
            if (pos < 0 || pos >= target.length()) {
                if (id == Id_charAt) return "";
                else return ScriptRuntime.NaNobj;
            }
            char c = target.charAt((int)pos);
            if (id == Id_charAt) return String.valueOf(c);
            else return wrap_int(c);
          }

          case Id_indexOf:
            return wrap_int(js_indexOf(
                ScriptRuntime.toString(thisObj), args));

          case Id_lastIndexOf:
            return wrap_int(js_lastIndexOf(
                ScriptRuntime.toString(thisObj), args));

          case Id_split:
            return js_split(cx, scope, ScriptRuntime.toString(thisObj), args);

          case Id_substring:
            return js_substring(cx, ScriptRuntime.toString(thisObj), args);

          case Id_toLowerCase:
            // See ECMA 15.5.4.11
            return ScriptRuntime.toString(thisObj).toLowerCase();

          case Id_toUpperCase:
            // See ECMA 15.5.4.12
            return ScriptRuntime.toString(thisObj).toUpperCase();

          case Id_substr:
            return js_substr(ScriptRuntime.toString(thisObj), args);

          case Id_concat:
            return js_concat(ScriptRuntime.toString(thisObj), args);

          case Id_slice:
            return js_slice(ScriptRuntime.toString(thisObj), args);

          case Id_bold:
            return tagify(thisObj, "b", null, null);

          case Id_italics:
            return tagify(thisObj, "i", null, null);

          case Id_fixed:
            return tagify(thisObj, "tt", null, null);

          case Id_strike:
            return tagify(thisObj, "strike", null, null);

          case Id_small:
            return tagify(thisObj, "small", null, null);

          case Id_big:
            return tagify(thisObj, "big", null, null);

          case Id_blink:
            return tagify(thisObj, "blink", null, null);

          case Id_sup:
            return tagify(thisObj, "sup", null, null);

          case Id_sub:
            return tagify(thisObj, "sub", null, null);

          case Id_fontsize:
            return tagify(thisObj, "font", "size", args);

          case Id_fontcolor:
            return tagify(thisObj, "font", "color", args);

          case Id_link:
            return tagify(thisObj, "a", "href", args);

          case Id_anchor:
            return tagify(thisObj, "a", "name", args);

          case Id_equals:
          case Id_equalsIgnoreCase: {
            String s1 = ScriptRuntime.toString(thisObj);
            String s2 = ScriptRuntime.toString(args, 0);
            return wrap_boolean(id == Id_equals
                ? s1.equals(s2) : s1.equalsIgnoreCase(s2));
          }

          case Id_match:
            return ScriptRuntime.checkRegExpProxy(cx).
                match(cx, scope, thisObj, args);

          case Id_search:
            return ScriptRuntime.checkRegExpProxy(cx).
                search(cx, scope, thisObj, args);

          case Id_replace:
            return ScriptRuntime.checkRegExpProxy(cx).
                replace(cx, scope, thisObj, args);
        }
        throw new IllegalArgumentException(String.valueOf(id));
    }

    private static NativeString realThis(Scriptable thisObj, IdFunction f)
    {
        if (!(thisObj instanceof NativeString))
            throw incompatibleCallError(f);
        return (NativeString)thisObj;
    }

    /*
     * HTML composition aids.
     */
    private static String tagify(Object thisObj, String tag,
                                 String attribute, Object[] args)
    {
        String str = ScriptRuntime.toString(thisObj);
        StringBuffer result = new StringBuffer();
        result.append('<');
        result.append(tag);
        if (attribute != null) {
            result.append(' ');
            result.append(attribute);
            result.append("=\"");
            result.append(ScriptRuntime.toString(args, 0));
            result.append('"');
        }
        result.append('>');
        result.append(str);
        result.append("</");
        result.append(tag);
        result.append('>');
        return result.toString();
    }

    public String toString() {
        return string;
    }

    /* Make array-style property lookup work for strings.
     * XXX is this ECMA?  A version check is probably needed. In js too.
     */
    public Object get(int index, Scriptable start) {
        if (0 <= index && index < string.length()) {
            return string.substring(index, index + 1);
        }
        return super.get(index, start);
    }

    public void put(int index, Scriptable start, Object value) {
        if (0 <= index && index < string.length()) {
            return;
        }
        super.put(index, start, value);
    }

    /*
     *
     * See ECMA 15.5.4.6.  Uses Java String.indexOf()
     * OPT to add - BMH searching from jsstr.c.
     */
    private static int js_indexOf(String target, Object[] args) {
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
    private static int js_lastIndexOf(String target, Object[] args) {
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
    private static int find_split(Context cx, Scriptable scope, String target,
                                  String separator, int version,
                                  RegExpProxy reProxy, Scriptable re,
                                  int[] ip, int[] matchlen, boolean[] matched,
                                  String[][] parensp)
    {
        int i = ip[0];
        int length = target.length();

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
            return reProxy.find_split(cx, scope, target, separator, re,
                                      ip, matchlen, matched, parensp);
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
    private static Object js_split(Context cx, Scriptable scope,
                                   String target, Object[] args)
    {
        // create an empty Array to return;
        Scriptable top = getTopLevelScope(scope);
        Scriptable result = ScriptRuntime.newObject(cx, top, "Array", null);

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
        int[] matchlen = new int[1];
        Scriptable re = null;
        RegExpProxy reProxy = null;
        if (args[0] instanceof Scriptable) {
            reProxy = ScriptRuntime.getRegExpProxy(cx);
            if (reProxy != null) {
                Scriptable test = (Scriptable)args[0];
                if (reProxy.isRegExp(test)) {
                    re = test;
                }
            }
        }
        if (re == null) {
            separator = ScriptRuntime.toString(args[0]);
            matchlen[0] = separator.length();
        }

        // split target with separator or re
        int[] ip = { 0 };
        int match;
        int len = 0;
        boolean[] matched = { false };
        String[][] parens = { null };
        int version = cx.getLanguageVersion();
        while ((match = find_split(cx, scope, target, separator, version,
                                   reProxy, re, ip, matchlen, matched, parens))
               >= 0)
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

            if (version < Context.VERSION_1_3
                && version != Context.VERSION_DEFAULT)
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
    private static String js_substring(Context cx, String target,
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

    int getLength() {
        return string.length();
    }

    /*
     * Non-ECMA methods.
     */
    private static String js_substr(String target, Object[] args) {
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
    private static String js_concat(String target, Object[] args) {
        int N = args.length;
        if (N == 0) { return target; }
        else if (N == 1) {
            String arg = ScriptRuntime.toString(args[0]);
            return target.concat(arg);
        }

        // Find total capacity for the final string to avoid unnecessary
        // re-allocations in StringBuffer
        int size = target.length();
        String[] argsAsStrings = new String[N];
        for (int i = 0; i != N; ++i) {
            String s = ScriptRuntime.toString(args[i]);
            argsAsStrings[i] = s;
            size += s.length();
        }

        StringBuffer result = new StringBuffer(size);
        result.append(target);
        for (int i = 0; i != N; ++i) {
            result.append(argsAsStrings[i]);
        }
        return result.toString();
    }

    private static String js_slice(String target, Object[] args) {
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

// #string_id_map#

    protected int findPrototypeId(String s)
    {
        int id;
// #generated# Last update: 2004-03-17 13:44:29 CET
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
                case 'e': X="search";id=Id_search; break L;
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
            case 8: c=s.charAt(4);
                if (c=='r') { X="toString";id=Id_toString; }
                else if (c=='s') { X="fontsize";id=Id_fontsize; }
                else if (c=='u') { X="toSource";id=Id_toSource; }
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
        ConstructorId_fromCharCode   = -1,

        Id_constructor               = 1,
        Id_toString                  = 2,
        Id_toSource                  = 3,
        Id_valueOf                   = 4,
        Id_charAt                    = 5,
        Id_charCodeAt                = 6,
        Id_indexOf                   = 7,
        Id_lastIndexOf               = 8,
        Id_split                     = 9,
        Id_substring                 = 10,
        Id_toLowerCase               = 11,
        Id_toUpperCase               = 12,
        Id_substr                    = 13,
        Id_concat                    = 14,
        Id_slice                     = 15,
        Id_bold                      = 16,
        Id_italics                   = 17,
        Id_fixed                     = 18,
        Id_strike                    = 19,
        Id_small                     = 20,
        Id_big                       = 21,
        Id_blink                     = 22,
        Id_sup                       = 23,
        Id_sub                       = 24,
        Id_fontsize                  = 25,
        Id_fontcolor                 = 26,
        Id_link                      = 27,
        Id_anchor                    = 28,
        Id_equals                    = 29,
        Id_equalsIgnoreCase          = 30,
        Id_match                     = 31,
        Id_search                    = 32,
        Id_replace                   = 33,

        MAX_PROTOTYPE_ID             = 33;

// #/string_id_map#

    private String string;
}

