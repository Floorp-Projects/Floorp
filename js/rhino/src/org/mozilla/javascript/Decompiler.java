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
 * newlines are important.)  The function decompile expands the
 * tokens back into their string representations, using simple
 * lookahead to correct spacing and indentation.
 *
 * Assignments are saved as two-token pairs (Token.ASSIGN, op). Number tokens
 * are stored inline, as a NUMBER token, a character representing the type, and
 * either 1 or 4 characters representing the bit-encoding of the number.  String
 * types NAME, STRING and OBJECT are currently stored as a token type,
 * followed by a character giving the length of the string (assumed to
 * be less than 2^16), followed by the characters of the string
 * inlined into the source string.  Changing this to some reference to
 * to the string in the compiled class' constant pool would probably
 * save a lot of space... but would require some method of deriving
 * the final constant pool entry from information available at parse
 * time.
 */
public class Decompiler
{
    /**
     * Flag to indicate that the decompilation should omit the
     * function header and trailing brace.
     */
    public static final int ONLY_BODY_FLAG = 1 << 0;

    /**
     * Flag to indicate that the decompilation generates toSource result.
     */
    public static final int TO_SOURCE_FLAG = 1 << 1;

    /**
     * Decompilation property to specify initial ident value.
     */
    public static final int INITIAL_INDENT_PROP = 1;

    /**
     * Decompilation property to specify default identation offset.
     */
    public static final int INDENT_GAP_PROP = 2;

    /**
     * Decompilation property to specify identation offset for case labels.
     */
    public static final int CASE_GAP_PROP = 3;

    // Marker to denote the last RC of function so it can be distinguished from
    // the last RC of object literals in case of function expressions
    private static final int FUNCTION_END = Token.LAST_TOKEN + 1;

    String getEncodedSource()
    {
        return sourceToString(0);
    }

    int getCurrentOffset()
    {
        return sourceTop;
    }

    int markFunctionStart(int functionType)
    {
        int savedOffset = getCurrentOffset();
        addToken(Token.FUNCTION);
        append((char)functionType);
        return savedOffset;
    }

