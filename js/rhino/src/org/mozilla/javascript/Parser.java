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

import java.io.IOException;

/**
 * This class implements the JavaScript parser.
 *
 * It is based on the C source files jsparse.c and jsparse.h
 * in the jsref package.
 *
 * @see TokenStream
 *
 * @author Mike McCabe
 * @author Brendan Eich
 */

class Parser {

    public Parser(IRFactory nf) {
        this.nf = nf;
    }

    private void mustMatchToken(TokenStream ts, int toMatch, String messageId)
        throws IOException, JavaScriptException
    {
        int tt;
        if ((tt = ts.getToken()) != toMatch) {
            reportError(ts, messageId);
            ts.ungetToken(tt); // In case the parser decides to continue
        }
    }

    private void reportError(TokenStream ts, String messageId)
        throws JavaScriptException
    {
        this.ok = false;
        ts.reportSyntaxError(messageId, null);

        /* Throw an exception to unwind the recursive descent parse.
         * We use JavaScriptException here even though it is really
         * a different use of the exception than it is usually used
         * for.
         */
        throw new JavaScriptException(messageId);
    }

    /*
     * Build a parse tree from the given TokenStream.
     *
     * @param ts the TokenStream to parse
     *
     * @return an Object representing the parsed
     * program.  If the parse fails, null will be returned.  (The
     * parse failure will result in a call to the current Context's
     * ErrorReporter.)
     */
    public Object parse(TokenStream ts)
        throws IOException
    {
        this.ok = true;
        sourceTop = 0;
        functionNumber = 0;

        int tt;          // last token from getToken();
        int baseLineno = ts.getLineno();  // line number where source starts

        /* so we have something to add nodes to until
         * we've collected all the source */
        Object tempBlock = nf.createLeaf(TokenStream.BLOCK);

        // Add script indicator
        sourceAdd((char)ts.SCRIPT);

        while (true) {
            ts.flags |= ts.TSF_REGEXP;
            tt = ts.getToken();
            ts.flags &= ~ts.TSF_REGEXP;

            if (tt <= ts.EOF) {
                break;
            }

            if (tt == ts.FUNCTION) {
                try {
                    nf.addChildToBack(tempBlock, function(ts, false));
                } catch (JavaScriptException e) {
                    this.ok = false;
                    break;
                }
            } else {
                ts.ungetToken(tt);
                nf.addChildToBack(tempBlock, statement(ts));
            }
        }

        if (!this.ok) {
            // XXX ts.clearPushback() call here?
            return null;
        }

        String source = sourceToString(0);
        sourceBuffer = null; // To help GC
        Object pn = nf.createScript(tempBlock, ts.getSourceName(),
                                    baseLineno, ts.getLineno(),
                                    source);
        return pn;
    }

    /*
     * The C version of this function takes an argument list,
     * which doesn't seem to be needed for tree generation...
     * it'd only be useful for checking argument hiding, which
     * I'm not doing anyway...
     */
    private Object parseFunctionBody(TokenStream ts)
        throws IOException
    {
        int oldflags = ts.flags;
        ts.flags &= ~(TokenStream.TSF_RETURN_EXPR
                      | TokenStream.TSF_RETURN_VOID);
        ts.flags |= TokenStream.TSF_FUNCTION;

        Object pn = nf.createBlock(ts.getLineno());
        try {
            int tt;
            while((tt = ts.peekToken()) > ts.EOF && tt != ts.RC) {
                if (tt == TokenStream.FUNCTION) {
                    ts.getToken();
                    nf.addChildToBack(pn, function(ts, false));
                } else {
                    nf.addChildToBack(pn, statement(ts));
                }
            }
        } catch (JavaScriptException e) {
            this.ok = false;
        } finally {
            // also in finally block:
            // flushNewLines, clearPushback.

            ts.flags = oldflags;
        }

        return pn;
    }

    private Object function(TokenStream ts, boolean isExpr)
        throws IOException, JavaScriptException
    {
        int baseLineno = ts.getLineno();  // line number where source starts

        String name;
        Object memberExprNode = null;
        if (ts.matchToken(ts.NAME)) {
            name = ts.getString();
            if (!ts.matchToken(ts.LP)) {
                if (Context.getContext().hasFeature
                    (Context.FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME))
                {
                    // Extension to ECMA: if 'function <name>' does not follow
                    // by '(', assume <name> starts memberExpr
                    sourceAddString(ts.NAME, name);
                    Object memberExprHead = nf.createName(name);
                    name = null;
                    memberExprNode = memberExprTail(ts, false, memberExprHead);
                }
                mustMatchToken(ts, ts.LP, "msg.no.paren.parms");
            }
        }
        else if (ts.matchToken(ts.LP)) {
            // Anonymous function
            name = null;
        }
        else {
            name = null;
            if (Context.getContext().hasFeature
                (Context.FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME))
            {
                // Note that memberExpr can not start with '(' like
                // in (1+2).toString, because 'function (' already
                // processed as anonymous function
                memberExprNode = memberExpr(ts, false);
            }
            mustMatchToken(ts, ts.LP, "msg.no.paren.parms");
        }

        if (memberExprNode != null) {
            // transform 'function' <memberExpr> to  <memberExpr> = function
            // even in the decompilated source
            sourceAdd((char)ts.ASSIGN);
            sourceAdd((char)ts.NOP);
        }

        // save a reference to the function in the enclosing source.
        sourceAdd((char) ts.FUNCTION);
        sourceAdd((char)functionNumber);
        ++functionNumber;

        // Save current source top to restore it on exit not to include
        // function to parent source
        int savedSourceTop = sourceTop;
        int savedFunctionNumber = functionNumber;
        Object args;
        Object body;
        String source;
        try {
            functionNumber = 0;

            // FUNCTION as the first token in a Source means it's a function
            // definition, and not a reference.
            sourceAdd((char) ts.FUNCTION);
            if (name != null) { sourceAddString(ts.NAME, name); }
            sourceAdd((char) ts.LP);
            args = nf.createLeaf(ts.LP);

            if (!ts.matchToken(ts.RP)) {
                boolean first = true;
                do {
                    if (!first)
                        sourceAdd((char)ts.COMMA);
                    first = false;
                    mustMatchToken(ts, ts.NAME, "msg.no.parm");
                    String s = ts.getString();
                    nf.addChildToBack(args, nf.createName(s));

                    sourceAddString(ts.NAME, s);
                } while (ts.matchToken(ts.COMMA));

                mustMatchToken(ts, ts.RP, "msg.no.paren.after.parms");
            }
            sourceAdd((char)ts.RP);

            mustMatchToken(ts, ts.LC, "msg.no.brace.body");
            sourceAdd((char)ts.LC);
            sourceAdd((char)ts.EOL);
            body = parseFunctionBody(ts);
            mustMatchToken(ts, ts.RC, "msg.no.brace.after.body");
            sourceAdd((char)ts.RC);
            // skip the last EOL so nested functions work...

            // name might be null;
            source = sourceToString(savedSourceTop);
        }
        finally {
            sourceTop = savedSourceTop;
            functionNumber = savedFunctionNumber;
        }

        Object pn = nf.createFunction(name, args, body,
                                      ts.getSourceName(),
                                      baseLineno, ts.getLineno(),
                                      source,
                                      isExpr || memberExprNode != null);
        if (memberExprNode != null) {
            pn = nf.createBinary(ts.ASSIGN, ts.NOP, memberExprNode, pn);
        }

        // Add EOL but only if function is not part of expression, in which
        // case it gets SEMI + EOL from Statement.
        if (!isExpr) {
            if (memberExprNode != null) {
                // Add ';' to make 'function x.f(){}' and 'x.f = function(){}'
                // to print the same strings when decompiling
                sourceAdd((char)ts.SEMI);
            }
            sourceAdd((char)ts.EOL);
            wellTerminated(ts, ts.FUNCTION);
        }

        return pn;
    }

