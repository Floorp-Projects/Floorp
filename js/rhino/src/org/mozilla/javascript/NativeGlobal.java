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

import java.io.StringReader;
import java.io.IOException;
import java.lang.reflect.Method;

/**
 * This class implements the global native object (function and value
 * properties only).
 *
 * See ECMA 15.1.[12].
 *
 * @author Mike Shaver
 */

public class NativeGlobal implements IdFunctionMaster {

    public static void init(Context cx, Scriptable scope, boolean sealed) {
        NativeGlobal obj = new NativeGlobal();
        obj.scopeSlaveFlag = true;

        for (int id = 1; id <= LAST_SCOPE_FUNCTION_ID; ++id) {
            String name = getMethodName(id);
            IdFunction f = new IdFunction(obj, name, id);
            f.setParentScope(scope);
            if (sealed) { f.sealObject(); }
            ScriptableObject.defineProperty(scope, name, f,
                                            ScriptableObject.DONTENUM);
        }

        ScriptableObject.defineProperty(scope, "NaN",
                                        ScriptRuntime.NaNobj,
                                        ScriptableObject.DONTENUM);
        ScriptableObject.defineProperty(scope, "Infinity",
                                        new Double(Double.POSITIVE_INFINITY),
                                        ScriptableObject.DONTENUM);
        ScriptableObject.defineProperty(scope, "undefined",
                                        Undefined.instance,
                                        ScriptableObject.DONTENUM);

        String[] errorMethods = { "ConversionError",
                                  "EvalError",
                                  "RangeError",
                                  "ReferenceError",
                                  "SyntaxError",
                                  "TypeError",
                                  "URIError"
                                };

        /*
            Each error constructor gets its own Error object as a prototype,
            with the 'name' property set to the name of the error.
        */
        for (int i = 0; i < errorMethods.length; i++) {
            String name = errorMethods[i];
            IdFunction ctor = new IdFunction(obj, name, Id_new_CommonError);
            ctor.setFunctionType(IdFunction.FUNCTION_AND_CONSTRUCTOR);
            ctor.setParentScope(scope);
            ScriptableObject.defineProperty(scope, name, ctor,
                                            ScriptableObject.DONTENUM);

            Scriptable errorProto = ScriptRuntime.newObject
                (cx, scope, "Error", ScriptRuntime.emptyArgs);

            errorProto.put("name", errorProto, name);
            ctor.put("prototype", ctor, errorProto);
            if (sealed) {
                ctor.sealObject();
                if (errorProto instanceof ScriptableObject) {
                    ((ScriptableObject)errorProto).sealObject();
                }
            }
        }
    }

    public Object execMethod(int methodId, IdFunction function, Context cx,
                             Scriptable scope, Scriptable thisObj,
                             Object[] args)
        throws JavaScriptException
    {
        if (scopeSlaveFlag) {
            switch (methodId) {
                case Id_decodeURI:
                    return js_decodeURI(cx, args);

                case Id_decodeURIComponent:
                    return js_decodeURIComponent(cx, args);

                case Id_encodeURI:
                    return js_encodeURI(cx, args);

                case Id_encodeURIComponent:
                    return js_encodeURIComponent(cx, args);

                case Id_escape:
                    return js_escape(cx, args);

                case Id_eval:
                    return js_eval(cx, scope, args);

                case Id_isFinite:
                    return js_isFinite(cx, args);

                case Id_isNaN:
                    return js_isNaN(cx, args);

                case Id_parseFloat:
                    return js_parseFloat(cx, args);

                case Id_parseInt:
                    return js_parseInt(cx, args);

                case Id_unescape:
                    return js_unescape(cx, args);

                case Id_new_CommonError:
                    return new_CommonError(function, cx, scope, args);
            }
        }
        throw IdFunction.onBadMethodId(this, methodId);
    }

    public int methodArity(int methodId) {
        if (scopeSlaveFlag) {
            switch (methodId) {
                case Id_decodeURI:           return 1;
                case Id_decodeURIComponent:  return 1;
                case Id_encodeURI:           return 1;
                case Id_encodeURIComponent:  return 1;
                case Id_escape:              return 1;
                case Id_eval:                return 1;
                case Id_isFinite:            return 1;
                case Id_isNaN:               return 1;
                case Id_parseFloat:          return 1;
                case Id_parseInt:            return 2;
                case Id_unescape:            return 1;

                case Id_new_CommonError:     return 1;
            }
        }
        return -1;
    }

