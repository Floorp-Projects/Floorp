/* -*- Mode: Java; tab-width: 8 -*-
 * Copyright © 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

import java.io.*;

/**
 * This class implements the JavaScript scanner.
 *
 * It is based on the C source files jsscan.c and jsscan.h
 * in the jsref package.
 *
 * @see com.netscape.javascript.Parser
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
        TSF_ERROR       = 0x0001,  // fatal error while scanning
//         TSF_EOF         = 0x0002,  // hit end of file
        TSF_NEWLINES    = 0x0004,  // tokenize newlines
        TSF_FUNCTION    = 0x0008,  // scanning inside function body
        TSF_RETURN_EXPR = 0x0010,  // function has 'return expr;'
        TSF_RETURN_VOID = 0x0020,  // function has 'return;'
//         TSF_INTERACTIVE = 0x0040,  // interactive parsing mode
//         TSF_COMMAND     = 0x0080,  // command parsing mode
//         TSF_LOOKAHEAD   = 0x0100,  // looking ahead for a token
        TSF_REGEXP      = 0x0200;  // looking for a regular expression

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
        JTHROW      = 87,
        // End of interpreter bytecodes
        SEMI        = 88,  // semicolon
        LB          = 89,  // left and right brackets
        RB          = 90,
        LC          = 91,  // left and right curlies (braces)
        RC          = 92,
        LP          = 93,  // left and right parentheses
        RP          = 94,
        COMMA       = 95,  // comma operator
        ASSIGN      = 96, // assignment ops (= += -= etc.)
        HOOK        = 97, // conditional (?:)
        COLON       = 98,
        OR          = 99, // logical or (||)
        AND         = 100, // logical and (&&)
        EQOP        = 101, // equality ops (== !=)
        RELOP       = 102, // relational ops (< <= > >=)
        SHOP        = 103, // shift ops (<< >> >>>)
        UNARYOP     = 104, // unary prefix operator
        INC         = 105, // increment/decrement (++ --)
        DEC         = 106,
        DOT         = 107, // member operator (.)
        PRIMARY     = 108, // true, false, null, this, super
        FUNCTION    = 109, // function keyword
        EXPORT      = 110, // export keyword
        IMPORT      = 111, // import keyword
        IF          = 112, // if keyword
        ELSE        = 113, // else keyword
        SWITCH      = 114, // switch keyword
        CASE        = 115, // case keyword
        DEFAULT     = 116, // default keyword
        WHILE       = 117, // while keyword
        DO          = 118, // do keyword
        FOR         = 119, // for keyword
        BREAK       = 120, // break keyword
        CONTINUE    = 121, // continue keyword
        VAR         = 122, // var keyword
        WITH        = 123, // with keyword
        CATCH       = 124, // catch keyword
        FINALLY     = 125, // finally keyword
        RESERVED    = 126, // reserved keywords

        /** Added by Mike - these are JSOPs in the jsref, but I
         * don't have them yet in the java implementation...
         * so they go here.  Also whatever I needed.

         * Most of these go in the 'op' field when returning
         * more general token types, eg. 'DIV' as the op of 'ASSIGN'.
         */
        NOP         = 127, // NOP
        NOT         = 128, // etc.
        PRE         = 129, // for INC, DEC nodes.
        POST        = 130,

        /**
         * For JSOPs associated with keywords...
         * eg. op = THIS; token = PRIMARY
         */

        VOID        = 131,

        /* types used for the parse tree - these never get returned
         * by the scanner.
         */
        BLOCK       = 132, // statement block
        ARRAYLIT    = 133, // array literal
        OBJLIT      = 134, // object literal
        LABEL       = 135, // label
        TARGET      = 136,
        LOOP        = 137,
        ENUMDONE    = 138,
        EXPRSTMT    = 139,
        PARENT      = 140,
        CONVERT     = 141,
        JSR         = 142,
        NEWLOCAL    = 143,
        USELOCAL    = 144,
        SCRIPT      = 145;   // top-level node for entire script
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
                "script"
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
            PRIMARY | (NOP << 8),	// super
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

    public TokenStream(Reader in,
                       String sourceName, int lineno)
    {
        this.in = new LineBuffer(in, lineno);

        this.pushbackToken = EOF;
        this.sourceName = sourceName;
        flags = 0;
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
            Object[] errArgs = { tokenToString(tt),
                                 tokenToString(this.pushbackToken) };
            String message = Context.getMessage("msg.token.replaces.pushback",
                                                errArgs);
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

    /* helper functions... these might be better inlined.
     * These are needed because the java.lang.Character.isWhatever
     * functions accept unicode, and we want to limit
     * identifiers to ASCII.
     */
    protected static boolean isJSIdentifier(String s) {
        int length = s.length();

        if (length == 0 || !isJSIdentifierStart(s.charAt(0)))
            return false;

        for (int i=1; i<length; i++) {
            if (!isJSIdentifierPart(s.charAt(i)))
                return false;
        }

        return true;
    }

    protected static boolean isJSIdentifierStart(int c) {
        return ((c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z')
                || c == '_'
                || c == '$');
    }

    protected static boolean isJSIdentifierPart(int c) {
        return ((c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9')
                || c == '_'
                || c == '$');
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
    static boolean isJSSpace(int c) {
        return (c == '\u0009' || c == '\u000B'
                || c == '\u000C' || c == '\u0020');
    }

    public int getToken() throws IOException {
        int c;
        tokenno++;
        // If there was a fatal error, keep returning TOK_ERROR.
        if ((flags & TSF_ERROR) != 0)
            return ERROR;

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
        if (isJSIdentifierStart(c)) {
            in.startString();
            do {
                c = in.read();
            } while (isJSIdentifierPart(c));
            in.unread();

            int result;

            String str = in.getString();
            // OPT we shouldn't have to make a string (object!) to
            // check if it's a keyword.

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
                    if (c < '8') {
                        base = 8;
                        // Restart the string, losing the leading 0
                        in.startString();
                    } else {
                        /* Checking against c < '8' is non-ECMA, but
                         * is required to support legacy code; we've
                         * supported it in the past, and it's likely
                         * that "08" and "09" are in use in code
                         * having to do with dates.  So we need to
                         * support it, which makes our behavior a
                         * superset of ECMA in this area.  We raise a
                         * warning if a non-octal digit is
                         * encountered, then proceed as if it were
                         * decimal.
                         */
                        Object[] errArgs = { String.valueOf((char)c) };
                        Context.reportWarning
                            (Context.getMessage("msg.bad.octal.literal",
                                                errArgs),
                             getSourceName(), in.getLineno(),
                             getLine(), getOffset());
                        // implicitly retain the leading 0
                    }
                } else {
                    // implicitly retain the leading 0
                }
            }

            while (isXDigit(c)) {
                if (base < 16 && (isAlpha(c)
                                  || (base == 8 && c >= '8'))) {
                    break;
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
                        reportError("msg.missing.exponent", null);
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
                    reportError("msg.caught.nfe", errArgs);
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
                    reportError("msg.unterminated.string.lit", null);
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
                                reportError("msg.oct.esc.too.large", null);
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
                        reportError("msg.nested.comment", null);
                        return ERROR;
                    }
                }
                if (c == EOF_CHAR) {
                    reportError("msg.unterminated.comment", null);
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
                        reportError("msg.unterminated.re.lit", null);
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
                    reportError("msg.invalid.re.flag", null);
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
            reportError("msg.illegal.character", null);
            return ERROR;
        }
    }

    private void reportError(String messageProperty, Object[] args) {
        flags |= TSF_ERROR;
        String message = Context.getMessage(messageProperty, args);
        Context.reportError(message, getSourceName(),
                            getLineno(), getLine(), getOffset());
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
