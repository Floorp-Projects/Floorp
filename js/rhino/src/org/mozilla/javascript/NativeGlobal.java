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

import java.io.Serializable;

import org.mozilla.javascript.xml.XMLLib;

/**
 * This class implements the global native object (function and value
 * properties only).
 *
 * See ECMA 15.1.[12].
 *
 * @author Mike Shaver
 */

public class NativeGlobal implements Serializable, IdFunctionCall
{

    public static void init(Context cx, Scriptable scope, boolean sealed) {
        NativeGlobal obj = new NativeGlobal();

        for (int id = 1; id <= LAST_SCOPE_FUNCTION_ID; ++id) {
            String name;
            int arity = 1;
            switch (id) {
              case Id_decodeURI:
                name = "decodeURI";
                break;
              case Id_decodeURIComponent:
                name = "decodeURIComponent";
                break;
              case Id_encodeURI:
                name = "encodeURI";
                break;
              case Id_encodeURIComponent:
                name = "encodeURIComponent";
                break;
              case Id_escape:
                name = "escape";
                break;
              case Id_eval:
                name = "eval";
                break;
              case Id_isFinite:
                name = "isFinite";
                break;
              case Id_isNaN:
                name = "isNaN";
                break;
              case Id_isXMLName:
                name = "isXMLName";
                break;
              case Id_parseFloat:
                name = "parseFloat";
                break;
              case Id_parseInt:
                name = "parseInt";
                arity = 2;
                break;
              case Id_unescape:
                name = "unescape";
                break;
              case Id_uneval:
                name = "uneval";
                break;
              default:
                  throw Kit.codeBug();
            }
            IdFunctionObject f = new IdFunctionObject(obj, FTAG, id, name,
                                                      arity, scope);
            if (sealed) {
                f.sealObject();
            }
            f.exportAsScopeProperty();
        }

        ScriptableObject.defineProperty(
            scope, "NaN", ScriptRuntime.NaNobj,
            ScriptableObject.DONTENUM);
        ScriptableObject.defineProperty(
            scope, "Infinity",
            ScriptRuntime.wrapNumber(Double.POSITIVE_INFINITY),
            ScriptableObject.DONTENUM);
        ScriptableObject.defineProperty(
            scope, "undefined", Undefined.instance,
            ScriptableObject.DONTENUM);

        String[] errorMethods = Kit.semicolonSplit(""
                                    +"ConversionError;"
                                    +"EvalError;"
                                    +"RangeError;"
                                    +"ReferenceError;"
                                    +"SyntaxError;"
                                    +"TypeError;"
                                    +"URIError;"
                                    +"InternalError;"
                                    +"JavaException;"
                                    );

        /*
            Each error constructor gets its own Error object as a prototype,
            with the 'name' property set to the name of the error.
        */
        for (int i = 0; i < errorMethods.length; i++) {
            String name = errorMethods[i];
            Scriptable errorProto = ScriptRuntime.
                                        newObject(cx, scope, "Error",
                                                  ScriptRuntime.emptyArgs);
            errorProto.put("name", errorProto, name);
            if (sealed) {
                if (errorProto instanceof ScriptableObject) {
                    ((ScriptableObject)errorProto).sealObject();
                }
            }
            IdFunctionObject ctor = new IdFunctionObject(obj, FTAG,
                                                         Id_new_CommonError,
                                                         name, 1, scope);
            ctor.markAsConstructor(errorProto);
            if (sealed) {
                ctor.sealObject();
            }
            ctor.exportAsScopeProperty();
        }
    }

