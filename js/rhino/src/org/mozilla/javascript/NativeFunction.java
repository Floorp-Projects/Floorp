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

/**
 * This class implements the Function native object.
 * See ECMA 15.3.
 * @author Norris Boyd
 */
public class NativeFunction extends BaseFunction {

    // how much to indent
    private final static int OFFSET = 4;

    // less how much for case labels
    private final static int SETBACK = 2;

    // whether to do a debug print of the source information, when
    // decompiling.
    private static final boolean printSource = false;

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
     * @param cx Current context
     *
     * @param indent How much to indent the decompiled result
     *
     * @param justbody Whether the decompilation should omit the
     * function header and trailing brace.
     */

    public String decompile(Context cx, int indent, boolean justbody) {
        Object[] srcData = new Object[1];
        StringBuffer result = new StringBuffer();
        decompile_r(this, indent, true, justbody, srcData, result);
        return result.toString();

    }

    private static void decompile_r(NativeFunction f, int indent,
                                    boolean toplevel, boolean justbody,
                                    Object[] srcData, StringBuffer result)
    {
        String source = f.source;

        if (source == null) {
            if (!justbody) {
                result.append("function ");
                result.append(f.getFunctionName());
                result.append("() {\n\t");
            }
            result.append("[native code]\n");
            if (!justbody) {
                result.append("}\n");
            }
            return;
        }

        int length = source.length();

        // Spew tokens in source, for debugging.
        // as TYPE number char
        if (printSource) {
            System.err.println("length:" + length);
            for (int i = 0; i < length; ++i) {
                // Note that tokenToName will fail unless Context.printTrees
                // is true.
                String tokenname = TokenStream.tokenToName(source.charAt(i));
                if (tokenname == null)
                    tokenname = "---";
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

        int i = 0;

        if (length != 0) {
            // If the first token is TokenStream.SCRIPT, then we're
            // decompiling the toplevel script, otherwise it a function
            // and should start with TokenStream.FUNCTION

            if (toplevel) {
                // add an initial newline to exactly match js.
                if (!justbody)
                    result.append('\n');
                for (int j = 0; j < indent; j++)
                    result.append(' ');
            }

            int token = source.charAt(i);
            ++i;
            if (token == TokenStream.FUNCTION) {
                if (!justbody) {
                    result.append("function ");

                    /* version != 1.2 Function constructor behavior - if
                     * there's no function name in the source info, and
                     * the names[0] entry is the empty string, then it must
                     * have been created by the Function constructor;
                     * print 'anonymous' as the function name if the
                     * version (under which the function was compiled) is
                     * less than 1.2... or if it's greater than 1.2, because
                     * we need to be closer to ECMA.  (ToSource, please?)
                     */
                    if (source.charAt(i) == TokenStream.LP
                        && f.version != Context.VERSION_1_2
                        && f.functionName != null
                        && f.functionName.equals("anonymous"))
                    {
                        result.append("anonymous");
                    }
                } else {
                    // Skip past the entire function header pass the next EOL.
                    skipLoop: for (;;) {
                        token = source.charAt(i);
                        ++i;
                        switch (token) {
                            case TokenStream.EOL:
                                break skipLoop;
                            case TokenStream.NAME:
                                // Skip function or argument name
                                i = Parser.getSourceString(source, i, null);
                                break;
                            case TokenStream.LP:
                            case TokenStream.COMMA:
                            case TokenStream.RP:
                                break;
                            default:
                                // Bad function header
                                throw new RuntimeException();
                        }
                    }
                }
            } else if (token != TokenStream.SCRIPT) {
                // Bad source header
                throw new RuntimeException();
            }
        }

        while (i < length) {
            switch(source.charAt(i)) {
            case TokenStream.NAME:
            case TokenStream.REGEXP:  // re-wrapped in '/'s in parser...
                /* NAMEs are encoded as NAME, (char) length, string...
                 * Note that lookahead for detecting labels depends on
                 * this encoding; change there if this changes.

                 * Also change function-header skipping code above,
                 * used when decompling under decompileFunctionBody.
                 */
                i = Parser.getSourceString(source, i + 1, srcData);
                result.append((String)srcData[0]);
                continue;

            case TokenStream.NUMBER: {
                i = Parser.getSourceNumber(source, i + 1, srcData);
                double number = ((Number)srcData[0]).doubleValue();
                result.append(ScriptRuntime.numberToString(number, 10));
                continue;
            }

            case TokenStream.STRING:
                i = Parser.getSourceString(source, i + 1, srcData);
                result.append('"');
                result.append(ScriptRuntime.escapeString((String)srcData[0]));
                result.append('"');
                continue;

            case TokenStream.PRIMARY:
                ++i;
                switch(source.charAt(i)) {
                case TokenStream.TRUE:
                    result.append("true");
                    break;

                case TokenStream.FALSE:
                    result.append("false");
                    break;

                case TokenStream.NULL:
                    result.append("null");
                    break;

                case TokenStream.THIS:
                    result.append("this");
                    break;

                case TokenStream.TYPEOF:
                    result.append("typeof");
                    break;

                case TokenStream.VOID:
                    result.append("void");
                    break;

                case TokenStream.UNDEFINED:
                    result.append("undefined");
                    break;
                }
                break;

            case TokenStream.FUNCTION: {
                /* decompile a FUNCTION token as an escape; call
                 * toString on the nth enclosed nested function,
                 * where n is given by the byte that follows.
                 */

                ++i;
                int functionNumber = source.charAt(i);
                if (f.nestedFunctions == null
                    || functionNumber > f.nestedFunctions.length)
                {
                    String message;
                    if (f.functionName != null && f.functionName.length() > 0) {
                        message = Context.getMessage2
                            ("msg.no.function.ref.found.in",
                             new Integer(functionNumber), f.functionName);
                    } else {
                        message = Context.getMessage1
                            ("msg.no.function.ref.found",
                             new Integer(functionNumber));
                    }
                    throw Context.reportRuntimeError(message);
                }
                decompile_r(f.nestedFunctions[functionNumber], indent,
                            false, false, srcData, result);
                break;
            }
            case TokenStream.COMMA:
                result.append(", ");
                break;

            case TokenStream.LC:
                if (nextIs(source, length, i, TokenStream.EOL))
                    indent += OFFSET;
                result.append('{');
                break;

            case TokenStream.RC:
                /* don't print the closing RC if it closes the
                 * toplevel function and we're called from
                 * decompileFunctionBody.
                 */
                if (justbody && toplevel && i + 1 == length)
                    break;

                if (nextIs(source, length, i, TokenStream.EOL))
                    indent -= OFFSET;
                if (nextIs(source, length, i, TokenStream.WHILE)
                    || nextIs(source, length, i, TokenStream.ELSE)) {
                    indent -= OFFSET;
                    result.append("} ");
                }
                else
                    result.append('}');
                break;

            case TokenStream.LP:
                result.append('(');
                break;

            case TokenStream.RP:
                if (nextIs(source, length, i, TokenStream.LC))
                    result.append(") ");
                else
                    result.append(')');
                break;

            case TokenStream.LB:
                result.append('[');
                break;

            case TokenStream.RB:
                result.append(']');
                break;

            case TokenStream.EOL:
                result.append('\n');

                /* add indent if any tokens remain,
                 * less setback if next token is
                 * a label, case or default.
                 */
                if (i + 1 < length) {
                    int less = 0;
                    int nextToken = source.charAt(i + 1);
                    if (nextToken == TokenStream.CASE
                        || nextToken == TokenStream.DEFAULT)
                        less = SETBACK;
                    else if (nextToken == TokenStream.RC)
                        less = OFFSET;

                    /* elaborate check against label... skip past a
                     * following inlined NAME and look for a COLON.
                     * Depends on how NAME is encoded.
                     */
                    else if (nextToken == TokenStream.NAME) {
                        int afterName = Parser.getSourceString(source, i + 2,
                                                               null);
                        if (source.charAt(afterName) == TokenStream.COLON)
                            less = OFFSET;
                    }

                    for (; less < indent; less++)
                        result.append(' ');
                }
                break;

            case TokenStream.DOT:
                result.append('.');
                break;

            case TokenStream.NEW:
                result.append("new ");
                break;

            case TokenStream.DELPROP:
                result.append("delete ");
                break;

            case TokenStream.IF:
                result.append("if ");
                break;

            case TokenStream.ELSE:
                result.append("else ");
                break;

            case TokenStream.FOR:
                result.append("for ");
                break;

            case TokenStream.IN:
                result.append(" in ");
                break;

            case TokenStream.WITH:
                result.append("with ");
                break;

            case TokenStream.WHILE:
                result.append("while ");
                break;

            case TokenStream.DO:
                result.append("do ");
                break;

            case TokenStream.TRY:
                result.append("try ");
                break;

            case TokenStream.CATCH:
                result.append("catch ");
                break;

            case TokenStream.FINALLY:
                result.append("finally ");
                break;

            case TokenStream.THROW:
                result.append("throw ");
                break;

            case TokenStream.SWITCH:
                result.append("switch ");
                break;

            case TokenStream.BREAK:
                if (nextIs(source, length, i, TokenStream.NAME))
                    result.append("break ");
                else
                    result.append("break");
                break;

            case TokenStream.CONTINUE:
                if (nextIs(source, length, i, TokenStream.NAME))
                    result.append("continue ");
                else
                    result.append("continue");
                break;

            case TokenStream.CASE:
                result.append("case ");
                break;

            case TokenStream.DEFAULT:
                result.append("default");
                break;

            case TokenStream.RETURN:
                if (nextIs(source, length, i, TokenStream.SEMI))
                    result.append("return");
                else
                    result.append("return ");
                break;

            case TokenStream.VAR:
                result.append("var ");
                break;

            case TokenStream.SEMI:
                if (nextIs(source, length, i, TokenStream.EOL))
                    // statement termination
                    result.append(';');
                else
                    // separators in FOR
                    result.append("; ");
                break;

            case TokenStream.ASSIGN:
                ++i;
                switch(source.charAt(i)) {
                case TokenStream.NOP:
                    result.append(" = ");
                    break;

                case TokenStream.ADD:
                    result.append(" += ");
                    break;

                case TokenStream.SUB:
                    result.append(" -= ");
                    break;

                case TokenStream.MUL:
                    result.append(" *= ");
                    break;

                case TokenStream.DIV:
                    result.append(" /= ");
                    break;

                case TokenStream.MOD:
                    result.append(" %= ");
                    break;

                case TokenStream.BITOR:
                    result.append(" |= ");
                    break;

                case TokenStream.BITXOR:
                    result.append(" ^= ");
                    break;

                case TokenStream.BITAND:
                    result.append(" &= ");
                    break;

                case TokenStream.LSH:
                    result.append(" <<= ");
                    break;

                case TokenStream.RSH:
                    result.append(" >>= ");
                    break;

                case TokenStream.URSH:
                    result.append(" >>>= ");
                    break;
                }
                break;

            case TokenStream.HOOK:
                result.append(" ? ");
                break;

            case TokenStream.OBJLIT:
                // pun OBJLIT to mean colon in objlit property initialization.
                // this needs to be distinct from COLON in the general case
                // to distinguish from the colon in a ternary... which needs
                // different spacing.
                result.append(':');
                break;

            case TokenStream.COLON:
                if (nextIs(source, length, i, TokenStream.EOL))
                    // it's the end of a label
                    result.append(':');
                else
                    // it's the middle part of a ternary
                    result.append(" : ");
                break;

            case TokenStream.OR:
                result.append(" || ");
                break;

            case TokenStream.AND:
                result.append(" && ");
                break;

            case TokenStream.BITOR:
                result.append(" | ");
                break;

            case TokenStream.BITXOR:
                result.append(" ^ ");
                break;

            case TokenStream.BITAND:
                result.append(" & ");
                break;

            case TokenStream.EQOP:
                ++i;
                switch(source.charAt(i)) {
                case TokenStream.SHEQ:
                    /*
                     * Emulate the C engine; if we're under version
                     * 1.2, then the == operator behaves like the ===
                     * operator (and the source is generated by
                     * decompiling a === opcode), so print the ===
                     * operator as ==.
                     */
                    result.append(f.version == Context.VERSION_1_2
                                  ? " == " : " === ");
                    break;

                case TokenStream.SHNE:
                    result.append(f.version == Context.VERSION_1_2
                                  ? " != " : " !== ");
                    break;

                case TokenStream.EQ:
                    result.append(" == ");
                    break;

                case TokenStream.NE:
                    result.append(" != ");
                    break;
                }
                break;

            case TokenStream.RELOP:
                ++i;
                switch(source.charAt(i)) {
                case TokenStream.LE:
                    result.append(" <= ");
                    break;

                case TokenStream.LT:
                    result.append(" < ");
                    break;

                case TokenStream.GE:
                    result.append(" >= ");
                    break;

                case TokenStream.GT:
                    result.append(" > ");
                    break;

                case TokenStream.INSTANCEOF:
                    result.append(" instanceof ");
                    break;
                }
                break;

            case TokenStream.SHOP:
                ++i;
                switch(source.charAt(i)) {
                case TokenStream.LSH:
                    result.append(" << ");
                    break;

                case TokenStream.RSH:
                    result.append(" >> ");
                    break;

                case TokenStream.URSH:
                    result.append(" >>> ");
                    break;
                }
                break;

            case TokenStream.UNARYOP:
                ++i;
                switch(source.charAt(i)) {
                case TokenStream.TYPEOF:
                    result.append("typeof ");
                    break;

                case TokenStream.VOID:
                    result.append("void ");
                    break;

                case TokenStream.NOT:
                    result.append('!');
                    break;

                case TokenStream.BITNOT:
                    result.append('~');
                    break;

                case TokenStream.ADD:
                    result.append('+');
                    break;

                case TokenStream.SUB:
                    result.append('-');
                    break;
                }
                break;

            case TokenStream.INC:
                result.append("++");
                break;

            case TokenStream.DEC:
                result.append("--");
                break;

            case TokenStream.ADD:
                result.append(" + ");
                break;

            case TokenStream.SUB:
                result.append(" - ");
                break;

            case TokenStream.MUL:
                result.append(" * ");
                break;

            case TokenStream.DIV:
                result.append(" / ");
                break;

            case TokenStream.MOD:
                result.append(" % ");
                break;

            default:
                // If we don't know how to decompile it, raise an exception.
                throw new RuntimeException();
            }
            ++i;
        }

        // add that trailing newline if it's an outermost function.
        if (toplevel && !justbody)
            result.append('\n');
    }

    private static boolean nextIs(String source, int length, int i, int token) {
        return (i + 1 < length) ? source.charAt(i + 1) == token : false;
    }

    public int getLength() {
        Context cx = Context.getContext();
        if (cx != null && cx.getLanguageVersion() != Context.VERSION_1_2)
            return argCount;
        NativeCall activation = getActivation(cx);
        if (activation == null)
            return argCount;
        return activation.getOriginalArguments().length;
    }

    public int getArity() {
        return argCount;
    }

    /**
     * For backwards compatibility keep an old method name used by
     * Batik and possibly others.
     */
    public String jsGet_name() {
        return getFunctionName();
    }

    /**
     * The "argsNames" array has the following information:
     * argNames[0] through argNames[argCount - 1]: the names of the parameters
     * argNames[argCount] through argNames[args.length-1]: the names of the
     * variables declared in var statements
     */
    protected String[] argNames;
    protected short argCount;
    protected short version;

    /**
     * An encoded representation of the function source, for
     * decompiling.  Needs to be visible (only) to generated
     * subclasses of NativeFunction.
     */
    protected String source;

    /**
     * An array of NativeFunction values for each nested function.
     * Used internally, and also for decompiling nested functions.
     */
    public NativeFunction[] nestedFunctions;

    // For all generated subclass objects debug_level is set to 0 or higher.
    // So, if debug_level remains -1 in some object, then that object is
    // known to have not been generated.
    public int debug_level = -1;
    public String debug_srcName;
}