    private Object statements(TokenStream ts)
        throws IOException
    {
        Object pn = nf.createBlock(ts.getLineno());

        int tt;
        while((tt = ts.peekToken()) > ts.EOF && tt != ts.RC) {
            nf.addChildToBack(pn, statement(ts));
        }

        return pn;
    }

    private Object condition(TokenStream ts)
        throws IOException, JavaScriptException
    {
        Object pn;
        mustMatchToken(ts, ts.LP, "msg.no.paren.cond");
        sourceAdd((char)ts.LP);
        pn = expr(ts, false);
        mustMatchToken(ts, ts.RP, "msg.no.paren.after.cond");
        sourceAdd((char)ts.RP);

        // there's a check here in jsparse.c that corrects = to ==

        return pn;
    }

    private boolean wellTerminated(TokenStream ts, int lastExprType)
        throws IOException, JavaScriptException
    {
        int tt = ts.peekTokenSameLine();
        if (tt == ts.ERROR) {
            return false;
        }

        if (tt != ts.EOF && tt != ts.EOL
            && tt != ts.SEMI && tt != ts.RC)
            {
                int version = Context.getContext().getLanguageVersion();
                if ((tt == ts.FUNCTION || lastExprType == ts.FUNCTION) &&
                    (version < Context.VERSION_1_2)) {
                    /*
                     * Checking against version < 1.2 and version >= 1.0
                     * in the above line breaks old javascript, so we keep it
                     * this way for now... XXX warning needed?
                     */
                    return true;
                } else {
                    reportError(ts, "msg.no.semi.stmt");
                }
            }
        return true;
    }

    // match a NAME; return null if no match.
    private String matchLabel(TokenStream ts)
        throws IOException, JavaScriptException
    {
        int lineno = ts.getLineno();

        String label = null;
        int tt;
        tt = ts.peekTokenSameLine();
        if (tt == ts.NAME) {
            ts.getToken();
            label = ts.getString();
        }

        if (lineno == ts.getLineno())
            wellTerminated(ts, ts.ERROR);

        return label;
    }

    private Object statement(TokenStream ts)
        throws IOException
    {
        try {
            return statementHelper(ts);
        } catch (JavaScriptException e) {
            // skip to end of statement
            int lineno = ts.getLineno();
            int t;
            do {
                t = ts.getToken();
            } while (t != TokenStream.SEMI && t != TokenStream.EOL &&
                     t != TokenStream.EOF && t != TokenStream.ERROR);
            return nf.createExprStatement(nf.createName("error"), lineno);
        }
    }

    /**
     * Whether the "catch (e: e instanceof Exception) { ... }" syntax
     * is implemented.
     */