    public Object execIdCall(IdFunctionObject f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (f.hasTag(FTAG)) {
            int methodId = f.methodId();
            switch (methodId) {
                case Id_decodeURI:
                case Id_decodeURIComponent: {
                    String str = ScriptRuntime.toString(args, 0);
                    return decode(str, methodId == Id_decodeURI);
                }

                case Id_encodeURI:
                case Id_encodeURIComponent: {
                    String str = ScriptRuntime.toString(args, 0);
                    return encode(str, methodId == Id_encodeURI);
                }

                case Id_escape:
                    return js_escape(args);

                case Id_eval:
                    return js_eval(cx, scope, args);

                case Id_isFinite: {
                    boolean result;
                    if (args.length < 1) {
                        result = false;
                    } else {
                        double d = ScriptRuntime.toNumber(args[0]);
                        result = (d == d
                                  && d != Double.POSITIVE_INFINITY
                                  && d != Double.NEGATIVE_INFINITY);
                    }
                    return ScriptRuntime.wrapBoolean(result);
                }

                case Id_isNaN: {
                    // The global method isNaN, as per ECMA-262 15.1.2.6.
                    boolean result;
                    if (args.length < 1) {
                        result = true;
                    } else {
                        double d = ScriptRuntime.toNumber(args[0]);
                        result = (d != d);
                    }
                    return ScriptRuntime.wrapBoolean(result);
                }

                case Id_isXMLName: {
                    Object name = (args.length == 0)
                                  ? Undefined.instance : args[0];
                    XMLLib xmlLib = XMLLib.extractFromScope(scope);
                    return ScriptRuntime.wrapBoolean(
                        xmlLib.isXMLName(cx, name));
                }

                case Id_parseFloat:
                    return js_parseFloat(args);

                case Id_parseInt:
                    return js_parseInt(args);

                case Id_unescape:
                    return js_unescape(args);

                case Id_uneval: {
                    Object value = (args.length != 0)
                                   ? args[0] : Undefined.instance;
                    return ScriptRuntime.uneval(cx, scope, value);
                }

                case Id_new_CommonError:
                    // The implementation of all the ECMA error constructors
                    // (SyntaxError, TypeError, etc.)
                    return NativeError.make(cx, scope, f, args);
            }
        }
        throw f.unknown();
    }

    /**
     * The global method parseInt, as per ECMA-262 15.1.2.2.
     */
    private Object js_parseInt(Object[] args) {
        String s = ScriptRuntime.toString(args, 0);
        int radix = ScriptRuntime.toInt32(args, 1);

        int len = s.length();
        if (len == 0)
            return ScriptRuntime.NaNobj;

        boolean negative = false;
        int start = 0;
        char c;
        do {
            c = s.charAt(start);
            if (!Character.isWhitespace(c))
                break;
            start++;
        } while (start < len);

        if (c == '+' || (negative = (c == '-')))
            start++;

        final int NO_RADIX = -1;
        if (radix == 0) {
            radix = NO_RADIX;
        } else if (radix < 2 || radix > 36) {
            return ScriptRuntime.NaNobj;
        } else if (radix == 16 && len - start > 1 && s.charAt(start) == '0') {
            c = s.charAt(start+1);
            if (c == 'x' || c == 'X')
                start += 2;
        }

        if (radix == NO_RADIX) {
            radix = 10;
            if (len - start > 1 && s.charAt(start) == '0') {
                c = s.charAt(start+1);
                if (c == 'x' || c == 'X') {
                    radix = 16;
                    start += 2;
                } else if ('0' <= c && c <= '9') {
                    radix = 8;
                    start++;
                }
            }
        }

        double d = ScriptRuntime.stringToNumber(s, start, radix);
        return ScriptRuntime.wrapNumber(negative ? -d : d);
    }

    /**
     * The global method parseFloat, as per ECMA-262 15.1.2.3.
     *
     * @param cx unused
     * @param thisObj unused
     * @param args the arguments to parseFloat, ignoring args[>=1]
     * @param funObj unused
     */
    private Object js_parseFloat(Object[] args)
    {
        if (args.length < 1)
            return ScriptRuntime.NaNobj;

        String s = ScriptRuntime.toString(args[0]);
        int len = s.length();
        int start = 0;
        // Scan forward to skip whitespace
        char c;
        for (;;) {
            if (start == len) {
                return ScriptRuntime.NaNobj;
            }
            c = s.charAt(start);
            if (!TokenStream.isJSSpace(c)) {
                break;
            }
            ++start;
        }

        int i = start;
        if (c == '+' || c == '-') {
            ++i;
            if (i == len) {
                return ScriptRuntime.NaNobj;
            }
            c = s.charAt(i);
        }

        if (c == 'I') {
            // check for "Infinity"
            if (i+8 <= len && s.regionMatches(i, "Infinity", 0, 8)) {
                double d;
                if (s.charAt(start) == '-') {
                    d = Double.NEGATIVE_INFINITY;
                } else {
                    d = Double.POSITIVE_INFINITY;
                }
                return ScriptRuntime.wrapNumber(d);
            }
            return ScriptRuntime.NaNobj;
        }

        // Find the end of the legal bit
        int decimal = -1;
        int exponent = -1;
        for (; i < len; i++) {
            switch (s.charAt(i)) {
              case '.':
                if (decimal != -1) // Only allow a single decimal point.
                    break;
                decimal = i;
                continue;

              case 'e':
              case 'E':
                if (exponent != -1)
                    break;
                exponent = i;
                continue;

              case '+':
              case '-':
                 // Only allow '+' or '-' after 'e' or 'E'
                if (exponent != i-1)
                    break;
                continue;

              case '0': case '1': case '2': case '3': case '4':
              case '5': case '6': case '7': case '8': case '9':
                continue;

              default:
                break;
            }
            break;
        }
        s = s.substring(start, i);
        try {
            return Double.valueOf(s);
        }
        catch (NumberFormatException ex) {
            return ScriptRuntime.NaNobj;
        }
    }

