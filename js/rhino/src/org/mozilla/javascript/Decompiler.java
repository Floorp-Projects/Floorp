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
 * Mike Ang
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

/**
 * The following class save decompilation information about the source.
 * Source information is returned from the parser as a String
 * associated with function nodes and with the toplevel script.  When
 * saved in the constant pool of a class, this string will be UTF-8
 * encoded, and token values will occupy a single byte.

 * Source is saved (mostly) as token numbers.  The tokens saved pretty
 * much correspond to the token stream of a 'canonical' representation
 * of the input program, as directed by the parser.  (There were a few
 * cases where tokens could have been left out where decompiler could
 * easily reconstruct them, but I left them in for clarity).  (I also
 * looked adding source collection to TokenStream instead, where I
 * could have limited the changes to a few lines in getToken... but
 * this wouldn't have saved any space in the resulting source
 * representation, and would have meant that I'd have to duplicate
 * parser logic in the decompiler to disambiguate situations where
 * newlines are important.)  NativeFunction.decompile expands the
 * tokens back into their string representations, using simple
 * lookahead to correct spacing and indentation.

 * Token types with associated ops (ASSIGN, SHOP, PRIMARY, etc.) are
 * saved as two-token pairs.  Number tokens are stored inline, as a
 * NUMBER token, a character representing the type, and either 1 or 4
 * characters representing the bit-encoding of the number.  String
 * types NAME, STRING and OBJECT are currently stored as a token type,
 * followed by a character giving the length of the string (assumed to
 * be less than 2^16), followed by the characters of the string
 * inlined into the source string.  Changing this to some reference to
 * to the string in the compiled class' constant pool would probably
 * save a lot of space... but would require some method of deriving
 * the final constant pool entry from information available at parse
 * time.

 * Nested functions need a similar mechanism... fortunately the nested
 * functions for a given function are generated in source order.
 * Nested functions are encoded as FUNCTION followed by a function
 * number (encoded as a character), which is enough information to
 * find the proper generated NativeFunction instance.

 */
class Decompiler
{

    void startScript()
    {
        sourceTop = 0;

        // Add script indicator
        addToken(Token.SCRIPT);
    }

    String stopScript()
    {
        String encoded = sourceToString(0);
        sourceBuffer = null; // It helpds GC
        sourceTop = 0;
        return encoded;
    }

    int startFunction(int functionIndex)
    {
        if (!(0 <= functionIndex))
            throw new IllegalArgumentException();

        // Add reference to the enclosing function/script
        addToken(Token.FUNCTION);
        append((char)functionIndex);

        return sourceTop;
    }

    String stopFunction(int savedTop)
    {
        if (!(0 <= savedTop && savedTop <= sourceTop))
            throw new IllegalArgumentException();

        String encoded = sourceToString(savedTop);
        sourceTop = savedTop;
        return encoded;
    }

    void addToken(int token)
    {
        if (!(0 <= token && token <= Token.LAST_TOKEN))
            throw new IllegalArgumentException();

        append((char)token);
    }

    void addEOL(int token)
    {
        if (!(0 <= token && token <= Token.LAST_TOKEN))
            throw new IllegalArgumentException();

        append((char)token);
        append((char)Token.EOL);
    }

    void addOp(int token, int op)
    {
        if (!(0 <= token && token <= Token.LAST_TOKEN))
            throw new IllegalArgumentException();
        if (!(0 <= op && op <= Token.LAST_TOKEN))
            throw new IllegalArgumentException();

        append((char)token);
        append((char)op);
    }

    void addName(String str)
    {
        addToken(Token.NAME);
        appendString(str);
    }

    void addString(String str)
    {
        addToken(Token.STRING);
        appendString(str);
    }

    void addRegexp(String regexp, String flags)
    {
        addToken(Token.REGEXP);
        appendString('/' + regexp + '/' + flags);
    }

