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
 * Roger Lawrence
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

import java.io.*;

/**
 * This class implements the JavaScript scanner.
 *
 * It is based on the C source files jsscan.c and jsscan.h
 * in the jsref package.
 *
 * @see org.mozilla.javascript.Parser
 *
 * @author Mike McCabe
 * @author Brendan Eich
 */

public class TokenStream {
    /*
     * JSTokenStream flags, mirroring those in jsscan.h.  These are used
     * by the parser to change/check the state of the scanner.
     */

    public final static int
        TSF_NEWLINES    = 0x0001,  // tokenize newlines
        TSF_FUNCTION    = 0x0002,  // scanning inside function body
        TSF_RETURN_EXPR = 0x0004,  // function has 'return expr;'
        TSF_RETURN_VOID = 0x0008,  // function has 'return;'
        TSF_REGEXP      = 0x0010;  // looking for a regular expression

    /*
     * For chars - because we need something out-of-range
     * to check.  (And checking EOF by exception is annoying.)
     * Note distinction from EOF token type!
     */
    private final static int
        EOF_CHAR = -1;

    /**
     * Token types.  These values correspond to JSTokenType values in
     * jsscan.c.
     */

    public final static int
	// start enum
        ERROR       = -1, // well-known as the only code < EOF
        EOF         = 0,  // end of file token - (not EOF_CHAR)
        EOL         = 1,  // end of line
        // Beginning here are interpreter bytecodes. Their values
        // must not exceed 127.
        POPV        = 2,
        ENTERWITH   = 3,
        LEAVEWITH   = 4,
        RETURN      = 5,
        GOTO        = 6,
        IFEQ        = 7,
        IFNE        = 8,
        DUP         = 9,
        SETNAME     = 10,
        BITOR       = 11,
        BITXOR      = 12,
        BITAND      = 13,
        EQ          = 14,
        NE          = 15,
        LT          = 16,
        LE          = 17,
        GT          = 18,
        GE          = 19,
        LSH         = 20,
        RSH         = 21,
        URSH        = 22,
        ADD         = 23,
        SUB         = 24,
        MUL         = 25,
        DIV         = 26,
        MOD         = 27,
        BITNOT      = 28,
        NEG         = 29,
        NEW         = 30,
        DELPROP     = 31,
        TYPEOF      = 32,
        NAMEINC     = 33,
        PROPINC     = 34,
        ELEMINC     = 35,
        NAMEDEC     = 36,
        PROPDEC     = 37,
        ELEMDEC     = 38,
        GETPROP     = 39,
        SETPROP     = 40,
        GETELEM     = 41,
        SETELEM     = 42,
        CALL        = 43,
        NAME        = 44,
        NUMBER      = 45,
        STRING      = 46,
        ZERO        = 47,
        ONE         = 48,
        NULL        = 49,
        THIS        = 50,
        FALSE       = 51,
        TRUE        = 52,
        SHEQ        = 53,   // shallow equality (===)
        SHNE        = 54,   // shallow inequality (!==)
        CLOSURE     = 55,
        OBJECT      = 56,
        POP         = 57,
        POS         = 58,
        VARINC      = 59,
        VARDEC      = 60,
        BINDNAME    = 61,
        THROW       = 62,
        IN          = 63,
        INSTANCEOF  = 64,
        GOSUB       = 65,
        RETSUB      = 66,
        CALLSPECIAL = 67,
        GETTHIS     = 68,
        NEWTEMP     = 69,
        USETEMP     = 70,
        GETBASE     = 71,
        GETVAR      = 72,
        SETVAR      = 73,
        UNDEFINED   = 74,
        TRY         = 75,
        ENDTRY      = 76,
        NEWSCOPE    = 77,
        TYPEOFNAME  = 78,
        ENUMINIT    = 79,
        ENUMNEXT    = 80,
        GETPROTO    = 81,
        GETPARENT   = 82,
        SETPROTO    = 83,
        SETPARENT   = 84,
        SCOPE       = 85,
        GETSCOPEPARENT = 86,
        THISFN      = 87,
        JTHROW      = 88,
        // End of interpreter bytecodes
        SEMI        = 89,  // semicolon
        LB          = 90,  // left and right brackets
        RB          = 91,
        LC          = 92,  // left and right curlies (braces)
        RC          = 93,
        LP          = 94,  // left and right parentheses
        RP          = 95,
        COMMA       = 96,  // comma operator
        ASSIGN      = 97, // assignment ops (= += -= etc.)
        HOOK        = 98, // conditional (?:)
        COLON       = 99,
        OR          = 100, // logical or (||)
        AND         = 101, // logical and (&&)
        EQOP        = 102, // equality ops (== !=)
        RELOP       = 103, // relational ops (< <= > >=)
        SHOP        = 104, // shift ops (<< >> >>>)
        UNARYOP     = 105, // unary prefix operator
        INC         = 106, // increment/decrement (++ --)
        DEC         = 107,
        DOT         = 108, // member operator (.)
        PRIMARY     = 109, // true, false, null, this
        FUNCTION    = 110, // function keyword
        EXPORT      = 111, // export keyword
        IMPORT      = 112, // import keyword
        IF          = 113, // if keyword
        ELSE        = 114, // else keyword
        SWITCH      = 115, // switch keyword
        CASE        = 116, // case keyword
        DEFAULT     = 117, // default keyword
        WHILE       = 118, // while keyword
        DO          = 119, // do keyword
        FOR         = 120, // for keyword
        BREAK       = 121, // break keyword
        CONTINUE    = 122, // continue keyword
        VAR         = 123, // var keyword
        WITH        = 124, // with keyword
        CATCH       = 125, // catch keyword
        FINALLY     = 126, // finally keyword
        RESERVED    = 127, // reserved keywords