    private Object statementHelper(TokenStream ts)
        throws IOException, JavaScriptException
    {
        Object pn = null;

        // If skipsemi == true, don't add SEMI + EOL to source at the
        // end of this statment.  For compound statements, IF/FOR etc.
        boolean skipsemi = false;

        int tt;

        int lastExprType = 0;  // For wellTerminated.  0 to avoid warning.

        tt = ts.getToken();

        switch(tt) {
        case TokenStream.IF: {
            skipsemi = true;

            sourceAdd((char)ts.IF);
            int lineno = ts.getLineno();
            Object cond = condition(ts);
            sourceAdd((char)ts.LC);
            sourceAdd((char)ts.EOL);
            Object ifTrue = statement(ts);
            Object ifFalse = null;
            if (ts.matchToken(ts.ELSE)) {
                sourceAdd((char)ts.RC);
                sourceAdd((char)ts.ELSE);
                sourceAdd((char)ts.LC);
                sourceAdd((char)ts.EOL);
                ifFalse = statement(ts);
            }
            sourceAdd((char)ts.RC);
            sourceAdd((char)ts.EOL);
            pn = nf.createIf(cond, ifTrue, ifFalse, lineno);
            break;
        }

        case TokenStream.SWITCH: {
            skipsemi = true;

            sourceAdd((char)ts.SWITCH);
            pn = nf.createSwitch(ts.getLineno());

            Object cur_case = null;  // to kill warning
            Object case_statements;

            mustMatchToken(ts, ts.LP, "msg.no.paren.switch");
            sourceAdd((char)ts.LP);
            nf.addChildToBack(pn, expr(ts, false));
            mustMatchToken(ts, ts.RP, "msg.no.paren.after.switch");
            sourceAdd((char)ts.RP);
            mustMatchToken(ts, ts.LC, "msg.no.brace.switch");
            sourceAdd((char)ts.LC);
            sourceAdd((char)ts.EOL);

            while ((tt = ts.getToken()) != ts.RC && tt != ts.EOF) {
                switch(tt) {
                case TokenStream.CASE:
                    sourceAdd((char)ts.CASE);
                    cur_case = nf.createUnary(ts.CASE, expr(ts, false));
                    sourceAdd((char)ts.COLON);
                    sourceAdd((char)ts.EOL);
                    break;

                case TokenStream.DEFAULT:
                    cur_case = nf.createLeaf(ts.DEFAULT);
                    sourceAdd((char)ts.DEFAULT);
                    sourceAdd((char)ts.COLON);
                    sourceAdd((char)ts.EOL);
                    // XXX check that there isn't more than one default
                    break;

                default:
                    reportError(ts, "msg.bad.switch");
                    break;
                }
                mustMatchToken(ts, ts.COLON, "msg.no.colon.case");

                case_statements = nf.createLeaf(TokenStream.BLOCK);

                while ((tt = ts.peekToken()) != ts.RC && tt != ts.CASE &&
                        tt != ts.DEFAULT && tt != ts.EOF)
                {
                    nf.addChildToBack(case_statements, statement(ts));
                }
                // assert cur_case
                nf.addChildToBack(cur_case, case_statements);

                nf.addChildToBack(pn, cur_case);
            }
            sourceAdd((char)ts.RC);
            sourceAdd((char)ts.EOL);
            break;
        }

        case TokenStream.WHILE: {
            skipsemi = true;

            sourceAdd((char)ts.WHILE);
            int lineno = ts.getLineno();
            Object cond = condition(ts);
            sourceAdd((char)ts.LC);
            sourceAdd((char)ts.EOL);
            Object body = statement(ts);
            sourceAdd((char)ts.RC);
            sourceAdd((char)ts.EOL);

            pn = nf.createWhile(cond, body, lineno);
            break;

        }

        case TokenStream.DO: {
            sourceAdd((char)ts.DO);
            sourceAdd((char)ts.LC);
            sourceAdd((char)ts.EOL);

            int lineno = ts.getLineno();

            Object body = statement(ts);

            sourceAdd((char)ts.RC);
            mustMatchToken(ts, ts.WHILE, "msg.no.while.do");
            sourceAdd((char)ts.WHILE);
            Object cond = condition(ts);

            pn = nf.createDoWhile(body, cond, lineno);
            break;
        }

        case TokenStream.FOR: {
            skipsemi = true;

            sourceAdd((char)ts.FOR);
            int lineno = ts.getLineno();

            Object init;  // Node init is also foo in 'foo in Object'
            Object cond;  // Node cond is also object in 'foo in Object'
            Object incr = null; // to kill warning
            Object body;

            mustMatchToken(ts, ts.LP, "msg.no.paren.for");
            sourceAdd((char)ts.LP);
            tt = ts.peekToken();
            if (tt == ts.SEMI) {
                init = nf.createLeaf(ts.VOID);
            } else {
                if (tt == ts.VAR) {
                    // set init to a var list or initial
                    ts.getToken();    // throw away the 'var' token
                    init = variables(ts, true);
                }
                else {
                    init = expr(ts, true);
                }
            }

            tt = ts.peekToken();
            if (tt == ts.RELOP && ts.getOp() == ts.IN) {
                ts.matchToken(ts.RELOP);
                sourceAdd((char)ts.IN);
                // 'cond' is the object over which we're iterating
                cond = expr(ts, false);
            } else {  // ordinary for loop
                mustMatchToken(ts, ts.SEMI, "msg.no.semi.for");
                sourceAdd((char)ts.SEMI);
                if (ts.peekToken() == ts.SEMI) {
                    // no loop condition
                    cond = nf.createLeaf(ts.VOID);
                } else {
                    cond = expr(ts, false);
                }

                mustMatchToken(ts, ts.SEMI, "msg.no.semi.for.cond");
                sourceAdd((char)ts.SEMI);
                if (ts.peekToken() == ts.RP) {
                    incr = nf.createLeaf(ts.VOID);
                } else {
                    incr = expr(ts, false);
                }
            }

            mustMatchToken(ts, ts.RP, "msg.no.paren.for.ctrl");
            sourceAdd((char)ts.RP);
            sourceAdd((char)ts.LC);
            sourceAdd((char)ts.EOL);
            body = statement(ts);
            sourceAdd((char)ts.RC);
            sourceAdd((char)ts.EOL);

            if (incr == null) {
                // cond could be null if 'in obj' got eaten by the init node.
                pn = nf.createForIn(init, cond, body, lineno);
            } else {
                pn = nf.createFor(init, cond, incr, body, lineno);
            }
            break;
        }

        case TokenStream.TRY: {
            int lineno = ts.getLineno();

            Object tryblock;
            Object catchblocks = null;
            Object finallyblock = null;

            skipsemi = true;
            sourceAdd((char)ts.TRY);
            sourceAdd((char)ts.LC);
            sourceAdd((char)ts.EOL);
            tryblock = statement(ts);
            sourceAdd((char)ts.RC);
            sourceAdd((char)ts.EOL);

            catchblocks = nf.createLeaf(TokenStream.BLOCK);

            boolean sawDefaultCatch = false;
            int peek = ts.peekToken();
            if (peek == ts.CATCH) {
                while (ts.matchToken(ts.CATCH)) {
                    if (sawDefaultCatch) {
                        reportError(ts, "msg.catch.unreachable");
                    }
                    sourceAdd((char)ts.CATCH);
                    mustMatchToken(ts, ts.LP, "msg.no.paren.catch");
                    sourceAdd((char)ts.LP);

                    mustMatchToken(ts, ts.NAME, "msg.bad.catchcond");
                    String varName = ts.getString();
                    sourceAddString(ts.NAME, varName);

                    Object catchCond = null;
                    if (ts.matchToken(ts.IF)) {
                        sourceAdd((char)ts.IF);
                        catchCond = expr(ts, false);
                    } else {
                        sawDefaultCatch = true;
                    }

                    mustMatchToken(ts, ts.RP, "msg.bad.catchcond");
                    sourceAdd((char)ts.RP);
                    mustMatchToken(ts, ts.LC, "msg.no.brace.catchblock");
                    sourceAdd((char)ts.LC);
                    sourceAdd((char)ts.EOL);

                    nf.addChildToBack(catchblocks,
                        nf.createCatch(varName, catchCond,
                                       statements(ts),
                                       ts.getLineno()));

                    mustMatchToken(ts, ts.RC, "msg.no.brace.after.body");
                    sourceAdd((char)ts.RC);
                    sourceAdd((char)ts.EOL);
                }
            } else if (peek != ts.FINALLY) {
                mustMatchToken(ts, ts.FINALLY, "msg.try.no.catchfinally");
            }

            if (ts.matchToken(ts.FINALLY)) {
                sourceAdd((char)ts.FINALLY);

                sourceAdd((char)ts.LC);
                sourceAdd((char)ts.EOL);
                finallyblock = statement(ts);
                sourceAdd((char)ts.RC);
                sourceAdd((char)ts.EOL);
            }

            pn = nf.createTryCatchFinally(tryblock, catchblocks,
                                          finallyblock, lineno);

            break;
        }
        case TokenStream.THROW: {
            int lineno = ts.getLineno();
            sourceAdd((char)ts.THROW);
            pn = nf.createThrow(expr(ts, false), lineno);
            if (lineno == ts.getLineno())
                wellTerminated(ts, ts.ERROR);
            break;
        }
        case TokenStream.BREAK: {
            int lineno = ts.getLineno();

            sourceAdd((char)ts.BREAK);

            // matchLabel only matches if there is one
            String label = matchLabel(ts);
            if (label != null) {
                sourceAddString(ts.NAME, label);
            }
            pn = nf.createBreak(label, lineno);
            break;
        }
        case TokenStream.CONTINUE: {
            int lineno = ts.getLineno();

            sourceAdd((char)ts.CONTINUE);

            // matchLabel only matches if there is one
            String label = matchLabel(ts);
            if (label != null) {
                sourceAddString(ts.NAME, label);
            }
            pn = nf.createContinue(label, lineno);
            break;
        }
        case TokenStream.WITH: {
            skipsemi = true;

            sourceAdd((char)ts.WITH);
            int lineno = ts.getLineno();
            mustMatchToken(ts, ts.LP, "msg.no.paren.with");
            sourceAdd((char)ts.LP);
            Object obj = expr(ts, false);
            mustMatchToken(ts, ts.RP, "msg.no.paren.after.with");
            sourceAdd((char)ts.RP);
            sourceAdd((char)ts.LC);
            sourceAdd((char)ts.EOL);

            Object body = statement(ts);

            sourceAdd((char)ts.RC);
            sourceAdd((char)ts.EOL);

            pn = nf.createWith(obj, body, lineno);
            break;
        }
        case TokenStream.VAR: {
            int lineno = ts.getLineno();
            pn = variables(ts, false);
            if (ts.getLineno() == lineno)
                wellTerminated(ts, ts.ERROR);
            break;
        }
        case TokenStream.RETURN: {
            Object retExpr = null;

            sourceAdd((char)ts.RETURN);

            // bail if we're not in a (toplevel) function
            if ((ts.flags & ts.TSF_FUNCTION) == 0)
                reportError(ts, "msg.bad.return");

            /* This is ugly, but we don't want to require a semicolon. */
            ts.flags |= ts.TSF_REGEXP;
            tt = ts.peekTokenSameLine();
            ts.flags &= ~ts.TSF_REGEXP;

            int lineno = ts.getLineno();
            if (tt != ts.EOF && tt != ts.EOL && tt != ts.SEMI && tt != ts.RC) {
                retExpr = expr(ts, false);
                if (ts.getLineno() == lineno)
                    wellTerminated(ts, ts.ERROR);
                ts.flags |= ts.TSF_RETURN_EXPR;
            } else {
                ts.flags |= ts.TSF_RETURN_VOID;
            }

            // XXX ASSERT pn
            pn = nf.createReturn(retExpr, lineno);
            break;
        }
        case TokenStream.LC:
            skipsemi = true;

            pn = statements(ts);
            mustMatchToken(ts, ts.RC, "msg.no.brace.block");
            break;

        case TokenStream.ERROR:
            // Fall thru, to have a node for error recovery to work on
        case TokenStream.EOL:
        case TokenStream.SEMI:
            pn = nf.createLeaf(ts.VOID);
            skipsemi = true;
            break;

        default: {
                lastExprType = tt;
                int tokenno = ts.getTokenno();
                ts.ungetToken(tt);
                int lineno = ts.getLineno();

                pn = expr(ts, false);

                if (ts.peekToken() == ts.COLON) {
                    /* check that the last thing the tokenizer returned was a
                     * NAME and that only one token was consumed.
                     */
                    if (lastExprType != ts.NAME || (ts.getTokenno() != tokenno))
                        reportError(ts, "msg.bad.label");

                    ts.getToken();  // eat the COLON

                    /* in the C source, the label is associated with the
                     * statement that follows:
                     *                nf.addChildToBack(pn, statement(ts));
                     */
                    String name = ts.getString();
                    pn = nf.createLabel(name, lineno);

                    // depend on decompiling lookahead to guess that that
                    // last name was a label.
                    sourceAdd((char)ts.COLON);
                    sourceAdd((char)ts.EOL);
                    return pn;
                }

                if (lastExprType == ts.FUNCTION) {
                    if (nf.getLeafType(pn) != ts.FUNCTION) {
                        reportError(ts, "msg.syntax");
                    }
                    nf.setFunctionExpressionStatement(pn);
                }

                pn = nf.createExprStatement(pn, lineno);

                /*
                 * Check explicitly against (multi-line) function
                 * statement.

                 * lastExprEndLine is a hack to fix an
                 * automatic semicolon insertion problem with function
                 * expressions; the ts.getLineno() == lineno check was
                 * firing after a function definition even though the
                 * next statement was on a new line, because
                 * speculative getToken calls advanced the line number
                 * even when they didn't succeed.
                 */
                if (ts.getLineno() == lineno ||
                    (lastExprType == ts.FUNCTION &&
                     ts.getLineno() == lastExprEndLine))
                {
                    wellTerminated(ts, lastExprType);
                }
                break;
            }
        }
        ts.matchToken(ts.SEMI);
        if (!skipsemi) {
            sourceAdd((char)ts.SEMI);
            sourceAdd((char)ts.EOL);
        }

        return pn;
    }