    private static String getMethodName(int methodId) {
        switch (methodId) {
            case Id_decodeURI:           return "decodeURI";
            case Id_decodeURIComponent:  return "decodeURIComponent";
            case Id_encodeURI:           return "encodeURI";
            case Id_encodeURIComponent:  return "encodeURIComponent";
            case Id_escape:              return "escape";
            case Id_eval:                return "eval";
            case Id_isFinite:            return "isFinite";
            case Id_isNaN:               return "isNaN";
            case Id_parseFloat:          return "parseFloat";
            case Id_parseInt:            return "parseInt";
            case Id_unescape:            return "unescape";
        }
        return null;
    }

    /**
     * The global method parseInt, as per ECMA-262 15.1.2.2.
     */
    private Object js_parseInt(Context cx, Object[] args) {
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
        return new Double(negative ? -d : d);
    }

    /**
     * The global method parseFloat, as per ECMA-262 15.1.2.3.
     *
     * @param cx unused
     * @param thisObj unused
     * @param args the arguments to parseFloat, ignoring args[>=1]
     * @param funObj unused
     */
    private Object js_parseFloat(Context cx, Object[] args) {
        if (args.length < 1)
            return ScriptRuntime.NaNobj;
        String s = ScriptRuntime.toString(args[0]);
        int len = s.length();
        if (len == 0)
            return ScriptRuntime.NaNobj;

        int i;
        char c;
        // Scan forward to the first digit or .
        for (i=0; TokenStream.isJSSpace(c = s.charAt(i)) && i+1 < len; i++)
            /* empty */
            ;

        int start = i;

        if (c == '+' || c == '-')
            c = s.charAt(++i);

        if (c == 'I') {
            // check for "Infinity"
            double d;
            if (i+8 <= len && s.substring(i, i+8).equals("Infinity"))
                d = s.charAt(start) == '-' ? Double.NEGATIVE_INFINITY
                                           : Double.POSITIVE_INFINITY;
            else
                return ScriptRuntime.NaNobj;
            return new Double(d);
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

    private Object js_escape(Context cx, Object[] args) {
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
                String message = Context.getMessage0("msg.bad.esc.mask");
                cx.reportError(message);
                // do the ecma thing, in case reportError returns.
                mask = URL_XALPHAS | URL_XPALPHAS | URL_PATH;
            }
        }

        StringBuffer R = new StringBuffer();
        for (int k = 0; k < s.length(); k++) {
            int c = s.charAt(k), d;
            if (mask != 0 && ((c >= '0' && c <= '9') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                c == '@' || c == '*' || c == '_' ||
                c == '-' || c == '.' ||
                ((c == '/' || c == '+') && mask > 3)))
                R.append((char)c);
            else if (c < 256) {
                if (c == ' ' && mask == URL_XPALPHAS) {
                    R.append('+');
                } else {
                    R.append('%');
                    R.append(hex_digit_to_char(c >>> 4));
                    R.append(hex_digit_to_char(c & 0xF));
                }
            } else {
                R.append('%');
                R.append('u');
                R.append(hex_digit_to_char(c >>> 12));
                R.append(hex_digit_to_char((c & 0xF00) >>> 8));
                R.append(hex_digit_to_char((c & 0xF0) >>> 4));
                R.append(hex_digit_to_char(c & 0xF));
            }
        }
        return R.toString();
    }

    private static char hex_digit_to_char(int x) {
        return (char)(x <= 9 ? x + '0' : x + ('A' - 10));
    }

    /**
     * The global unescape method, as per ECMA-262 15.1.2.5.
     */

    private Object js_unescape(Context cx, Object[] args)
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

    /**
     * The global method isNaN, as per ECMA-262 15.1.2.6.
     */

    private Object js_isNaN(Context cx, Object[] args) {
        if (args.length < 1)
            return Boolean.TRUE;
        double d = ScriptRuntime.toNumber(args[0]);
        return (d != d) ? Boolean.TRUE : Boolean.FALSE;
    }