    int markFunctionEnd(int functionStart)
    {
        int offset = getCurrentOffset();
        append((char)FUNCTION_END);
        return offset;
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
            // by NEG
               if (lbits < 0) Kit.codeBug();

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
        if (minimalCapacity <= sourceBuffer.length) Kit.codeBug();
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
        if (offset < 0 || sourceTop < offset) Kit.codeBug();
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
     * @param source encoded source tree presentation
     *
     * @param flags flags to select output format
     *
     * @param properties indentation properties
     *
     */
    public static String decompile(String source, int flags,
                                   UintMap properties)
    {
        int length = source.length();
        if (length == 0) { return ""; }

        int indent = properties.getInt(INITIAL_INDENT_PROP, 0);
        if (indent < 0) throw new IllegalArgumentException();
        int indentGap = properties.getInt(INDENT_GAP_PROP, 4);
        if (indentGap < 0) throw new IllegalArgumentException();
        int caseGap = properties.getInt(CASE_GAP_PROP, 2);
        if (caseGap < 0) throw new IllegalArgumentException();

        StringBuffer result = new StringBuffer();
        boolean justFunctionBody = (0 != (flags & Decompiler.ONLY_BODY_FLAG));
        boolean toSource = (0 != (flags & Decompiler.TO_SOURCE_FLAG));

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

        int braceNesting = 0;
        boolean afterFirstEOL = false;
        int i = 0;
        int topFunctionType;
        if (source.charAt(i) == Token.SCRIPT) {
            ++i;
            topFunctionType = -1;
        } else {
            topFunctionType = source.charAt(i + 1);
        }

        if (!toSource) {
            // add an initial newline to exactly match js.
            result.append('\n');
            for (int j = 0; j < indent; j++)
                result.append(' ');
        } else {
            if (topFunctionType == FunctionNode.FUNCTION_EXPRESSION) {
                result.append('(');
            }
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

            case Token.FUNCTION:
                ++i; // skip function type
                result.append("function ");
                break;

            case FUNCTION_END:
                // Do nothing
                break;

            case Token.COMMA:
                result.append(", ");
                break;

            case Token.LC:
                ++braceNesting;
                if (Token.EOL == getNext(source, length, i))
                    indent += indentGap;
                result.append('{');
                break;

            case Token.RC: {
                --braceNesting;
                /* don't print the closing RC if it closes the
                 * toplevel function and we're called from
                 * decompileFunctionBody.
                 */
                if (justFunctionBody && braceNesting == 0)
                    break;

                result.append('}');
                switch (getNext(source, length, i)) {
                    case Token.EOL:
                    case FUNCTION_END:
                        indent -= indentGap;
                        break;
                    case Token.WHILE:
                    case Token.ELSE:
                        indent -= indentGap;
                        result.append(' ');
                        break;
                }
                break;
            }
            case Token.LP:
                result.append('(');
                break;

            case Token.RP:
                result.append(')');
                if (Token.LC == getNext(source, length, i))
                    result.append(' ');
                break;

            case Token.LB:
                result.append('[');
                break;

            case Token.RB:
                result.append(']');
                break;

            case Token.EOL: {
                if (toSource) break;
                boolean newLine = true;
                if (!afterFirstEOL) {
                    afterFirstEOL = true;
                    if (justFunctionBody) {
                        /* throw away just added 'function name(...) {'
                         * and restore the original indent
                         */
                        result.setLength(0);
                        indent -= indentGap;
                        newLine = false;
                    }
                }
                if (newLine) {
                    result.append('\n');
                }

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
            }
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
                result.append("break");
                if (Token.NAME == getNext(source, length, i))
                    result.append(' ');
                break;

            case Token.CONTINUE:
                result.append("continue");
                if (Token.NAME == getNext(source, length, i))
                    result.append(' ');
                break;

            case Token.CASE:
                result.append("case ");
                break;

            case Token.DEFAULT:
                result.append("default");
                break;

            case Token.RETURN:
                result.append("return");
                if (Token.SEMI != getNext(source, length, i))
                    result.append(' ');
                break;

            case Token.VAR:
                result.append("var ");
                break;

            case Token.SEMI:
                result.append(';');
                if (Token.EOL != getNext(source, length, i)) {
                    // separators in FOR
                    result.append(' ');
                }
                break;

            case Token.ASSIGN:
                result.append(" = ");
                break;

            case Token.ASSIGN_ADD:
                result.append(" += ");
                break;

            case Token.ASSIGN_SUB:
                result.append(" -= ");
                break;

            case Token.ASSIGN_MUL:
                result.append(" *= ");
                break;

            case Token.ASSIGN_DIV:
                result.append(" /= ");
                break;

            case Token.ASSIGN_MOD:
                result.append(" %= ");
                break;

            case Token.ASSIGN_BITOR:
                result.append(" |= ");
                break;

            case Token.ASSIGN_BITXOR:
                result.append(" ^= ");
                break;

            case Token.ASSIGN_BITAND:
                result.append(" &= ");
                break;

            case Token.ASSIGN_LSH:
                result.append(" <<= ");
                break;

            case Token.ASSIGN_RSH:
                result.append(" >>= ");
                break;

            case Token.ASSIGN_URSH:
                result.append(" >>>= ");
                break;

            case Token.HOOK:
                result.append(" ? ");
                break;

            case Token.OBJECTLIT:
                // pun OBJECTLIT to mean colon in objlit property
                // initialization.
                // This needs to be distinct from COLON in the general case
                // to distinguish from the colon in a ternary... which needs
                // different spacing.
                result.append(':');
                break;

            case Token.COLON:
                if (Token.EOL == getNext(source, length, i))
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

            case Token.LSH:
                result.append(" << ");
                break;

            case Token.RSH:
                result.append(" >> ");
                break;

            case Token.URSH:
                result.append(" >>> ");
                break;

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

            case Token.POS:
                result.append('+');
                break;

            case Token.NEG:
                result.append('-');
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

            case Token.COLONCOLON:
                result.append("::");
                break;

            case Token.DOTDOT:
                result.append("..");
                break;

            case Token.XMLATTR:
                result.append('@');
                break;

            default:
                // If we don't know how to decompile it, raise an exception.
                throw new RuntimeException();
            }
            ++i;
        }

        if (!toSource) {
            // add that trailing newline if it's an outermost function.
            if (!justFunctionBody)
                result.append('\n');
        } else {
            if (topFunctionType == FunctionNode.FUNCTION_EXPRESSION) {
                result.append(')');
            }
        }

        return result.toString();
    }

    private static int getNext(String source, int length, int i)
    {
        return (i + 1 < length) ? source.charAt(i + 1) : Token.EOF;
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