    private Object variables(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = nf.createVariables(ts.getLineno());
        boolean first = true;

        sourceAdd((char)ts.VAR);

        for (;;) {
            Object name;
            Object init;
            mustMatchToken(ts, ts.NAME, "msg.bad.var");
            String s = ts.getString();

            if (!first)
                sourceAdd((char)ts.COMMA);
            first = false;

            sourceAddString(ts.NAME, s);
            name = nf.createName(s);

            // omitted check for argument hiding

            if (ts.matchToken(ts.ASSIGN)) {
                if (ts.getOp() != ts.NOP)
                    reportError(ts, "msg.bad.var.init");

                sourceAdd((char)ts.ASSIGN);
                sourceAdd((char)ts.NOP);

                init = assignExpr(ts, inForInit);
                nf.addChildToBack(name, init);
            }
            nf.addChildToBack(pn, name);
            if (!ts.matchToken(ts.COMMA))
                break;
        }
        return pn;
    }

    private Object expr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = assignExpr(ts, inForInit);
        while (ts.matchToken(ts.COMMA)) {
            sourceAdd((char)ts.COMMA);
            pn = nf.createBinary(ts.COMMA, pn, assignExpr(ts, inForInit));
        }
        return pn;
    }

    private Object assignExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = condExpr(ts, inForInit);

        if (ts.matchToken(ts.ASSIGN)) {
            // omitted: "invalid assignment left-hand side" check.
            sourceAdd((char)ts.ASSIGN);
            sourceAdd((char)ts.getOp());
            pn = nf.createBinary(ts.ASSIGN, ts.getOp(), pn,
                                 assignExpr(ts, inForInit));
        }

        return pn;
    }

    private Object condExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object ifTrue;
        Object ifFalse;

        Object pn = orExpr(ts, inForInit);

        if (ts.matchToken(ts.HOOK)) {
            sourceAdd((char)ts.HOOK);
            ifTrue = assignExpr(ts, false);
            mustMatchToken(ts, ts.COLON, "msg.no.colon.cond");
            sourceAdd((char)ts.COLON);
            ifFalse = assignExpr(ts, inForInit);
            return nf.createTernary(pn, ifTrue, ifFalse);
        }

        return pn;
    }

    private Object orExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = andExpr(ts, inForInit);
        if (ts.matchToken(ts.OR)) {
            sourceAdd((char)ts.OR);
            pn = nf.createBinary(ts.OR, pn, orExpr(ts, inForInit));
        }

        return pn;
    }

    private Object andExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = bitOrExpr(ts, inForInit);
        if (ts.matchToken(ts.AND)) {
            sourceAdd((char)ts.AND);
            pn = nf.createBinary(ts.AND, pn, andExpr(ts, inForInit));
        }

        return pn;
    }

    private Object bitOrExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = bitXorExpr(ts, inForInit);
        while (ts.matchToken(ts.BITOR)) {
            sourceAdd((char)ts.BITOR);
            pn = nf.createBinary(ts.BITOR, pn, bitXorExpr(ts, inForInit));
        }
        return pn;
    }

    private Object bitXorExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = bitAndExpr(ts, inForInit);
        while (ts.matchToken(ts.BITXOR)) {
            sourceAdd((char)ts.BITXOR);
            pn = nf.createBinary(ts.BITXOR, pn, bitAndExpr(ts, inForInit));
        }
        return pn;
    }

    private Object bitAndExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = eqExpr(ts, inForInit);
        while (ts.matchToken(ts.BITAND)) {
            sourceAdd((char)ts.BITAND);
            pn = nf.createBinary(ts.BITAND, pn, eqExpr(ts, inForInit));
        }
        return pn;
    }

    private Object eqExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = relExpr(ts, inForInit);
        while (ts.matchToken(ts.EQOP)) {
            sourceAdd((char)ts.EQOP);
            sourceAdd((char)ts.getOp());
            pn = nf.createBinary(ts.EQOP, ts.getOp(), pn,
                                 relExpr(ts, inForInit));
        }
        return pn;
    }

    private Object relExpr(TokenStream ts, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = shiftExpr(ts);
        while (ts.matchToken(ts.RELOP)) {
            int op = ts.getOp();
            if (inForInit && op == ts.IN) {
                ts.ungetToken(ts.RELOP);
                break;
            }
            sourceAdd((char)ts.RELOP);
            sourceAdd((char)op);
            pn = nf.createBinary(ts.RELOP, op, pn, shiftExpr(ts));
        }
        return pn;
    }

    private Object shiftExpr(TokenStream ts)
        throws IOException, JavaScriptException
    {
        Object pn = addExpr(ts);
        while (ts.matchToken(ts.SHOP)) {
            sourceAdd((char)ts.SHOP);
            sourceAdd((char)ts.getOp());
            pn = nf.createBinary(ts.getOp(), pn, addExpr(ts));
        }
        return pn;
    }

    private Object addExpr(TokenStream ts)
        throws IOException, JavaScriptException
    {
        int tt;
        Object pn = mulExpr(ts);

        while ((tt = ts.getToken()) == ts.ADD || tt == ts.SUB) {
            sourceAdd((char)tt);
            // flushNewLines
            pn = nf.createBinary(tt, pn, mulExpr(ts));
        }
        ts.ungetToken(tt);

        return pn;
    }

    private Object mulExpr(TokenStream ts)
        throws IOException, JavaScriptException
    {
        int tt;

        Object pn = unaryExpr(ts);

        while ((tt = ts.peekToken()) == ts.MUL ||
               tt == ts.DIV ||
               tt == ts.MOD) {
            tt = ts.getToken();
            sourceAdd((char)tt);
            pn = nf.createBinary(tt, pn, unaryExpr(ts));
        }


        return pn;
    }

    private Object unaryExpr(TokenStream ts)
        throws IOException, JavaScriptException
    {
        int tt;

        ts.flags |= ts.TSF_REGEXP;
        tt = ts.getToken();
        ts.flags &= ~ts.TSF_REGEXP;

        switch(tt) {
        case TokenStream.UNARYOP:
            sourceAdd((char)ts.UNARYOP);
            sourceAdd((char)ts.getOp());
            return nf.createUnary(ts.UNARYOP, ts.getOp(),
                                  unaryExpr(ts));

        case TokenStream.ADD:
        case TokenStream.SUB:
            sourceAdd((char)ts.UNARYOP);
            sourceAdd((char)tt);
            return nf.createUnary(ts.UNARYOP, tt, unaryExpr(ts));

        case TokenStream.INC:
        case TokenStream.DEC:
            sourceAdd((char)tt);
            return nf.createUnary(tt, ts.PRE, memberExpr(ts, true));

        case TokenStream.DELPROP:
            sourceAdd((char)ts.DELPROP);
            return nf.createUnary(ts.DELPROP, unaryExpr(ts));

        case TokenStream.ERROR:
            break;

        default:
            ts.ungetToken(tt);

            int lineno = ts.getLineno();

            Object pn = memberExpr(ts, true);

            /* don't look across a newline boundary for a postfix incop.

             * the rhino scanner seems to work differently than the js
             * scanner here; in js, it works to have the line number check
             * precede the peekToken calls.  It'd be better if they had
             * similar behavior...
             */
            int peeked;
            if (((peeked = ts.peekToken()) == ts.INC ||
                 peeked == ts.DEC) &&
                ts.getLineno() == lineno)
            {
                int pf = ts.getToken();
                sourceAdd((char)pf);
                return nf.createUnary(pf, ts.POST, pn);
            }
            return pn;
        }
        return nf.createName("err"); // Only reached on error.  Try to continue.

    }

    private Object argumentList(TokenStream ts, Object listNode)
        throws IOException, JavaScriptException
    {
        boolean matched;
        ts.flags |= ts.TSF_REGEXP;
        matched = ts.matchToken(ts.RP);
        ts.flags &= ~ts.TSF_REGEXP;
        if (!matched) {
            boolean first = true;
            do {
                if (!first)
                    sourceAdd((char)ts.COMMA);
                first = false;
                nf.addChildToBack(listNode, assignExpr(ts, false));
            } while (ts.matchToken(ts.COMMA));

            mustMatchToken(ts, ts.RP, "msg.no.paren.arg");
        }
        sourceAdd((char)ts.RP);
        return listNode;
    }

    private Object memberExpr(TokenStream ts, boolean allowCallSyntax)
        throws IOException, JavaScriptException
    {
        int tt;

        Object pn;

        /* Check for new expressions. */
        ts.flags |= ts.TSF_REGEXP;
        tt = ts.peekToken();
        ts.flags &= ~ts.TSF_REGEXP;
        if (tt == ts.NEW) {
            /* Eat the NEW token. */
            ts.getToken();
            sourceAdd((char)ts.NEW);

            /* Make a NEW node to append to. */
            pn = nf.createLeaf(ts.NEW);
            nf.addChildToBack(pn, memberExpr(ts, false));

            if (ts.matchToken(ts.LP)) {
                sourceAdd((char)ts.LP);
                /* Add the arguments to pn, if any are supplied. */
                pn = argumentList(ts, pn);
            }

            /* XXX there's a check in the C source against
             * "too many constructor arguments" - how many
             * do we claim to support?
             */

            /* Experimental syntax:  allow an object literal to follow a new expression,
             * which will mean a kind of anonymous class built with the JavaAdapter.
             * the object literal will be passed as an additional argument to the constructor.
             */
            tt = ts.peekToken();
            if (tt == ts.LC) {
                nf.addChildToBack(pn, primaryExpr(ts));
            }
        } else {
            pn = primaryExpr(ts);
        }

        return memberExprTail(ts, allowCallSyntax, pn);
    }

    private Object memberExprTail(TokenStream ts, boolean allowCallSyntax,
                                  Object pn)
        throws IOException, JavaScriptException
    {
        lastExprEndLine = ts.getLineno();
        int tt;
        while ((tt = ts.getToken()) > ts.EOF) {
            if (tt == ts.DOT) {
                sourceAdd((char)ts.DOT);
                mustMatchToken(ts, ts.NAME, "msg.no.name.after.dot");
                String s = ts.getString();
                sourceAddString(ts.NAME, s);
                pn = nf.createBinary(ts.DOT, pn,
                                     nf.createName(ts.getString()));
                /* pn = nf.createBinary(ts.DOT, pn, memberExpr(ts))
                 * is the version in Brendan's IR C version.  Not in ECMA...
                 * does it reflect the 'new' operator syntax he mentioned?
                 */
                lastExprEndLine = ts.getLineno();
            } else if (tt == ts.LB) {
                sourceAdd((char)ts.LB);
                pn = nf.createBinary(ts.LB, pn, expr(ts, false));

                mustMatchToken(ts, ts.RB, "msg.no.bracket.index");
                sourceAdd((char)ts.RB);
                lastExprEndLine = ts.getLineno();
            } else if (allowCallSyntax && tt == ts.LP) {
                /* make a call node */

                pn = nf.createUnary(ts.CALL, pn);
                sourceAdd((char)ts.LP);

                /* Add the arguments to pn, if any are supplied. */
                pn = argumentList(ts, pn);
                lastExprEndLine = ts.getLineno();
            } else {
                ts.ungetToken(tt);

                break;
            }
        }
        return pn;
    }

    private Object primaryExpr(TokenStream ts)
        throws IOException, JavaScriptException
    {
        int tt;

        Object pn;

        ts.flags |= ts.TSF_REGEXP;
        tt = ts.getToken();
        ts.flags &= ~ts.TSF_REGEXP;

        switch(tt) {

        case TokenStream.FUNCTION:
            return function(ts, true);

        case TokenStream.LB:
            {
                sourceAdd((char)ts.LB);
                pn = nf.createLeaf(ts.ARRAYLIT);

                ts.flags |= ts.TSF_REGEXP;
                boolean matched = ts.matchToken(ts.RB);
                ts.flags &= ~ts.TSF_REGEXP;

                if (!matched) {
                    boolean first = true;
                    do {
                        ts.flags |= ts.TSF_REGEXP;
                        tt = ts.peekToken();
                        ts.flags &= ~ts.TSF_REGEXP;

                        if (!first)
                            sourceAdd((char)ts.COMMA);
                        else
                            first = false;

                        if (tt == ts.RB) {  // to fix [,,,].length behavior...
                            break;
                        }

                        if (tt == ts.COMMA) {
                            nf.addChildToBack(pn, nf.createLeaf(ts.PRIMARY,
                                                                ts.UNDEFINED));
                        } else {
                            nf.addChildToBack(pn, assignExpr(ts, false));
                        }

                    } while (ts.matchToken(ts.COMMA));
                    mustMatchToken(ts, ts.RB, "msg.no.bracket.arg");
                }
                sourceAdd((char)ts.RB);
                return nf.createArrayLiteral(pn);
            }

        case TokenStream.LC: {
            pn = nf.createLeaf(ts.OBJLIT);

            sourceAdd((char)ts.LC);
            if (!ts.matchToken(ts.RC)) {

                boolean first = true;
            commaloop:
                do {
                    Object property;

                    if (!first)
                        sourceAdd((char)ts.COMMA);
                    else
                        first = false;

                    tt = ts.getToken();
                    switch(tt) {
                        // map NAMEs to STRINGs in object literal context.
                    case TokenStream.NAME:
                    case TokenStream.STRING:
                        String s = ts.getString();
                        sourceAddString(ts.NAME, s);
                        property = nf.createString(ts.getString());
                        break;
                    case TokenStream.NUMBER:
                        double n = ts.getNumber();
                        sourceAddNumber(n);
                        property = nf.createNumber(n);
                        break;
                    case TokenStream.RC:
                        // trailing comma is OK.
                        ts.ungetToken(tt);
                        break commaloop;
                    default:
                        reportError(ts, "msg.bad.prop");
                        break commaloop;
                    }
                    mustMatchToken(ts, ts.COLON, "msg.no.colon.prop");

                    // OBJLIT is used as ':' in object literal for
                    // decompilation to solve spacing ambiguity.
                    sourceAdd((char)ts.OBJLIT);
                    nf.addChildToBack(pn, property);
                    nf.addChildToBack(pn, assignExpr(ts, false));

                } while (ts.matchToken(ts.COMMA));

                mustMatchToken(ts, ts.RC, "msg.no.brace.prop");
            }
            sourceAdd((char)ts.RC);
            return nf.createObjectLiteral(pn);
        }

        case TokenStream.LP:

            /* Brendan's IR-jsparse.c makes a new node tagged with
             * TOK_LP here... I'm not sure I understand why.  Isn't
             * the grouping already implicit in the structure of the
             * parse tree?  also TOK_LP is already overloaded (I
             * think) in the C IR as 'function call.'  */
            sourceAdd((char)ts.LP);
            pn = expr(ts, false);
            sourceAdd((char)ts.RP);
            mustMatchToken(ts, ts.RP, "msg.no.paren");
            return pn;

        case TokenStream.NAME:
            String name = ts.getString();
            sourceAddString(ts.NAME, name);
            return nf.createName(name);

        case TokenStream.NUMBER:
            double n = ts.getNumber();
            sourceAddNumber(n);
            return nf.createNumber(n);

        case TokenStream.STRING:
            String s = ts.getString();
            sourceAddString(ts.STRING, s);
            return nf.createString(s);

        case TokenStream.REGEXP:
        {
            String flags = ts.regExpFlags;
            ts.regExpFlags = null;
            String re = ts.getString();
            sourceAddString(ts.REGEXP, '/' + re + '/' + flags);
            return nf.createRegExp(re, flags);
        }

        case TokenStream.PRIMARY:
            sourceAdd((char)ts.PRIMARY);
            sourceAdd((char)ts.getOp());
            return nf.createLeaf(ts.PRIMARY, ts.getOp());

        case TokenStream.RESERVED:
            reportError(ts, "msg.reserved.id");
            break;

        case TokenStream.ERROR:
            /* the scanner or one of its subroutines reported the error. */
            break;

        default:
            reportError(ts, "msg.syntax");
            break;

        }
        return null;    // should never reach here
    }