    private Object js_isFinite(Context cx, Object[] args) {
        if (args.length < 1)
            return Boolean.FALSE;
        double d = ScriptRuntime.toNumber(args[0]);
        return (d != d || d == Double.POSITIVE_INFINITY ||
                d == Double.NEGATIVE_INFINITY)
               ? Boolean.FALSE
               : Boolean.TRUE;
    }

    private Object js_eval(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        String m = ScriptRuntime.getMessage1("msg.cant.call.indirect", "eval");
        throw NativeGlobal.constructError(cx, "EvalError", m, scope);
    }

    /**
     * The eval function property of the global object.
     *
     * See ECMA 15.1.2.1
     */
    public static Object evalSpecial(Context cx, Scriptable scope,
                                     Object thisArg, Object[] args,
                                     String filename, int lineNumber)
        throws JavaScriptException
    {
        if (args.length < 1)
            return Undefined.instance;
        Object x = args[0];
        if (!(x instanceof String)) {
            String message = Context.getMessage0("msg.eval.nonstring");
            Context.reportWarning(message);
            return x;
        }
        int[] linep = { lineNumber };
        if (filename == null) {
            filename = Context.getSourcePositionFromStack(linep);
            if (filename == null) {
                filename = "";
                linep[0] = 1;
            } 
        } 
        filename += "(eval)";

        try {
            StringReader in = new StringReader((String) x);
            Object securityDomain = cx.getSecurityDomainForStackDepth(3);

            // Compile the reader with opt level of -1 to force interpreter
            // mode.
            int oldOptLevel = cx.getOptimizationLevel();
            cx.setOptimizationLevel(-1);
            Script script = cx.compileReader(scope, in, filename, linep[0],
                                             securityDomain);
            cx.setOptimizationLevel(oldOptLevel);

            // if the compile fails, an error has been reported by the
            // compiler, but we need to stop execution to avoid
            // infinite looping on while(true) { eval('foo bar') } -
            // so we throw an EvaluatorException.
            if (script == null) {
                String message = Context.getMessage0("msg.syntax");
                throw new EvaluatorException(message);
            }

            InterpretedScript is = (InterpretedScript) script;
            is.itsData.itsFromEvalCode = true;
            Object result = is.call(cx, scope, (Scriptable) thisArg, null);

            return result;
        }
        catch (IOException ioe) {
            // should never happen since we just made the Reader from a String
            throw new RuntimeException("unexpected io exception");
        }
    }


    /**
     * The NativeError functions
     *
     * See ECMA 15.11.6
     */
    public static EcmaError constructError(Context cx,
                                           String error,
                                           String message,
                                           Object scope)
    {
        int[] linep = { 0 };
        String filename = cx.getSourcePositionFromStack(linep);
        return constructError(cx, error, message, scope,
                              filename, linep[0], 0, null);
    }

    static EcmaError typeError0(String messageId, Object scope) {
        return constructError(Context.getContext(), "TypeError",
            ScriptRuntime.getMessage0(messageId), scope);
    }

    static EcmaError typeError1(String messageId, Object arg1, Object scope) {
        return constructError(Context.getContext(), "TypeError",
            ScriptRuntime.getMessage1(messageId, arg1), scope);
    }

    /**
     * The NativeError functions
     *
     * See ECMA 15.11.6
     */
    public static EcmaError constructError(Context cx,
                                           String error,
                                           String message,
                                           Object scope,
                                           String sourceName,
                                           int lineNumber,
                                           int columnNumber,
                                           String lineSource)
    {
        Scriptable scopeObject;
        try {
            scopeObject = (Scriptable) scope;
        }
        catch (ClassCastException x) {
            throw new RuntimeException(x.toString());
        }

        Object args[] = { message };
        try {
            Scriptable errorObject = cx.newObject(scopeObject, error, args);
            errorObject.put("name", errorObject, error);
            return new EcmaError((NativeError)errorObject, sourceName,
                                 lineNumber, columnNumber, lineSource);
        }
        catch (PropertyException x) {
            throw new RuntimeException(x.toString());
        }
        catch (JavaScriptException x) {
            throw new RuntimeException(x.toString());
        }
        catch (NotAFunctionException x) {
            throw new RuntimeException(x.toString());
        }
    }