    void addNumber(double n)
    {
        addToken(Token.NUMBER);

        /* encode the number in the source stream.
         * Save as NUMBER type (char | char char char char)
         * where type is
         * 'D' - double, 'S' - short, 'J' - long.

         * We need to retain float vs. integer type info to keep the
         * behavior of liveconnect type-guessing the same after
         * decompilation.  (Liveconnect tries to present 1.0 to Java
         * as a float/double)
         * OPT: This is no longer true. We could compress the format.

         * This may not be the most space-efficient encoding;
         * the chars created below may take up to 3 bytes in
         * constant pool UTF-8 encoding, so a Double could take
         * up to 12 bytes.
         */

        long lbits = (long)n;
        if (lbits != n) {
            // if it's floating point, save as a Double bit pattern.
            // (12/15/97 our scanner only returns Double for f.p.)
            lbits = Double.doubleToLongBits(n);
            append('D');
            append((char)(lbits >> 48));
            append((char)(lbits >> 32));
            append((char)(lbits >> 16));
            append((char)lbits);
        }
        else {
            // we can ignore negative values, bc they're already prefixed
            // by UNARYOP SUB
               if (lbits < 0) Context.codeBug();

            // will it fit in a char?
            // this gives a short encoding for integer values up to 2^16.
            if (lbits <= Character.MAX_VALUE) {
                append('S');
                append((char)lbits);
            }
            else { // Integral, but won't fit in a char. Store as a long.
                append('J');
                append((char)(lbits >> 48));
                append((char)(lbits >> 32));
                append((char)(lbits >> 16));
                append((char)lbits);
            }
        }
    }

    private void appendString(String str)
    {
        int L = str.length();
        int lengthEncodingSize = 1;
        if (L >= 0x8000) {
            lengthEncodingSize = 2;
        }
        int nextTop = sourceTop + lengthEncodingSize + L;
        if (nextTop > sourceBuffer.length) {
            increaseSourceCapacity(nextTop);
        }
        if (L >= 0x8000) {
            // Use 2 chars to encode strings exceeding 32K, were the highest
            // bit in the first char indicates presence of the next byte
            sourceBuffer[sourceTop] = (char)(0x8000 | (L >>> 16));
            ++sourceTop;
        }
        sourceBuffer[sourceTop] = (char)L;
        ++sourceTop;
        str.getChars(0, L, sourceBuffer, sourceTop);
        sourceTop = nextTop;
    }

    private void append(char c)
    {
        if (sourceTop == sourceBuffer.length) {
            increaseSourceCapacity(sourceTop + 1);
        }
        sourceBuffer[sourceTop] = c;
        ++sourceTop;
    }

    private void increaseSourceCapacity(int minimalCapacity)
    {
        // Call this only when capacity increase is must
        if (minimalCapacity <= sourceBuffer.length) Context.codeBug();
        int newCapacity = sourceBuffer.length * 2;
        if (newCapacity < minimalCapacity) {
            newCapacity = minimalCapacity;
        }
        char[] tmp = new char[newCapacity];
        System.arraycopy(sourceBuffer, 0, tmp, 0, sourceTop);
        sourceBuffer = tmp;
    }

    private String sourceToString(int offset)
    {
        if (offset < 0 || sourceTop < offset) Context.codeBug();
        return new String(sourceBuffer, offset, sourceTop - offset);
    }

    /**
     * Decompile the source information associated with this js
     * function/script back into a string.  For the most part, this
     * just means translating tokens back to their string
     * representations; there's a little bit of lookahead logic to
     * decide the proper spacing/indentation.  Most of the work in
     * mapping the original source to the prettyprinted decompiled
     * version is done by the parser.
     *
     * Note that support for Context.decompileFunctionBody is hacked
     * on through special cases; I suspect that js makes a distinction
     * between function header and function body that rhino
     * decompilation does not.
     *
     * @param encodedSourcesTree See {@link NativeFunction#getSourcesTree()}
     *        for definition
     *
     * @param justbody Whether the decompilation should omit the
     * function header and trailing brace.
     *
     * @param indent How much to indent the decompiled result
     *
     * @param indentGap the default identation offset
     *
     * @param indentGap the identation offset for case labels
     *
     */
    static String decompile(Object encodedSourcesTree, boolean justbody,
                            int indent, int indentGap, int caseGap)
    {
        StringBuffer result = new StringBuffer();
        decompile_r(encodedSourcesTree, false, justbody,
                    indent, indentGap, caseGap, result);
        return result.toString();
    }