        /** Added by Mike - these are JSOPs in the jsref, but I
         * don't have them yet in the java implementation...
         * so they go here.  Also whatever I needed.

         * Most of these go in the 'op' field when returning
         * more general token types, eg. 'DIV' as the op of 'ASSIGN'.
         */
        NOP         = 128, // NOP
        NOT         = 129, // etc.
        PRE         = 130, // for INC, DEC nodes.
        POST        = 131,

        /**
         * For JSOPs associated with keywords...
         * eg. op = THIS; token = PRIMARY
         */

        VOID        = 132,

        /* types used for the parse tree - these never get returned
         * by the scanner.
         */
        BLOCK       = 133, // statement block
        ARRAYLIT    = 134, // array literal
        OBJLIT      = 135, // object literal
        LABEL       = 136, // label
        TARGET      = 137,
        LOOP        = 138,
        ENUMDONE    = 139,
        EXPRSTMT    = 140,
        PARENT      = 141,
        CONVERT     = 142,
        JSR         = 143,
        NEWLOCAL    = 144,
        USELOCAL    = 145,
        SCRIPT      = 146,   // top-level node for entire script
        
        /**
         * For the interpreted mode indicating a line number change in icodes.
         */
        LINE        = 147,
        SOURCEFILE  = 148,
        
        // For debugger
        
        BREAKPOINT  = 149;
    // end enum


    /* for mapping int token types to printable strings.
     * make sure to add 1 to index before using these!
     */
    private static String names[];
    private static void checkNames() {
        if (Context.printTrees && names == null) {
            String[] a = {
                "error",
                "eof",
                "eol",
                "popv",
                "enterwith",
                "leavewith",
                "return",
                "goto",
                "ifeq",
                "ifne",
                "dup",
                "setname",
                "bitor",
                "bitxor",
                "bitand",
                "eq",
                "ne",
                "lt",
                "le",
                "gt",
                "ge",
                "lsh",
                "rsh",
                "ursh",
                "add",
                "sub",
                "mul",
                "div",
                "mod",
                "bitnot",
                "neg",
                "new",
                "delprop",
                "typeof",
                "nameinc",
                "propinc",
                "eleminc",
                "namedec",
                "propdec",
                "elemdec",
                "getprop",
                "setprop",
                "getelem",
                "setelem",
                "call",
                "name",
                "number",
                "string",
                "zero",
                "one",
                "null",
                "this",
                "false",
                "true",
                "sheq",
                "shne",
                "closure",
                "object",
                "pop",
                "pos",
                "varinc",
                "vardec",
                "bindname",
                "throw",
                "in",
                "instanceof",
                "gosub",
                "retsub",
                "callspecial",
                "getthis",
                "newtemp",
                "usetemp",
                "getbase",
                "getvar",
                "setvar",
                "undefined",
                "try",
                "endtry",
                "newscope",
                "typeofname",
                "enuminit",
                "enumnext",
                "getproto",
                "getparent",
                "setproto",
                "setparent",
                "scope",
                "getscopeparent",
                "thisfn",
                "jthrow",
                "semi",
                "lb",
                "rb",
                "lc",
                "rc",
                "lp",
                "rp",
                "comma",
                "assign",
                "hook",
                "colon",
                "or",
                "and",
                "eqop",
                "relop",
                "shop",
                "unaryop",
                "inc",
                "dec",
                "dot",
                "primary",
                "function",
                "export",
                "import",
                "if",
                "else",
                "switch",
                "case",
                "default",
                "while",
                "do",
                "for",
                "break",
                "continue",
                "var",
                "with",
                "catch",
                "finally",
                "reserved",
                "nop",
                "not",
                "pre",
                "post",
                "void",
                "block",
                "arraylit",
                "objlit",
                "label",
                "target",
                "loop",
                "enumdone",
                "exprstmt",
                "parent",
                "convert",
                "jsr",
                "newlocal",
                "uselocal",
                "script",
                "line",
                "sourcefile",
            };
            names = a;
        }
    }