    /**
     * The implementation of all the ECMA error constructors (SyntaxError,
     * TypeError, etc.)
     */
    private Object new_CommonError(IdFunction ctorObj, Context cx,
                                   Scriptable scope, Object[] args)
    {
        Scriptable newInstance = new NativeError();
        newInstance.setPrototype((Scriptable)(ctorObj.get("prototype", ctorObj)));
        newInstance.setParentScope(scope);
        if (args.length > 0)
            newInstance.put("message", newInstance, args[0]);
        return newInstance;
    }

    /*
    *   ECMA 3, 15.1.3 URI Handling Function Properties
    *
    *   The following are implementations of the algorithms
    *   given in the ECMA specification for the hidden functions
    *   'Encode' and 'Decode'.
    */
    private static String encode(Context cx, String str, String unescapedSet) {
        int j, k = 0, L;
        char C, C2;
        int V;
        char utf8buf[] = new char[6];
        StringBuffer R;

        R = new StringBuffer();

        while (k < str.length()) {
            C = str.charAt(k);
            if (unescapedSet.indexOf(C) != -1) {
                R.append(C);
            } else {
                if ((C >= 0xDC00) && (C <= 0xDFFF)) {
                    throw cx.reportRuntimeError0("msg.bad.uri");
                }
                if ((C < 0xD800) || (C > 0xDBFF))
                    V = C;
                else {
                    k++;
                    if (k == str.length()) {
                        throw cx.reportRuntimeError0("msg.bad.uri");
                    }
                    C2 = str.charAt(k);
                    if ((C2 < 0xDC00) || (C2 > 0xDFFF)) {
                        throw cx.reportRuntimeError0("msg.bad.uri");
                    }
                    V = ((C - 0xD800) << 10) + (C2 - 0xDC00) + 0x10000;
                }
                L = oneUcs4ToUtf8Char(utf8buf, V);
                for (j = 0; j < L; j++) {
                    R.append('%');
                    if (utf8buf[j] < 16)
                        R.append('0');
                    R.append(Integer.toHexString(utf8buf[j]));
                }
            }
            k++;
        }
        return R.toString();
    }

    private static boolean isHex(char c) {
        return ((c >= '0' && c <= '9')
                || (c >= 'a' && c <= 'f')
                || (c >= 'A' && c <= 'F'));
    }

    private static int unHex(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        else
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            else
                return c - 'A' +10;
    }

    private static String decode(Context cx, String str, String reservedSet) {
        int start, k = 0;
        char C, H;
        int V;
        int B;
        char[] octets = new char[6];
        StringBuffer R;
        int j, n;

        R = new StringBuffer();

        while (k < str.length()) {
            C = str.charAt(k);
            if (C == '%') {
                start = k;
                if ((k + 2) >= str.length())
                    throw cx.reportRuntimeError0("msg.bad.uri");
                if (!isHex(str.charAt(k + 1)) || !isHex(str.charAt(k + 2)))
                    throw cx.reportRuntimeError0("msg.bad.uri");
                B = unHex(str.charAt(k + 1)) * 16 + unHex(str.charAt(k + 2));
                k += 2;
                if ((B & 0x80) == 0)
                    C = (char)B;
                else {
                    n = 1;
                    while ((B & (0x80 >>> n)) != 0) n++;
                    if ((n == 1) || (n > 6))
                        throw cx.reportRuntimeError0("msg.bad.uri");
                    octets[0] = (char)B;
                    if ((k + 3 * (n - 1)) >= str.length())
                        throw cx.reportRuntimeError0("msg.bad.uri");
                    for (j = 1; j < n; j++) {
                        k++;
                        if (str.charAt(k) != '%')
                            throw cx.reportRuntimeError0("msg.bad.uri");
                        if (!isHex(str.charAt(k + 1))
                            || !isHex(str.charAt(k + 2)))
                            throw cx.reportRuntimeError0("msg.bad.uri");
                        B = unHex(str.charAt(k + 1)) * 16
                            + unHex(str.charAt(k + 2));
                        if ((B & 0xC0) != 0x80)
                            throw cx.reportRuntimeError0("msg.bad.uri");
                        k += 2;
                        octets[j] = (char)B;
                    }
                    V = utf8ToOneUcs4Char(octets, n);
                    if (V >= 0x10000) {
                        V -= 0x10000;
                        if (V > 0xFFFFF)
                            throw cx.reportRuntimeError0("msg.bad.uri");
                        C = (char)((V & 0x3FF) + 0xDC00);
                        H = (char)((V >>> 10) + 0xD800);
                        R.append(H);
                    }
                    else
                        C = (char)V;
                }
                if (reservedSet.indexOf(C) != -1) {
                    for (int x = 0; x < (k - start + 1); x++)
                        R.append(str.charAt(start + x));
                }
                else
                    R.append(C);
            }
            else
                R.append(C);
            k++;
        }
        return R.toString();
    }