    private static void decompile_r(Object encodedSourcesTree,
                                    boolean nested, boolean justbody,
                                    int indent, int indentGap, int caseGap,
                                    StringBuffer result)
    {
        String source;
        Object[] childNodes = null;
        if (encodedSourcesTree == null) {
            source = null;
        } else if (encodedSourcesTree instanceof String) {
            source = (String)encodedSourcesTree;
        } else {
            childNodes = (Object[])encodedSourcesTree;
            source = (String)childNodes[0];
        }

        if (source == null) { return; }

        int length = source.length();
        if (length == 0) { return; }

        // Spew tokens in source, for debugging.
        // as TYPE number char
        if (printSource) {
            System.err.println("length:" + length);
            for (int i = 0; i < length; ++i) {
                // Note that tokenToName will fail unless Context.printTrees
                // is true.
                String tokenname = null;
                if (Token.printNames) {
                    tokenname = Token.name(source.charAt(i));
                }
                if (tokenname == null) {
                    tokenname = "---";
                }
                String pad = tokenname.length() > 7
                    ? "\t"
                    : "\t\t";
                System.err.println
                    (tokenname
                     + pad + (int)source.charAt(i)
                     + "\t'" + ScriptRuntime.escapeString
                     (source.substring(i, i+1))
                     + "'");
            }
            System.err.println();
        }

        if (!nested) {
            // add an initial newline to exactly match js.
            if (!justbody)
                result.append('\n');
            for (int j = 0; j < indent; j++)
                result.append(' ');
        }

        int i = 0;

        // If the first token is Token.SCRIPT, then we're
        // decompiling the toplevel script, otherwise it a function
        // and should start with Token.FUNCTION

        int token = source.charAt(i);
        ++i;
        if (token == Token.FUNCTION) {
            if (!justbody) {
                result.append("function ");
            } else {
                // Skip past the entire function header pass the next EOL.
                skipLoop: for (;;) {
                    token = source.charAt(i);
                    ++i;
                    switch (token) {
                        case Token.EOL:
                            break skipLoop;
                        case Token.NAME:
                            // Skip function or argument name
                            i = getSourceStringEnd(source, i);
                            break;
                        case Token.LP:
                        case Token.COMMA:
                        case Token.RP:
                        case Token.LC:
                            break;
                        default:
                            // Bad function header
                            throw new RuntimeException();
                    }
                }
            }
        } else if (token != Token.SCRIPT) {
            // Bad source header
            throw new RuntimeException();
        }

        while (i < length) {
            switch(source.charAt(i)) {
            case Token.NAME:
            case Token.REGEXP:  // re-wrapped in '/'s in parser...
                i = printSourceString(source, i + 1, false, result);
                continue;

            case Token.STRING:
                i = printSourceString(source, i + 1, true, result);
                continue;

            case Token.NUMBER:
                i = printSourceNumber(source, i + 1, result);
                continue;

            case Token.PRIMARY:
                ++i;
                switch(source.charAt(i)) {
                case Token.TRUE:
                    result.append("true");
                    break;

                case Token.FALSE:
                    result.append("false");
                    break;

                case Token.NULL:
                    result.append("null");
                    break;

                case Token.THIS:
                    result.append("this");
                    break;

                case Token.TYPEOF:
                    result.append("typeof");
                    break;

                case Token.VOID:
                    result.append("void");
                    break;

                case Token.UNDEFINED:
                    result.append("undefined");
                    break;
                }
                break;

            case Token.FUNCTION: {
                /* decompile a FUNCTION token as an escape; call
                 * toString on the nth enclosed nested function,
                 * where n is given by the byte that follows.
                 */

                ++i;
                int functionIndex = source.charAt(i);
                if (childNodes == null
                    || functionIndex + 1 > childNodes.length)
                {
                    throw Context.reportRuntimeError(Context.getMessage1
                        ("msg.no.function.ref.found",
                         new Integer(functionIndex)));
                }
                decompile_r(childNodes[functionIndex + 1], true, false,
                            indent, indentGap, caseGap, result);
                break;
            }
            case Token.COMMA:
                result.append(", ");
                break;

            case Token.LC:
                if (nextIs(source, length, i, Token.EOL))
                    indent += indentGap;
                result.append('{');
                break;

            case Token.RC:
                /* don't print the closing RC if it closes the
                 * toplevel function and we're called from
                 * decompileFunctionBody.
                 */
                if (justbody && !nested && i + 1 == length)
                    break;

                if (nextIs(source, length, i, Token.EOL))
                    indent -= indentGap;
                if (nextIs(source, length, i, Token.WHILE)
                    || nextIs(source, length, i, Token.ELSE)) {
                    indent -= indentGap;
                    result.append("} ");
                }
                else
                    result.append('}');
                break;

            case Token.LP:
                result.append('(');
                break;

            case Token.RP:
                if (nextIs(source, length, i, Token.LC))
                    result.append(") ");
                else
                    result.append(')');
                break;

            case Token.LB:
                result.append('[');
                break;

            case Token.RB:
                result.append(']');
                break;

            case Token.EOL:
                result.append('\n');

                /* add indent if any tokens remain,
                 * less setback if next token is
                 * a label, case or default.
                 */
                if (i + 1 < length) {
                    int less = 0;
                    int nextToken = source.charAt(i + 1);
                    if (nextToken == Token.CASE
                        || nextToken == Token.DEFAULT)
                    {
                        less = indentGap - caseGap;
                    } else if (nextToken == Token.RC) {
                        less = indentGap;
                    }

                    /* elaborate check against label... skip past a
                     * following inlined NAME and look for a COLON.
                     * Depends on how NAME is encoded.
                     */
                    else if (nextToken == Token.NAME) {
                        int afterName = getSourceStringEnd(source, i + 2);
                        if (source.charAt(afterName) == Token.COLON)
                            less = indentGap;
                    }

                    for (; less < indent; less++)
                        result.append(' ');
                }
                break;

            case Token.DOT:
                result.append('.');
                break;

            case Token.NEW:
                result.append("new ");
                break;

            case Token.DELPROP:
                result.append("delete ");
                break;

            case Token.IF:
                result.append("if ");
                break;

            case Token.ELSE:
                result.append("else ");
                break;

            case Token.FOR:
                result.append("for ");
                break;

            case Token.IN:
                result.append(" in ");
                break;

            case Token.WITH:
                result.append("with ");
                break;

            case Token.WHILE:
                result.append("while ");
                break;

            case Token.DO:
                result.append("do ");
                break;

            case Token.TRY:
                result.append("try ");
                break;

            case Token.CATCH:
                result.append("catch ");
                break;

            case Token.FINALLY:
                result.append("finally ");
                break;

            case Token.THROW:
                result.append("throw ");
                break;

            case Token.SWITCH:
                result.append("switch ");
                break;

            case Token.BREAK:
                if (nextIs(source, length, i, Token.NAME))
                    result.append("break ");
                else
                    result.append("break");
                break;

            case Token.CONTINUE:
                if (nextIs(source, length, i, Token.NAME))
                    result.append("continue ");
                else
                    result.append("continue");
                break;

            case Token.CASE:
                result.append("case ");
                break;

            case Token.DEFAULT:
                result.append("default");
                break;

            case Token.RETURN:
                if (nextIs(source, length, i, Token.SEMI))
                    result.append("return");
                else
                    result.append("return ");
                break;

            case Token.VAR:
                result.append("var ");
                break;

            case Token.SEMI:
                if (nextIs(source, length, i, Token.EOL))
                    // statement termination
                    result.append(';');
                else
                    // separators in FOR
                    result.append("; ");
                break;

            case Token.ASSIGN:
                ++i;
                switch(source.charAt(i)) {
                case Token.NOP:
                    result.append(" = ");
                    break;

                case Token.ADD:
                    result.append(" += ");
                    break;

                case Token.SUB:
                    result.append(" -= ");
                    break;

                case Token.MUL:
                    result.append(" *= ");
                    break;

                case Token.DIV:
                    result.append(" /= ");
                    break;

                case Token.MOD:
                    result.append(" %= ");
                    break;

                case Token.BITOR:
                    result.append(" |= ");
                    break;

                case Token.BITXOR:
                    result.append(" ^= ");
                    break;

                case Token.BITAND:
                    result.append(" &= ");
                    break;

                case Token.LSH:
                    result.append(" <<= ");
                    break;

                case Token.RSH:
                    result.append(" >>= ");
                    break;

                case Token.URSH:
                    result.append(" >>>= ");
                    break;
                }
                break;

            case Token.HOOK:
                result.append(" ? ");
                break;

            case Token.OBJLIT:
                // pun OBJLIT to mean colon in objlit property initialization.
                // this needs to be distinct from COLON in the general case
                // to distinguish from the colon in a ternary... which needs
                // different spacing.
                result.append(':');
                break;

            case Token.COLON:
                if (nextIs(source, length, i, Token.EOL))
                    // it's the end of a label
                    result.append(':');
                else
                    // it's the middle part of a ternary
                    result.append(" : ");
                break;

            case Token.OR:
                result.append(" || ");
                break;

            case Token.AND:
                result.append(" && ");
                break;

            case Token.BITOR:
                result.append(" | ");
                break;

            case Token.BITXOR:
                result.append(" ^ ");
                break;

            case Token.BITAND:
                result.append(" & ");
                break;

            case Token.EQOP:
                ++i;
                switch(source.charAt(i)) {
                case Token.SHEQ:
                    result.append(" === ");
                    break;

                case Token.SHNE:
                    result.append(" !== ");
                    break;

                case Token.EQ:
                    result.append(" == ");
                    break;

                case Token.NE:
                    result.append(" != ");
                    break;
                }
                break;

            case Token.RELOP:
                ++i;
                switch(source.charAt(i)) {
                case Token.LE:
                    result.append(" <= ");
                    break;

                case Token.LT:
                    result.append(" < ");
                    break;

                case Token.GE:
                    result.append(" >= ");
                    break;

                case Token.GT:
                    result.append(" > ");
                    break;

                case Token.INSTANCEOF:
                    result.append(" instanceof ");
                    break;
                }
                break;

            case Token.SHOP:
                ++i;
                switch(source.charAt(i)) {
                case Token.LSH:
                    result.append(" << ");
                    break;

                case Token.RSH:
                    result.append(" >> ");
                    break;

                case Token.URSH:
                    result.append(" >>> ");
                    break;
                }
                break;

            case Token.UNARYOP:
                ++i;
                switch(source.charAt(i)) {
                case Token.TYPEOF:
                    result.append("typeof ");
                    break;

                case Token.VOID:
                    result.append("void ");
                    break;

                case Token.NOT:
                    result.append('!');
                    break;

                case Token.BITNOT:
                    result.append('~');
                    break;

                case Token.ADD:
                    result.append('+');
                    break;

                case Token.SUB:
                    result.append('-');
                    break;
                }
                break;

            case Token.INC:
                result.append("++");
                break;

            case Token.DEC:
                result.append("--");
                break;

            case Token.ADD:
                result.append(" + ");
                break;

            case Token.SUB:
                result.append(" - ");
                break;

            case Token.MUL:
                result.append(" * ");
                break;

            case Token.DIV:
                result.append(" / ");
                break;

            case Token.MOD:
                result.append(" % ");
                break;

            default:
                // If we don't know how to decompile it, raise an exception.
                throw new RuntimeException();
            }
            ++i;
        }

        // add that trailing newline if it's an outermost function.
        if (!nested && !justbody)
            result.append('\n');
    }