/**
 * The following methods save decompilation information about the source.
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
    private void sourceAdd(char c) {
        if (sourceTop == sourceBuffer.length) {
            increaseSourceCapacity(sourceTop + 1);
        }
        sourceBuffer[sourceTop] = c;
        ++sourceTop;
    }

    private void sourceAddString(int type, String str) {
        sourceAdd((char)type);
        sourceAddString(str);
    }

    private void sourceAddString(String str) {
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

    private void sourceAddNumber(double n) {
        sourceAdd((char)TokenStream.NUMBER);

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
            sourceAdd('D');
            sourceAdd((char)(lbits >> 48));
            sourceAdd((char)(lbits >> 32));
            sourceAdd((char)(lbits >> 16));
            sourceAdd((char)lbits);
        }
        else {
            // we can ignore negative values, bc they're already prefixed
            // by UNARYOP SUB
               if (Context.check && lbits < 0) Context.codeBug();

            // will it fit in a char?
            // this gives a short encoding for integer values up to 2^16.
            if (lbits <= Character.MAX_VALUE) {
                sourceAdd('S');
                sourceAdd((char)lbits);
            }
            else { // Integral, but won't fit in a char. Store as a long.
                sourceAdd('J');
                sourceAdd((char)(lbits >> 48));
                sourceAdd((char)(lbits >> 32));
                sourceAdd((char)(lbits >> 16));
                sourceAdd((char)lbits);
            }
        }
    }

    private void increaseSourceCapacity(int minimalCapacity) {
        // Call this only when capacity increase is must
        if (Context.check && minimalCapacity <= sourceBuffer.length)
            Context.codeBug();
        int newCapacity = sourceBuffer.length * 2;
        if (newCapacity < minimalCapacity) {
            newCapacity = minimalCapacity;
        }
        char[] tmp = new char[newCapacity];
        System.arraycopy(sourceBuffer, 0, tmp, 0, sourceTop);
        sourceBuffer = tmp;
    }

    private String sourceToString(int offset) {
        if (Context.check && (offset < 0 || sourceTop < offset))
            Context.codeBug();
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
     * @param fromFunctionConstructor true if encodedSourcesTree represents
     *                                result of Function(...)
     *
     * @param version engine version used to compile the source
     *
     * @param indent How much to indent the decompiled result
     *
     * @param justbody Whether the decompilation should omit the
     * function header and trailing brace.
     */
    static String decompile(Object encodedSourcesTree,
                            boolean fromFunctionConstructor, int version,
                            int indent, boolean justbody)
    {
        StringBuffer result = new StringBuffer();
        Object[] srcData = new Object[1];
        int type = fromFunctionConstructor ? CONSTRUCTED_FUNCTION
                                           : TOP_LEVEL_SCRIPT_OR_FUNCTION;

        decompile_r(encodedSourcesTree, version, indent, type, justbody,
                    srcData, result);
        return result.toString();
    }

    private static void decompile_r(Object encodedSourcesTree, int version,
                                    int indent, int type, boolean justbody,
                                    Object[] srcData, StringBuffer result)
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

        if (type != NESTED_FUNCTION) {
            // add an initial newline to exactly match js.
            if (!justbody)
                result.append('\n');
            for (int j = 0; j < indent; j++)
                result.append(' ');
        }

        int i = 0;

        // If the first token is TokenStream.SCRIPT, then we're
        // decompiling the toplevel script, otherwise it a function
        // and should start with TokenStream.FUNCTION

        int token = source.charAt(i);
        ++i;
        if (token == TokenStream.FUNCTION) {
            if (!justbody) {
                result.append("function ");

                /* version != 1.2 Function constructor behavior -
                 * print 'anonymous' as the function name if the
                 * version (under which the function was compiled) is
                 * less than 1.2... or if it's greater than 1.2, because
                 * we need to be closer to ECMA.  (ToSource, please?)
                 */
                if (source.charAt(i) == TokenStream.LP
                    && version != Context.VERSION_1_2
                    && type == CONSTRUCTED_FUNCTION)
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
                if (childNodes == null
                    || functionNumber + 1 > childNodes.length)
                {
                    throw Context.reportRuntimeError(Context.getMessage1
                        ("msg.no.function.ref.found",
                         new Integer(functionNumber)));
                }
                decompile_r(childNodes[functionNumber + 1], version,
                            indent, NESTED_FUNCTION, false, srcData, result);
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
                if (justbody && type != NESTED_FUNCTION && i + 1 == length)
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
                    result.append(version == Context.VERSION_1_2
                                  ? " == " : " === ");
                    break;

                case TokenStream.SHNE:
                    result.append(version == Context.VERSION_1_2
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
        if (type != NESTED_FUNCTION && !justbody)
            result.append('\n');
    }

    private static boolean nextIs(String source, int length, int i, int token) {
        return (i + 1 < length) ? source.charAt(i + 1) == token : false;
    }

    private static int getSourceString(String source, int offset,
                                       Object[] result)
    {
        int length = source.charAt(offset);
        ++offset;
        if ((0x8000 & length) != 0) {
            length = ((0x7FFF & length) << 16) | source.charAt(offset);
            ++offset;
        }
        if (result != null) {
            result[0] = source.substring(offset, offset + length);
        }
        return offset + length;
    }

    private static int getSourceNumber(String source, int offset,
                                       Object[] result)
    {
        char type = source.charAt(offset);
        ++offset;
        if (type == 'S') {
            if (result != null) {
                int ival = source.charAt(offset);
                result[0] = new Integer(ival);
            }
            ++offset;
        } else if (type == 'J' || type == 'D') {
            if (result != null) {
                long lbits;
                lbits = (long)source.charAt(offset) << 48;
                lbits |= (long)source.charAt(offset + 1) << 32;
                lbits |= (long)source.charAt(offset + 2) << 16;
                lbits |= (long)source.charAt(offset + 3);
                double dval;
                if (type == 'J') {
                    dval = lbits;
                } else {
                    dval = Double.longBitsToDouble(lbits);
                }
                result[0] = new Double(dval);
            }
            offset += 4;
        } else {
            // Bad source
            throw new RuntimeException();
        }
        return offset;
    }

    private int lastExprEndLine; // Hack to handle function expr termination.
    private IRFactory nf;
    private ErrorReporter er;
    private boolean ok; // Did the parse encounter an error?

    private char[] sourceBuffer = new char[128];
    private int sourceTop;
    private int functionNumber;

    // how much to indent
    private final static int OFFSET = 4;

    // less how much for case labels
    private final static int SETBACK = 2;

    private static final int TOP_LEVEL_SCRIPT_OR_FUNCTION = 0;
    private static final int CONSTRUCTED_FUNCTION = 1;
    private static final int NESTED_FUNCTION = 2;

    // whether to do a debug print of the source information, when
    // decompiling.
    private static final boolean printSource = false;

}

