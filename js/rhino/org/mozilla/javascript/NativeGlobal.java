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

import java.io.StringReader;
import java.io.IOException;

/**
 * This class implements the global native object (function and value properties only).
 * See ECMA 15.1.[12].
 *
 * @author Mike Shaver
 */

public class NativeGlobal {

    public static void init(Scriptable scope)
        throws PropertyException
    {

        String names[] = { "eval",
                           "parseInt",
                           "parseFloat",
                           "escape",
                           "unescape",
                           "isNaN",
                           "isFinite"
                         };

        // We can downcast here because Context.initStandardObjects
        // takes a ScriptableObject scope.
        ScriptableObject global = (ScriptableObject) scope;
        global.defineFunctionProperties(names, NativeGlobal.class,
                                        ScriptableObject.DONTENUM);

        global.defineProperty("NaN", ScriptRuntime.NaNobj, 
                              ScriptableObject.DONTENUM);
        global.defineProperty("Infinity", new Double(Double.POSITIVE_INFINITY),
                              ScriptableObject.DONTENUM);
        global.defineProperty("undefined", Undefined.instance,
                              ScriptableObject.DONTENUM);
    }

    /**
     * The global method parseInt, as per ECMA-262 15.1.2.2.
     */
    public static Object parseInt(String s, int radix) {
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
        } else if (radix == 16 && len - start > 1 &&
                 s.charAt(start) == '0')
        {
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
                } else if (c != '.') {
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
    public static Object parseFloat(Context cx, Scriptable thisObj,
                                    Object[] args, Function funObj)
    {
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
    private static int
        URL_XALPHAS = 1,
        URL_XPALPHAS = 2,
        URL_PATH = 4;

    public static Object escape(Context cx, Scriptable thisObj,
                                Object[] args, Function funObj)
    {
        char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                         '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
        
        if (args.length < 1)
            args = ScriptRuntime.padArguments(args, 1);
	
        String s = ScriptRuntime.toString(args[0]);

        int mask = URL_XALPHAS | URL_XPALPHAS | URL_PATH;
        if (args.length > 1) { // the 'mask' argument.  Non-ECMA.
            double d = ScriptRuntime.toNumber(args[1]);
            if (d != d || ((mask = (int) d) != d) ||
                0 != (mask & ~(URL_XALPHAS | URL_XPALPHAS | URL_PATH)))
            {
                String message = Context.getMessage
                    ("msg.bad.esc.mask", null);
                cx.reportError(message);
                // do the ecma thing, in case reportError returns.
                mask = URL_XALPHAS | URL_XPALPHAS | URL_PATH;
            }
        }

	StringBuffer R = new StringBuffer();
	for (int k = 0; k < s.length(); k++) {
	    char c = s.charAt(k);
	    if (mask != 0 && 
                ((c >= '0' && c <= '9') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z') ||
		c == '@' || c == '*' || c == '_' ||
                c == '-' || c == '.' ||
		((c == '/' || c == '+') && mask > 3)))
		R.append(c);
	    else if (c < 256) {
                if (c == ' ' && mask == URL_XPALPHAS) {
                    R.append('+');
                } else {
                    R.append('%');
                    R.append(digits[c >> 4]);
                    R.append(digits[c & 0xF]);
                }
	    } else {
                R.append('%');
                R.append('u');
                R.append(digits[c >> 12]);
                R.append(digits[(c & 0xF00) >> 8]);
                R.append(digits[(c & 0xF0) >> 4]);
                R.append(digits[c & 0xF]);
	    }
	}
	return R.toString();
    }

    /**
     * The global unescape method, as per ECMA-262 15.1.2.5.
     */

    public static Object unescape(Context cx, Scriptable thisObj,
                                  Object[] args, Function funObj)
    {
	if (args.length < 1)
            args = ScriptRuntime.padArguments(args, 1);

	String s = ScriptRuntime.toString(args[0]);
	StringBuffer R = new StringBuffer();
	stringIter: for (int k = 0; k < s.length(); k++) {
	    char c = s.charAt(k);
	    if (c != '%' || k == s.length() -1) {
		R.append(c);
		continue;
	    }
	    String hex;
	    int end, start;
	    if (s.charAt(k+1) == 'u') {
		start = k+2;
		end = k+6;
	    } else {
		start = k+1;
		end = k+3;
	    }
	    if (end > s.length()) {
		R.append('%');
		continue;
	    }
	    hex = s.substring(start, end);
	    for (int i = 0; i < hex.length(); i++)
		if (!TokenStream.isXDigit(hex.charAt(i))) {
		    R.append('%');
		    continue stringIter;
		}
	    k = end - 1;
	    R.append((new Character((char) Integer.valueOf(hex, 16).intValue())));
	}

	return R.toString();
    }

    /**
     * The global method isNaN, as per ECMA-262 15.1.2.6.
     */

    public static Object isNaN(Context cx, Scriptable thisObj,
                               Object[] args, Function funObj)
    {
        if (args.length < 1)
            return Boolean.TRUE;
        double d = ScriptRuntime.toNumber(args[0]);
        return (d != d) ? Boolean.TRUE : Boolean.FALSE;
    }

    public static Object isFinite(Context cx, Scriptable thisObj,
                                  Object[] args, Function funObj)
    {
        if (args.length < 1)
            return Boolean.FALSE;
        double d = ScriptRuntime.toNumber(args[0]);
        return (d != d || d == Double.POSITIVE_INFINITY || 
                d == Double.NEGATIVE_INFINITY)
               ? Boolean.FALSE
               : Boolean.TRUE;
    }

    public static Object eval(Context cx, Scriptable thisObj,
                              Object[] args, Function funObj)
        throws JavaScriptException
    {
        Object[] msgArgs = { "eval" };
        throw Context.reportRuntimeError(
            Context.getMessage("msg.cant.call.indirect", msgArgs));
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
            String message = Context.getMessage("msg.eval.nonstring", null);
            Context.reportWarning(message);
            return x;
        }
        int[] linep = { lineNumber };
        if (filename == null) {
            filename = Context.getSourcePositionFromStack(linep);
            if (filename == null) {
                filename = "<eval'ed string>";
                linep[0] = 1;
            }
        }
        
        try {
            StringReader in = new StringReader((String) x);
            Object securityDomain = cx.getSecurityDomainForStackDepth(3);
            
            // Compile the reader with opt level of -1 to force interpreter
            // mode.
            int oldOptLevel = cx.getOptimizationLevel();
            cx.setOptimizationLevel(-1);
            Script script = cx.compileReader(null, in, filename, linep[0], 
                                             securityDomain);
            cx.setOptimizationLevel(oldOptLevel);
            
            // if the compile fails, an error has been reported by the
            // compiler, but we need to stop execution to avoid
            // infinite looping on while(true) { eval('foo bar') } -
            // so we throw an EvaluatorException.
            if (script == null) {
                String message = Context.getMessage("msg.syntax", null);
                throw new EvaluatorException(message);
            }

            InterpretedScript is = (InterpretedScript) script;
            Object result = is.call(cx, scope, (Scriptable) thisArg, null);
                             
            return result;
        }
        catch (IOException ioe) {
            // should never happen since we just made the Reader from a String
            throw new RuntimeException("unexpected io exception");
        }
        
    }
}