    private static boolean nextIs(String source, int length, int i, int token)
    {
        return (i + 1 < length) ? source.charAt(i + 1) == token : false;
    }

    private static int getSourceStringEnd(String source, int offset)
    {
        return printSourceString(source, offset, false, null);
    }

    private static int printSourceString(String source, int offset,
                                         boolean asQuotedString,
                                         StringBuffer sb)
    {
        int length = source.charAt(offset);
        ++offset;
        if ((0x8000 & length) != 0) {
            length = ((0x7FFF & length) << 16) | source.charAt(offset);
            ++offset;
        }
        if (sb != null) {
            String str = source.substring(offset, offset + length);
            if (!asQuotedString) {
                sb.append(str);
            } else {
                sb.append('"');
                sb.append(ScriptRuntime.escapeString(str));
                sb.append('"');
            }
        }
        return offset + length;
    }

    private static int printSourceNumber(String source, int offset,
                                         StringBuffer sb)
    {
        double number = 0.0;
        char type = source.charAt(offset);
        ++offset;
        if (type == 'S') {
            if (sb != null) {
                int ival = source.charAt(offset);
                number = ival;
            }
            ++offset;
        } else if (type == 'J' || type == 'D') {
            if (sb != null) {
                long lbits;
                lbits = (long)source.charAt(offset) << 48;
                lbits |= (long)source.charAt(offset + 1) << 32;
                lbits |= (long)source.charAt(offset + 2) << 16;
                lbits |= (long)source.charAt(offset + 3);
                if (type == 'J') {
                    number = lbits;
                } else {
                    number = Double.longBitsToDouble(lbits);
                }
            }
            offset += 4;
        } else {
            // Bad source
            throw new RuntimeException();
        }
        if (sb != null) {
            sb.append(ScriptRuntime.numberToString(number, 10));
        }
        return offset;
    }

    private char[] sourceBuffer = new char[128];

// Per script/function source buffer top: parent source does not include a
// nested functions source and uses function index as a reference instead.
    private int sourceTop;

// whether to do a debug print of the source information, when decompiling.
    private static final boolean printSource = false;

}