    private static String uriReservedPlusPound = ";/?:@&=+$,#";
    private static String uriUnescaped =
                                        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.!~*'()";

    private String js_decodeURI(Context cx, Object[] args) {
        String str = ScriptRuntime.toString(args, 0);
        return decode(cx, str, uriReservedPlusPound);
    }

    private String js_decodeURIComponent(Context cx, Object[] args) {
        String str = ScriptRuntime.toString(args, 0);
        return decode(cx, str, "");
    }

    private Object js_encodeURI(Context cx, Object[] args) {
        String str = ScriptRuntime.toString(args, 0);
        return encode(cx, str, uriReservedPlusPound + uriUnescaped);
    }

    private String js_encodeURIComponent(Context cx, Object[] args) {
        String str = ScriptRuntime.toString(args, 0);
        return encode(cx, str, uriUnescaped);
    }

    /* Convert one UCS-4 char and write it into a UTF-8 buffer, which must be
    * at least 6 bytes long.  Return the number of UTF-8 bytes of data written.
    */
    private static int oneUcs4ToUtf8Char(char[] utf8Buffer, int ucs4Char) {
        int utf8Length = 1;

        //JS_ASSERT(ucs4Char <= 0x7FFFFFFF);
        if ((ucs4Char & ~0x7F) == 0)
            utf8Buffer[0] = (char)ucs4Char;
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
                utf8Buffer[i] = (char)((ucs4Char & 0x3F) | 0x80);
                ucs4Char >>>= 6;
            }
            utf8Buffer[0] = (char)(0x100 - (1 << (8-utf8Length)) + ucs4Char);
        }
        return utf8Length;
    }


    /* Convert a utf8 character sequence into a UCS-4 character and return that
    * character.  It is assumed that the caller already checked that the sequence is valid.
    */
    private static int utf8ToOneUcs4Char(char[] utf8Buffer, int utf8Length) {
        int ucs4Char;
        int k = 0;
        //JS_ASSERT(utf8Length >= 1 && utf8Length <= 6);
        if (utf8Length == 1) {
            ucs4Char = utf8Buffer[0];
            //            JS_ASSERT(!(ucs4Char & 0x80));
        } else {
            //JS_ASSERT((*utf8Buffer & (0x100 - (1 << (7-utf8Length)))) == (0x100 - (1 << (8-utf8Length))));
            ucs4Char = utf8Buffer[k++] & ((1<<(7-utf8Length))-1);
            while (--utf8Length > 0) {
                //JS_ASSERT((*utf8Buffer & 0xC0) == 0x80);
                ucs4Char = ucs4Char<<6 | (utf8Buffer[k++] & 0x3F);
            }
        }
        return ucs4Char;
    }

    private static final int
        Id_decodeURI           =  1,
        Id_decodeURIComponent  =  2,
        Id_encodeURI           =  3,
        Id_encodeURIComponent  =  4,
        Id_escape              =  5,
        Id_eval                =  6,
        Id_isFinite            =  7,
        Id_isNaN               =  8,
        Id_parseFloat          =  9,
        Id_parseInt            = 10,
        Id_unescape            = 11,

        LAST_SCOPE_FUNCTION_ID = 11,

        Id_new_CommonError     = 12;

    private boolean scopeSlaveFlag;

}