    /* This function uses the cached op, string and number fields in
     * TokenStream; if getToken has been called since the passed token
     * was scanned, the op or string printed may be incorrect.
     */
    public String tokenToString(int token) {
        if (Context.printTrees) {
            checkNames();
            if (token + 1 >= names.length)
                return null;

            if (token == UNARYOP ||
                token == ASSIGN ||
                token == PRIMARY ||
                token == EQOP ||
                token == SHOP ||
                token == RELOP) {
                return names[token + 1] + " " + names[this.op + 1];
            }

            if (token == STRING || token == OBJECT || token == NAME)
                return names[token + 1] + " `" + this.string + "'";

            if (token == NUMBER)
                return "NUMBER " + this.number;

            return names[token + 1];
        }
        return "";
    }

    public static String tokenToName(int type) {
        checkNames();
        return names == null ? "" : names[type + 1];
    }

    private static java.util.Hashtable keywords;

    static {
        String[] strings = {
            "break",
            "case",
            "continue",
            "default",
            "delete",
            "do",
            "else",
            "export",
            "false",
            "for",
            "function",
            "if",
            "in",
            "new",
            "null",
            "return",
            "switch",
            "this",
            "true",
            "typeof",
            "var",
            "void",
            "while",
            "with",

            // the following are #ifdef RESERVE_JAVA_KEYWORDS in jsscan.c
            "abstract",
            "boolean",
            "byte",
            "catch",
            "char",
            "class",
            "const",
            "debugger",
            "double",
            "enum",
            "extends",
            "final",
            "finally",
            "float",
            "goto",
            "implements",
            "import",
            "instanceof",
            "int",
            "interface",
            "long",
            "native",
            "package",
            "private",
            "protected",
            "public",
            "short",
            "static",
            "super",
            "synchronized",
            "throw",
            "throws",
            "transient",
            "try",
            "volatile"
        };
        int[] values = {
            BREAK,	                // break
            CASE,	                // case
            CONTINUE,	                // continue
            DEFAULT,	                // default
            DELPROP,	                // delete
            DO,	                        // do
            ELSE,	                // else
            EXPORT,	                // export
            PRIMARY | (FALSE << 8),	// false
            FOR,	                // for
            FUNCTION,	                // function
            IF,	                        // if
            RELOP | (IN << 8),	        // in
            NEW,	                // new
            PRIMARY | (NULL << 8),	// null
            RETURN,	                // return
            SWITCH,	                // switch
            PRIMARY | (THIS << 8),	// this
            PRIMARY | (TRUE << 8),	// true
            UNARYOP | (TYPEOF << 8),	// typeof
            VAR,	                // var
            UNARYOP | (VOID << 8),	// void
            WHILE,	                // while
            WITH,	                // with
            RESERVED,	                // abstract
            RESERVED,	                // boolean
            RESERVED,	                // byte
            CATCH,	                // catch
            RESERVED,	                // char
            RESERVED,	                // class
            RESERVED,	                // const
            RESERVED,	                // debugger
            RESERVED,	                // double
            RESERVED,	                // enum
            RESERVED,	                // extends
            RESERVED,	                // final
            FINALLY,	                // finally
            RESERVED,	                // float
            RESERVED,	                // goto
            RESERVED,	                // implements
            IMPORT,	                // import
            RELOP | (INSTANCEOF << 8),	// instanceof
            RESERVED,	                // int
            RESERVED,	                // interface
            RESERVED,	                // long
            RESERVED,	                // native
            RESERVED,	                // package
            RESERVED,	                // private
            RESERVED,	                // protected
            RESERVED,	                // public
            RESERVED,	                // short
            RESERVED,	                // static
            RESERVED,                   // super
            RESERVED,	                // synchronized
            THROW,	                // throw
            RESERVED,	                // throws
            RESERVED,	                // transient
            TRY,	                // try
            RESERVED	                // volatile
        };
        keywords = new java.util.Hashtable(strings.length);
        Integer res = new Integer(RESERVED);
        for (int i=0; i < strings.length; i++)
            keywords.put(strings[i], values[i] == RESERVED
                                     ? res
                                     : new Integer(values[i]));
    }