    /**
     * The global method escape, as per ECMA-262 15.1.2.4.

     * Includes code for the 'mask' argument supported by the C escape
     * method, which used to be part of the browser imbedding.  Blame
     * for the strange constant names should be directed there.
     */

    private Object js_escape(Object[] args) {
        final int
            URL_XALPHAS = 1,
            URL_XPALPHAS = 2,
            URL_PATH = 4;

        String s = ScriptRuntime.toString(args, 0);

        int mask = URL_XALPHAS | URL_XPALPHAS | URL_PATH;
        if (args.length > 1) { // the 'mask' argument.  Non-ECMA.
            double d = ScriptRuntime.toNumber(args[1]);
            if (d != d || ((mask = (int) d) != d) ||
                0 != (mask & ~(URL_XALPHAS | URL_XPALPHAS | URL_PATH)))
            {
                throw Context.reportRuntimeError0("msg.bad.esc.mask");
            }
        }

        StringBuffer sb = null;
        for (int k = 0, L = s.length(); k != L; ++k) {
            int c = s.charAt(k);
            if (mask != 0
                && ((c >= '0' && c <= '9')
                    || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                    || c == '@' || c == '*' || c == '_' || c == '-' || c == '.'
                    || (0 != (mask & URL_PATH) && (c == '/' || c == '+'))))
            {
                if (sb != null) {
                    sb.append((char)c);
                }
            } else {
                if (sb == null) {
                    sb = new StringBuffer(L + 3);
                    sb.append(s);
                    sb.setLength(k);
                }

                int hexSize;
                if (c < 256) {
                    if (c == ' ' && mask == URL_XPALPHAS) {
                        sb.append('+');
                        continue;
                    }
                    sb.append('%');
                    hexSize = 2;
                } else {
                    sb.append('%');
                    sb.append('u');
                    hexSize = 4;
                }

                // append hexadecimal form of c left-padded with 0
                for (int shift = (hexSize - 1) * 4; shift >= 0; shift -= 4) {
                    int digit = 0xf & (c >> shift);
                    int hc = (digit < 10) ? '0' + digit : 'A' - 10 + digit;
                    sb.append((char)hc);
                }
            }
        }

        return (sb == null) ? s : sb.toString();
    }

    /**
     * The global unescape method, as per ECMA-262 15.1.2.5.
     */

    private Object js_unescape(Object[] args)
    {
        String s = ScriptRuntime.toString(args, 0);
        int firstEscapePos = s.indexOf('%');
        if (firstEscapePos >= 0) {
            int L = s.length();
            char[] buf = s.toCharArray();
            int destination = firstEscapePos;
            for (int k = firstEscapePos; k != L;) {
                char c = buf[k];
                ++k;
                if (c == '%' && k != L) {
                    int end, start;
                    if (buf[k] == 'u') {
                        start = k + 1;
                        end = k + 5;
                    } else {
                        start = k;
                        end = k + 2;
                    }
                    if (end <= L) {
                        int x = 0;
                        for (int i = start; i != end; ++i) {
                            x = (x << 4) | TokenStream.xDigitToInt(buf[i]);
                        }
                        if (x >= 0) {
                            c = (char)x;
                            k = end;
                        }
                    }
                }
                buf[destination] = c;
                ++destination;
            }
            s = new String(buf, 0, destination);
        }
        return s;
    }

    private Object js_eval(Context cx, Scriptable scope, Object[] args)
    {
        String m = ScriptRuntime.getMessage1("msg.cant.call.indirect", "eval");
        throw NativeGlobal.constructError(cx, "EvalError", m, scope);
    }

    static boolean isEvalFunction(Object functionObj)
    {
        if (functionObj instanceof IdFunctionObject) {
            IdFunctionObject function = (IdFunctionObject)functionObj;
            if (function.hasTag(FTAG) && function.methodId() == Id_eval) {
                return true;
            }
        }
        return false;
    }

    /**
     * @deprecated Use {@link ScriptRuntime#constructError(String,String)}
     * instead.
     */
    public static EcmaError constructError(Context cx,
                                           String error,
                                           String message,
                                           Scriptable scope)
    {
        return ScriptRuntime.constructError(error, message);
    }