    private int stringToKeyword(String name) {
        Integer result = (Integer) keywords.get(name);
        if (result == null)
            return EOF;
        int x = result.intValue();
        this.op = x >> 8;
        return x & 0xff;
    }

    public TokenStream(Reader in, Scriptable scope,
                       String sourceName, int lineno)
    {
        this.in = new LineBuffer(in, lineno);
        this.scope = scope;
        this.pushbackToken = EOF;
        this.sourceName = sourceName;
        flags = 0;
    }
    
    public Scriptable getScope() { 
        return scope;
    }

    /* return and pop the token from the stream if it matches...
     * otherwise return null
     */
    public boolean matchToken(int toMatch) throws IOException {
        int token = getToken();
        if (token == toMatch)
            return true;

        // didn't match, push back token
        tokenno--;
        this.pushbackToken = token;
        return false;
    }

    public void clearPushback() {
        this.pushbackToken = EOF;
    }

    public void ungetToken(int tt) {
        if (this.pushbackToken != EOF && tt != ERROR) {
            String message = Context.getMessage2("msg.token.replaces.pushback",
                tokenToString(tt), tokenToString(this.pushbackToken));
            throw new RuntimeException(message);
        }
        this.pushbackToken = tt;
        tokenno--;
    }

    public int peekToken() throws IOException {
        int result = getToken();

        this.pushbackToken = result;
        tokenno--;
        return result;
    }

    public int peekTokenSameLine() throws IOException {
        int result;

        flags |= TSF_NEWLINES;          // SCAN_NEWLINES from jsscan.h
        result = peekToken();
        flags &= ~TSF_NEWLINES;         // HIDE_NEWLINES from jsscan.h
        if (this.pushbackToken == EOL)
            this.pushbackToken = EOF;
        return result;
    }

    protected static boolean isJSIdentifier(String s) {
        int length = s.length();

        if (length == 0 || !Character.isJavaIdentifierStart(s.charAt(0)))
            return false;

        for (int i=1; i<length; i++) {
			char c = s.charAt(i);
            if (!Character.isJavaIdentifierPart(c))
				if (c == '\\')
					if (! ((i + 5) < length)
							&& (s.charAt(i + 1) == 'u')
							&& isXDigit(s.charAt(i + 2))
							&& isXDigit(s.charAt(i + 3))
							&& isXDigit(s.charAt(i + 4))
							&& isXDigit(s.charAt(i + 5)))
					
                return false;
        }

        return true;
    }

    private static boolean isAlpha(int c) {
        return ((c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z'));
    }

    static boolean isDigit(int c) {
        return (c >= '0' && c <= '9');
    }

    static boolean isXDigit(int c) {
        return ((c >= '0' && c <= '9')
                || (c >= 'a' && c <= 'f')
                || (c >= 'A' && c <= 'F'));
    }

    /* As defined in ECMA.  jsscan.c uses C isspace() (which allows
     * \v, I think.)  note that code in in.read() implicitly accepts
     * '\r' == \u000D as well.
     */
    public static boolean isJSSpace(int c) {
        return (c == '\u0020' || c == '\u0009'
                || c == '\u000C' || c == '\u000B'
                || c == '\u00A0' 
                || Character.getType((char)c) == Character.SPACE_SEPARATOR);
    }

    public static boolean isJSLineTerminator(int c) {
        return (c == '\n' || c == '\r'
                || c == 0x2028 || c == 0x2029);
    }
    
    public int getToken() throws IOException {
        int c;
        tokenno++;

        // Check for pushed-back token
        if (this.pushbackToken != EOF) {
            int result = this.pushbackToken;
            this.pushbackToken = EOF;
            return result;
        }

        // Eat whitespace, possibly sensitive to newlines.
        do {
            c = in.read();
            if (c == '\n')
                if ((flags & TSF_NEWLINES) != 0)
                    break;
        } while (isJSSpace(c) || c == '\n');

        if (c == EOF_CHAR)
            return EOF;

        // identifier/keyword/instanceof?
		// watch out for starting with a <backslash>
		boolean isUnicodeEscapeStart = false;
		if (c == '\\') {
			c = in.read();
			if (c == 'u')
				isUnicodeEscapeStart = true;
			else
				c = '\\';
			// always unread the 'u' or whatever, we need 
			// to start the string below at the <backslash>.
			in.unread();
		}
        if (isUnicodeEscapeStart ||
					Character.isJavaIdentifierStart((char)c)) {
            in.startString();

			boolean containsEscape = isUnicodeEscapeStart;			
			do {
                c = in.read();
				if (c == '\\') {
					c = in.read();
					containsEscape = (c == 'u');
				}					
            } while (Character.isJavaIdentifierPart((char)c));
            in.unread();

            int result;

            String str = in.getString();
            // OPT we shouldn't have to make a string (object!) to
            // check if it's a keyword.
			
			// strictly speaking we should probably push-back
			// all the bad characters if the <backslash>uXXXX  
			// sequence is malformed. But since there isn't a  
			// correct context(is there?) for a bad Unicode  
			// escape sequence after an identifier, we can report
			// an error here.
			if (containsEscape) {
				char ca[] = str.toCharArray();
				StringBuffer x = new StringBuffer();
				int escStart = str.indexOf("\\u");
				int start = 0;
				while (escStart != -1) {
					x.append(ca, start, escStart);
					boolean goodEscape = false;
					if ((escStart + 5) < ca.length) {
						if (isXDigit(ca[escStart + 2])) {
							int val = Character.digit(ca[escStart + 2], 16);
							if (isXDigit(ca[escStart + 3])) {
								val = (val << 4) | Character.digit(ca[escStart + 3], 16);
								if (isXDigit(ca[escStart + 4])) {
									val = (val << 4) | Character.digit(ca[escStart + 4], 16);
									if (isXDigit(ca[escStart + 5])) {
										val = (val << 4) | Character.digit(ca[escStart + 5], 16);
										x.append((char)val);
										start = escStart + 6;
										goodEscape = true;
									}
								}
							}
						}
					}
					if (!goodEscape) {
						reportSyntaxError("msg.invalid.escape", null);
						return ERROR;
					}
					escStart = str.indexOf("\\u", start);
				}
				x.append(ca, start, ca.length - start);
				str = x.toString();
			}
			else
				// Return the corresponding token if it's a keyword
				if ((result = stringToKeyword(str)) != EOF) {
				    return result;
				}

            this.string = str;
            return NAME;
        }

        // is it a number?
        if (isDigit(c) || (c == '.' && isDigit(in.peek()))) {
            int base = 10;
            in.startString();

            double dval = ScriptRuntime.NaN;
            long longval = 0;
            boolean isInteger = true;

            if (c == '0') {
                c = in.read();
                if (c == 'x' || c == 'X') {
                    c = in.read();
                    base = 16;
                    // restart the string, losing leading 0x
                    in.startString();
                } else if (isDigit(c)) {
                    base = 8;
                }
            }

            while (isXDigit(c)) {
                if (base < 16) {
                    if (isAlpha(c))
                        break;
                    /*
                     * We permit 08 and 09 as decimal numbers, which
                     * makes our behavior a superset of the ECMA
                     * numeric grammar.  We might not always be so
                     * permissive, so we warn about it.
                     */
                    if (base == 8 && c >= '8') {
                        Object[] errArgs = { c == '8' ? "8" : "9" };
                        Context.reportWarning(
                            Context.getMessage("msg.bad.octal.literal",
                                               errArgs),
                            getSourceName(),
                            in.getLineno(), getLine(), getOffset());
                        base = 10;
                    }
                }
                c = in.read();
            }

            if (base == 10 && (c == '.' || c == 'e' || c == 'E')) {
                isInteger = false;
                if (c == '.') {
                    do {
                        c = in.read();
                    } while (isDigit(c));
                }

                if (c == 'e' || c == 'E') {
                    c = in.read();
                    if (c == '+' || c == '-') {
                        c = in.read();
                    }
                    if (!isDigit(c)) {
                        in.getString(); // throw away string in progress
                        reportSyntaxError("msg.missing.exponent", null);
                        return ERROR;
                    }
                    do {
                        c = in.read();
                    } while (isDigit(c));
                }
            }
            in.unread();
            String numString = in.getString();

            if (base == 10 && !isInteger) {
                try {
                    // Use Java conversion to number from string...
                    dval = (Double.valueOf(numString)).doubleValue();
                }
                catch (NumberFormatException ex) {
                    Object[] errArgs = { ex.getMessage() };
                    reportSyntaxError("msg.caught.nfe", errArgs);
                    return ERROR;
                }
            } else {
                dval = ScriptRuntime.stringToNumber(numString, 0, base);
                longval = (long) dval;

                // is it an integral fits-in-a-long value?
                if (longval != dval)
                    isInteger = false;
            }

            if (!isInteger) {
                /* Can't handle floats right now, because postfix INC/DEC
                   generate Doubles, but I would generate a Float through this
                   path, and it causes a stack mismatch. FIXME (MS)
                   if (Float.MIN_VALUE <= dval && dval <= Float.MAX_VALUE)
                   this.number = new Xloat((float) dval);
                   else
                */
                this.number = new Double(dval);
            } else {
                // We generate the smallest possible type here
                if (Byte.MIN_VALUE <= longval && longval <= Byte.MAX_VALUE)
                    this.number = new Byte((byte)longval);
                else if (Short.MIN_VALUE <= longval &&
                         longval <= Short.MAX_VALUE)
                    this.number = new Short((short)longval);
                else if (Integer.MIN_VALUE <= longval &&
                         longval <= Integer.MAX_VALUE)
                    this.number = new Integer((int)longval);
                else {
                    // May lose some precision here, but that's the 
                    // appropriate semantics.
                    this.number = new Double(longval);
                }
            }
            return NUMBER;
        }

        // is it a string?
        if (c == '"' || c == '\'') {
            // We attempt to accumulate a string the fast way, by
            // building it directly out of the reader.  But if there
            // are any escaped characters in the string, we revert to
            // building it out of a StringBuffer.

            StringBuffer stringBuf = null;

            int quoteChar = c;
            int val = 0;

            c = in.read();
            in.startString(); // start after the first "
            while(c != quoteChar) {
                if (c == '\n' || c == EOF_CHAR) {
                    in.unread();
                    in.getString(); // throw away the string in progress
                    reportSyntaxError("msg.unterminated.string.lit", null);
                    return ERROR;
                }

                if (c == '\\') {
                    // We've hit an escaped character; revert to the
                    // slow method of building a string.
                    if (stringBuf == null) {
                        // Don't include the backslash
                        in.unread();
                        stringBuf = new StringBuffer(in.getString());
                        in.read();
                    }

                    switch (c = in.read()) {
                    case 'b': c = '\b'; break;
                    case 'f': c = '\f'; break;
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case 'v': c = '\u000B'; break;
                        // \v a late addition to the ECMA spec.
                        // '\v' doesn't seem to be valid Java.

                    default:
                        if (isDigit(c) && c < '8') {
                            val = c - '0';
                            c = in.read();
                            if (isDigit(c) && c < '8') {
                                val = 8 * val + c - '0';
                                c = in.read();
                                if (isDigit(c) && c < '8') {
                                    val = 8 * val + c - '0';
                                    c = in.read();
                                }
                            }
                            in.unread();
                            if (val > 0377) {
                                reportSyntaxError("msg.oct.esc.too.large", null);
                                return ERROR;
                            }
                            c = val;
                        } else if (c == 'u') {
							/*
							 * Get 4 hex digits; if the u escape is not
							 * followed by 4 hex digits, use 'u' + the literal
							 * character sequence that follows.  Do some manual
							 * match (OK because we're in a string) to avoid
							 * multi-char match on the underlying stream.
							 */
							    int c1, c2, c3, c4;

							    c1 = in.read();
							    if (!isXDigit(c1)) {
							        in.unread();
							        c = 'u';
							    } else {
							        val = Character.digit((char) c1, 16);
							        c2 = in.read();
							        if (!isXDigit(c2)) {
							            in.unread();
							            stringBuf.append('u');
							            c = c1;
							        } else {
							            val = 16 * val
							                + Character.digit((char) c2, 16);
							            c3 = in.read();
							            if (!isXDigit(c3)) {
							                in.unread();
							                stringBuf.append('u');
							                stringBuf.append((char)c1);
							                c = c2;
							            } else {
							                val = 16 * val
							                    + Character.digit((char) c3, 16);
							                c4 = in.read();
							                if (!isXDigit(c4)) {
							                    in.unread();
							                    stringBuf.append('u');
							                    stringBuf.append((char)c1);
							                    stringBuf.append((char)c2);
							                    c = c3;
							                } else {
							                    // got 4 hex digits! Woo Hoo!
							                    val = 16 * val
							                        + Character.digit((char) c4, 16);
							                    c = val;
							                }
							            }
							        }
							    }
                        } else if (c == 'x') {
                            /* Get 2 hex digits, defaulting to 'x' + literal
                             * sequence, as above.
                             */
                            int c1, c2;

                            c1 = in.read();
                            if (!isXDigit(c1)) {
                                in.unread();
                                c = 'x';
                            } else {
                                val = Character.digit((char) c1, 16);
                                c2 = in.read();
                                if (!isXDigit(c2)) {
                                    in.unread();
                                    stringBuf.append('x');
                                    c = c1;
                                } else {
                                    // got 2 hex digits
                                    val = 16 * val
                                        + Character.digit((char) c2, 16);
                                    c = val;
                                }
                            }
                        }
                    }
                }
                
                if (stringBuf != null)
                    stringBuf.append((char) c);
                c = in.read();
            }

            if (stringBuf != null)
                this.string = stringBuf.toString();
            else {
                in.unread(); // miss the trailing "
                this.string = in.getString();
                in.read();
            }
            return STRING;
        }

        switch (c)
        {
        case '\n': return EOL;
        case ';': return SEMI;
        case '[': return LB;
        case ']': return RB;
        case '{': return LC;
        case '}': return RC;
        case '(': return LP;
        case ')': return RP;
        case ',': return COMMA;
        case '?': return HOOK;
        case ':': return COLON;
        case '.': return DOT;

        case '|':
            if (in.match('|')) {
                return OR;
            } else if (in.match('=')) {
                this.op = BITOR;
                return ASSIGN;
            } else {
                return BITOR;
            }

        case '^':
            if (in.match('=')) {
                this.op = BITXOR;
                return ASSIGN;
            } else {
                return BITXOR;
            }

        case '&':
            if (in.match('&')) {
                return AND;
            } else if (in.match('=')) {
                this.op = BITAND;
                return ASSIGN;
            } else {
                return BITAND;
            }

        case '=':
            if (in.match('=')) {
                if (in.match('='))
                    this.op = SHEQ;
                else
                    this.op = EQ;
                return EQOP;
            } else {
                this.op = NOP;
                return ASSIGN;
            }

        case '!':
            if (in.match('=')) {
                if (in.match('='))
                    this.op = SHNE;
                else
                    this.op = NE;
                return EQOP;
            } else {
                this.op = NOT;
                return UNARYOP;
            }

        case '<':
            /* NB:treat HTML begin-comment as comment-till-eol */
            if (in.match('!')) {
                if (in.match('-')) {
                    if (in.match('-')) {
                        while ((c = in.read()) != EOF_CHAR && c != '\n')
                            /* skip to end of line */;
                        in.unread();
                        return getToken();  // in place of 'goto retry'
                    }
                    in.unread();
                }
                in.unread();
            }
            if (in.match('<')) {
                if (in.match('=')) {
                    this.op = LSH;
                    return ASSIGN;
                } else {
                    this.op = LSH;
                    return SHOP;
                }
            } else {
                if (in.match('=')) {
                    this.op = LE;
                    return RELOP;
                } else {
                    this.op = LT;
                    return RELOP;
                }
            }

        case '>':
            if (in.match('>')) {
                if (in.match('>')) {
                    if (in.match('=')) {
                        this.op = URSH;
                        return ASSIGN;
                    } else {
                        this.op = URSH;
                        return SHOP;
                    }
                } else {
                    if (in.match('=')) {
                        this.op = RSH;
                        return ASSIGN;
                    } else {
                        this.op = RSH;
                        return SHOP;
                    }
                }
            } else {
                if (in.match('=')) {
                    this.op = GE;
                    return RELOP;
                } else {
                    this.op = GT;
                    return RELOP;
                }
            }

        case '*':
            if (in.match('=')) {
                this.op = MUL;
                return ASSIGN;
            } else {
                return MUL;
            }

        case '/':
            // is it a // comment?
            if (in.match('/')) {
                while ((c = in.read()) != EOF_CHAR && c != '\n')
                    /* skip to end of line */;
                in.unread();
                return getToken();
            }
            if (in.match('*')) {
                while ((c = in.read()) != -1
                       && !(c == '*' && in.match('/'))) {
                    if (c == '\n') {
                    } else if (c == '/' && in.match('*')) {
                        if (in.match('/'))
                            return getToken();
                        reportSyntaxError("msg.nested.comment", null);
                        return ERROR;
                    }
                }
                if (c == EOF_CHAR) {
                    reportSyntaxError("msg.unterminated.comment", null);
                    return ERROR;
                }
                return getToken();  // `goto retry'
            }

            // is it a regexp?
            if ((flags & TSF_REGEXP) != 0) {
                // We don't try to use the in.startString/in.getString
                // approach, because escaped characters (which break it)
                // seem likely to be common.
                StringBuffer re = new StringBuffer();
                while ((c = in.read()) != '/') {
                    if (c == '\n' || c == EOF_CHAR) {
                        in.unread();
                        reportSyntaxError("msg.unterminated.re.lit", null);
                        return ERROR;
                    }
                    if (c == '\\') {
                        re.append((char) c);
                        c = in.read();
                    }

                    re.append((char) c);
                }

                StringBuffer flagsBuf = new StringBuffer();
                while (true) {
                    if (in.match('g'))
                        flagsBuf.append('g');
                    else if (in.match('i'))
                        flagsBuf.append('i');
                    else if (in.match('m'))
                        flagsBuf.append('m');
                    else
                        break;
                }

                if (isAlpha(in.peek())) {
                    reportSyntaxError("msg.invalid.re.flag", null);
                    return ERROR;
                }

                this.string = re.toString();
                this.regExpFlags = flagsBuf.toString();
                return OBJECT;
            }


            if (in.match('=')) {
                this.op = DIV;
                return ASSIGN;
            } else {
                return DIV;
            }

        case '%':
            this.op = MOD;
            if (in.match('=')) {
                return ASSIGN;
            } else {
                return MOD;
            }

        case '~':
            this.op = BITNOT;
            return UNARYOP;

        case '+':
        case '-':
            if (in.match('=')) {
                if (c == '+') {
                    this.op = ADD;
                    return ASSIGN;
                } else {
                    this.op = SUB;
                    return ASSIGN;
                }
            } else if (in.match((char) c)) {
                if (c == '+') {
                    return INC;
                } else {
                    return DEC;
                }
            } else if (c == '-') {
                return SUB;
            } else {
                return ADD;
            }

        default:
            reportSyntaxError("msg.illegal.character", null);
            return ERROR;
        }
    }

    public void reportSyntaxError(String messageProperty, Object[] args) {
        String message = Context.getMessage(messageProperty, args);
        if (scope != null) {
            // We're probably in an eval. Need to throw an exception.
            throw NativeGlobal.constructError(
                            Context.getContext(), "SyntaxError",
                            message, scope, getSourceName(),
                            getLineno(), getOffset(), getLine());
        } else {
            Context.reportError(message, getSourceName(),
                                getLineno(), getLine(), getOffset());
        }
    }

    public String getSourceName() { return sourceName; }
    public int getLineno() { return in.getLineno(); }
    public int getOp() { return op; }
    public String getString() { return string; }
    public Number getNumber() { return number; }
    public String getLine() { return in.getLine(); }
    public int getOffset() { return in.getOffset(); }
    public int getTokenno() { return tokenno; }
    public boolean eof() { return in.eof(); }

    // instance variables
    private LineBuffer in;


    /* for TSF_REGEXP, etc.
     * should this be manipulated by gettor/settor functions?
     * should it be passed to getToken();
     */
    public int flags;
    public String regExpFlags;

    private String sourceName;
    private String line;
    private Scriptable scope;
    private int pushbackToken;
    private int tokenno;

    private int op;

    // Set this to an inital non-null value so that the Parser has
    // something to retrieve even if an error has occured and no
    // string is found.  Fosters one class of error, but saves lots of
    // code.
    private String string = "";
    private Number number;
}