    /**
     * @deprecated Use
     * {@link ScriptRuntime#constructError(String,String,String,int,int,String)}
     * instead.
     */
    public static EcmaError constructError(Context cx,
                                           String error,
                                           String message,
                                           Scriptable scope,
                                           String sourceName,
                                           int lineNumber,
                                           int columnNumber,
                                           String lineSource)
    {
        return ScriptRuntime.constructError(error, message,
                                            sourceName, lineNumber,
                                            lineSource, columnNumber);
    }

    /*
    *   ECMA 3, 15.1.3 URI Handling Function Properties
    *
    *   The following are implementations of the algorithms
    *   given in the ECMA specification for the hidden functions
    *   'Encode' and 'Decode'.
    */
    private static String encode(String str, boolean fullUri) {
        byte[] utf8buf = null;
        StringBuffer sb = null;

        for (int k = 0, length = str.length(); k != length; ++k) {
            char C = str.charAt(k);
            if (encodeUnescaped(C, fullUri)) {
                if (sb != null) {
                    sb.append(C);
                }
            } else {
                if (sb == null) {
                    sb = new StringBuffer(length + 3);
                    sb.append(str);
                    sb.setLength(k);
                    utf8buf = new byte[6];
                }
                if (0xDC00 <= C && C <= 0xDFFF) {
                    throw Context.reportRuntimeError0("msg.bad.uri");
                }
                int V;
                if (C < 0xD800 || 0xDBFF < C) {
                    V = C;
                } else {
                    k++;
                    if (k == length) {
                        throw Context.reportRuntimeError0("msg.bad.uri");
                    }
                    char C2 = str.charAt(k);
                    if (!(0xDC00 <= C2 && C2 <= 0xDFFF)) {
                        throw Context.reportRuntimeError0("msg.bad.uri");
                    }
                    V = ((C - 0xD800) << 10) + (C2 - 0xDC00) + 0x10000;
                }
                int L = oneUcs4ToUtf8Char(utf8buf, V);
                for (int j = 0; j < L; j++) {
                    int d = 0xff & utf8buf[j];
                    sb.append('%');
                    sb.append(toHexChar(d >>> 4));
                    sb.append(toHexChar(d & 0xf));
                }
            }
        }
        return (sb == null) ? str : sb.toString();
    }

    private static char toHexChar(int i) {
        if (i >> 4 != 0) Kit.codeBug();
        return (char)((i < 10) ? i + '0' : i - 10 + 'a');
    }

    private static int unHex(char c) {
        if ('A' <= c && c <= 'F') {
            return c - 'A' + 10;
        } else if ('a' <= c && c <= 'f') {
            return c - 'a' + 10;
        } else if ('0' <= c && c <= '9') {
            return c - '0';
        } else {
            return -1;
        }
    }

    private static int unHex(char c1, char c2) {
        int i1 = unHex(c1);
        int i2 = unHex(c2);
        if (i1 >= 0 && i2 >= 0) {
            return (i1 << 4) | i2;
        }
        return -1;
    }

    private static String decode(String str, boolean fullUri) {
        char[] buf = null;
        int bufTop = 0;

        for (int k = 0, length = str.length(); k != length;) {
            char C = str.charAt(k);
            if (C != '%') {
                if (buf != null) {
                    buf[bufTop++] = C;
                }
                ++k;
            } else {
                if (buf == null) {
                    // decode always compress so result can not be bigger then
                    // str.length()
                    buf = new char[length];
                    str.getChars(0, k, buf, 0);
                    bufTop = k;
                }
                int start = k;
                if (k + 3 > length)
                    throw Context.reportRuntimeError0("msg.bad.uri");
                int B = unHex(str.charAt(k + 1), str.charAt(k + 2));
                if (B < 0) throw Context.reportRuntimeError0("msg.bad.uri");
                k += 3;
                if ((B & 0x80) == 0) {
                    C = (char)B;
                } else {
                    // Decode UTF-8 sequence into ucs4Char and encode it into
                    // UTF-16
                    int utf8Tail, ucs4Char, minUcs4Char;
                    if ((B & 0xC0) == 0x80) {
                        // First  UTF-8 should be ouside 0x80..0xBF
                        throw Context.reportRuntimeError0("msg.bad.uri");
                    } else if ((B & 0x20) == 0) {
                        utf8Tail = 1; ucs4Char = B & 0x1F;
                        minUcs4Char = 0x80;
                    } else if ((B & 0x10) == 0) {
                        utf8Tail = 2; ucs4Char = B & 0x0F;
                        minUcs4Char = 0x800;
                    } else if ((B & 0x08) == 0) {
                        utf8Tail = 3; ucs4Char = B & 0x07;
                        minUcs4Char = 0x10000;
                    } else if ((B & 0x04) == 0) {
                        utf8Tail = 4; ucs4Char = B & 0x03;
                        minUcs4Char = 0x200000;
                    } else if ((B & 0x02) == 0) {
                        utf8Tail = 5; ucs4Char = B & 0x01;
                        minUcs4Char = 0x4000000;
                    } else {
                        // First UTF-8 can not be 0xFF or 0xFE
                        throw Context.reportRuntimeError0("msg.bad.uri");
                    }
                    if (k + 3 * utf8Tail > length)
                        throw Context.reportRuntimeError0("msg.bad.uri");
                    for (int j = 0; j != utf8Tail; j++) {
                        if (str.charAt(k) != '%')
                            throw Context.reportRuntimeError0("msg.bad.uri");
                        B = unHex(str.charAt(k + 1), str.charAt(k + 2));
                        if (B < 0 || (B & 0xC0) != 0x80)
                            throw Context.reportRuntimeError0("msg.bad.uri");
                        ucs4Char = (ucs4Char << 6) | (B & 0x3F);
                        k += 3;
                    }
                    // Check for overlongs and other should-not-present codes
                    if (ucs4Char < minUcs4Char
                        || ucs4Char == 0xFFFE || ucs4Char == 0xFFFF)
                    {
                        ucs4Char = 0xFFFD;
                    }
                    if (ucs4Char >= 0x10000) {
                        ucs4Char -= 0x10000;
                        if (ucs4Char > 0xFFFFF)
                            throw Context.reportRuntimeError0("msg.bad.uri");
                        char H = (char)((ucs4Char >>> 10) + 0xD800);
                        C = (char)((ucs4Char & 0x3FF) + 0xDC00);
                        buf[bufTop++] = H;
                    } else {
                        C = (char)ucs4Char;
                    }
                }
                if (fullUri && URI_DECODE_RESERVED.indexOf(C) >= 0) {
                    for (int x = start; x != k; x++) {
                        buf[bufTop++] = str.charAt(x);
                    }
                } else {
                    buf[bufTop++] = C;
                }
            }
        }
        return (buf == null) ? str : new String(buf, 0, bufTop);
    }

    private static boolean encodeUnescaped(char c, boolean fullUri) {
        if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')
            || ('0' <= c && c <= '9'))
        {
            return true;
        }
        if ("-_.!~*'()".indexOf(c) >= 0)
            return true;
        if (fullUri) {
            return URI_DECODE_RESERVED.indexOf(c) >= 0;
        }
        return false;
    }

    private static final String URI_DECODE_RESERVED = ";/?:@&=+$,#";

    /* Convert one UCS-4 char and write it into a UTF-8 buffer, which must be
    * at least 6 bytes long.  Return the number of UTF-8 bytes of data written.
    */
    private static int oneUcs4ToUtf8Char(byte[] utf8Buffer, int ucs4Char) {
        int utf8Length = 1;

        //JS_ASSERT(ucs4Char <= 0x7FFFFFFF);
        if ((ucs4Char & ~0x7F) == 0)
            utf8Buffer[0] = (byte)ucs4Char;
        else {
            int i;
            int a = ucs4Char >>> 11;
            utf8Length = 2;
            while (a != 0) {
                a >>>= 5;
                utf8Length++;
            }
            i = utf8Length;
            while (--i > 0) {
                utf8Buffer[i] = (byte)((ucs4Char & 0x3F) | 0x80);
                ucs4Char >>>= 6;
            }
            utf8Buffer[0] = (byte)(0x100 - (1 << (8-utf8Length)) + ucs4Char);
        }
        return utf8Length;
    }

    private static final Object FTAG = new Object();

    private static final int
        Id_decodeURI           =  1,
        Id_decodeURIComponent  =  2,
        Id_encodeURI           =  3,
        Id_encodeURIComponent  =  4,
        Id_escape              =  5,
        Id_eval                =  6,
        Id_isFinite            =  7,
        Id_isNaN               =  8,
        Id_isXMLName           =  9,
        Id_parseFloat          = 10,
        Id_parseInt            = 11,
        Id_unescape            = 12,
        Id_uneval              = 13,

        LAST_SCOPE_FUNCTION_ID = 13,

        Id_new_CommonError     = 14;
}
