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

    public Parser() { }

    public void setLanguageVersion(int languageVersion) {
        this.languageVersion = languageVersion;
    }

    public void setAllowMemberExprAsFunctionName(boolean flag) {
        this.allowMemberExprAsFunctionName = flag;
    }

    private void mustMatchToken(TokenStream ts, int toMatch, String messageId)
        throws IOException, ParserException
    {
        int tt;
        if ((tt = ts.getToken()) != toMatch) {
            reportError(ts, messageId);
            ts.ungetToken(tt); // In case the parser decides to continue
        }
    }

    private void reportError(TokenStream ts, String messageId)
        throws ParserException
    {
        this.ok = false;
        ts.reportCurrentLineError(messageId, null);

        // Throw a ParserException exception to unwind the recursive descent
        // parse.
        throw new ParserException();
    }

    /*
     * Build a parse tree from the given TokenStream.
     *
     * @param ts the TokenStream to parse
     * @param nf the node factory to use to build parse nodes
     *
     * @return an Object representing the parsed
     * program.  If the parse fails, null will be returned.  (The
     * parse failure will result in a call to the current Context's
     * ErrorReporter.)
     */
    public ScriptOrFnNode parse(TokenStream ts, IRFactory nf,
                                Decompiler decompiler)
        throws IOException
    {
        this.decompiler = decompiler;
        this.nf = nf;
        currentScriptOrFn = nf.createScript();

        this.ok = true;

        int tt;          // last token from getToken();
        int baseLineno = ts.getLineno();  // line number where source starts

        /* so we have something to add nodes to until
         * we've collected all the source */
        Object pn = nf.createLeaf(Token.BLOCK);

        while (true) {
            ts.flags |= TokenStream.TSF_REGEXP;
            tt = ts.getToken();
            ts.flags &= ~TokenStream.TSF_REGEXP;

            if (tt <= Token.EOF) {
                break;
            }

            Object n;
            if (tt == Token.FUNCTION) {
                try {
                    n = function(ts, FunctionNode.FUNCTION_STATEMENT);
                } catch (ParserException e) {
                    this.ok = false;
                    break;
                }
            } else {
                ts.ungetToken(tt);
                n = statement(ts);
            }
            nf.addChildToBack(pn, n);
        }

        if (!this.ok) {
            // XXX ts.clearPushback() call here?
            return null;
        }

        String source = decompiler.getEncodedSource();
        this.decompiler = null; // To help GC

        nf.initScript(currentScriptOrFn, pn, ts.getSourceName(),
                      baseLineno, ts.getLineno(), source);
        return currentScriptOrFn;
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
            while((tt = ts.peekToken()) > Token.EOF && tt != Token.RC) {
                Object n;
                if (tt == Token.FUNCTION) {
                    ts.getToken();
                    n = function(ts, FunctionNode.FUNCTION_STATEMENT);
                } else {
                    n = statement(ts);
                }
                nf.addChildToBack(pn, n);
            }
        } catch (ParserException e) {
            this.ok = false;
        } finally {
            // also in finally block:
            // flushNewLines, clearPushback.

            ts.flags = oldflags;
        }

        return pn;
    }

    private Object function(TokenStream ts, int functionType)
        throws IOException, ParserException
    {
        int baseLineno = ts.getLineno();  // line number where source starts

        String name;
        Object memberExprNode = null;
        if (ts.matchToken(Token.NAME)) {
            name = ts.getString();
            if (!ts.matchToken(Token.LP)) {
                if (allowMemberExprAsFunctionName) {
                    // Extension to ECMA: if 'function <name>' does not follow
                    // by '(', assume <name> starts memberExpr
                    decompiler.addName(name);
                    Object memberExprHead = nf.createName(name);
                    name = "";
                    memberExprNode = memberExprTail(ts, false, memberExprHead);
                }
                mustMatchToken(ts, Token.LP, "msg.no.paren.parms");
            }
        } else if (ts.matchToken(Token.LP)) {
            // Anonymous function
            name = "";
        } else {
            name = "";
            if (allowMemberExprAsFunctionName) {
                // Note that memberExpr can not start with '(' like
                // in function (1+2).toString(), because 'function (' already
                // processed as anonymous function
                memberExprNode = memberExpr(ts, false);
            }
            mustMatchToken(ts, Token.LP, "msg.no.paren.parms");
        }

        if (memberExprNode != null) {
            // transform 'function' <memberExpr> to  <memberExpr> = function
            // even in the decompilated source
            decompiler.addOp(Token.ASSIGN, Token.NOP);
        }

        FunctionNode fnNode = nf.createFunction(name);
        int functionIndex = currentScriptOrFn.addFunction(fnNode);

        int functionSourceOffset = decompiler.startFunction();

        ScriptOrFnNode savedScriptOrFn = currentScriptOrFn;
        currentScriptOrFn = fnNode;

        Object body;
        String source;
        try {
            decompiler.addToken(Token.FUNCTION);
            if (name.length() != 0) {
                decompiler.addName(name);
            }
            decompiler.addToken(Token.LP);
            if (!ts.matchToken(Token.RP)) {
                boolean first = true;
                do {
                    if (!first)
                        decompiler.addToken(Token.COMMA);
                    first = false;
                    mustMatchToken(ts, Token.NAME, "msg.no.parm");
                    String s = ts.getString();
                    if (fnNode.hasParamOrVar(s)) {
                        Object[] msgArgs = { s };
                        ts.reportCurrentLineWarning("msg.dup.parms", msgArgs);
                    }
                    fnNode.addParam(s);
                    decompiler.addName(s);
                } while (ts.matchToken(Token.COMMA));

                mustMatchToken(ts, Token.RP, "msg.no.paren.after.parms");
            }
            decompiler.addToken(Token.RP);

            mustMatchToken(ts, Token.LC, "msg.no.brace.body");
            decompiler.addEOL(Token.LC);
            body = parseFunctionBody(ts);
            mustMatchToken(ts, Token.RC, "msg.no.brace.after.body");
            decompiler.addToken(Token.RC);
            // skip the last EOL so nested functions work...
            // name might be null;
        }
        finally {
            source = decompiler.stopFunction(functionSourceOffset);
            currentScriptOrFn = savedScriptOrFn;
        }

        Object pn;
        if (memberExprNode == null) {
            pn = nf.initFunction(fnNode, functionIndex, body,
                                 ts.getSourceName(),
                                 baseLineno, ts.getLineno(),
                                 source,
                                 functionType);
            if (functionType == FunctionNode.FUNCTION_EXPRESSION_STATEMENT) {
                // The following can be removed but then code generators should
                // be modified not to push on the stack function expression
                // statements
                pn = nf.createExprStatementNoReturn(pn, baseLineno);
            }
            // Add EOL but only if function is not part of expression, in which
            // case it gets SEMI + EOL from Statement.
            if (functionType != FunctionNode.FUNCTION_EXPRESSION) {
                decompiler.addToken(Token.EOL);
                checkWellTerminatedFunction(ts);
            }
        } else {
            pn = nf.initFunction(fnNode, functionIndex, body,
                                 ts.getSourceName(),
                                 baseLineno, ts.getLineno(),
                                 source,
                                 FunctionNode.FUNCTION_EXPRESSION);
            pn = nf.createBinary(Token.ASSIGN, Token.NOP, memberExprNode, pn);
            if (functionType != FunctionNode.FUNCTION_EXPRESSION) {
                pn = nf.createExprStatement(pn, baseLineno);
                // Add ';' to make 'function x.f(){}' and 'x.f = function(){}'
                // to print the same strings when decompiling
                decompiler.addEOL(Token.SEMI);
                checkWellTerminatedFunction(ts);
            }
        }
        return pn;
    }

    private Object statements(TokenStream ts)
        throws IOException
    {
        Object pn = nf.createBlock(ts.getLineno());

        int tt;
        while((tt = ts.peekToken()) > Token.EOF && tt != Token.RC) {
            nf.addChildToBack(pn, statement(ts));
        }

        return pn;
    }

    private Object condition(TokenStream ts)
        throws IOException, ParserException
    {
        Object pn;
        mustMatchToken(ts, Token.LP, "msg.no.paren.cond");
        decompiler.addToken(Token.LP);
        pn = expr(ts, false);
        mustMatchToken(ts, Token.RP, "msg.no.paren.after.cond");
        decompiler.addToken(Token.RP);

        // there's a check here in jsparse.c that corrects = to ==

        return pn;
    }

    private void checkWellTerminated(TokenStream ts)
        throws IOException, ParserException
    {
        int tt = ts.peekTokenSameLine();
        switch (tt) {
        case Token.ERROR:
        case Token.EOF:
        case Token.EOL:
        case Token.SEMI:
        case Token.RC:
            return;

        case Token.FUNCTION:
            if (languageVersion < Context.VERSION_1_2) {
              /*
               * Checking against version < 1.2 and version >= 1.0
               * in the above line breaks old javascript, so we keep it
               * this way for now... XXX warning needed?
               */
                return;
            }
        }
        reportError(ts, "msg.no.semi.stmt");
    }

    private void checkWellTerminatedFunction(TokenStream ts)
        throws IOException, ParserException
    {
        if (languageVersion < Context.VERSION_1_2) {
            // See comments in checkWellTerminated
             return;
        }
        checkWellTerminated(ts);
    }

    // match a NAME; return null if no match.
    private String matchLabel(TokenStream ts)
        throws IOException, ParserException
    {
        int lineno = ts.getLineno();

        String label = null;
        int tt;
        tt = ts.peekTokenSameLine();
        if (tt == Token.NAME) {
            ts.getToken();
            label = ts.getString();
        }

        if (lineno == ts.getLineno())
            checkWellTerminated(ts);

        return label;
    }

    private Object statement(TokenStream ts)
        throws IOException
    {
        try {
            return statementHelper(ts);
        } catch (ParserException e) {
            // skip to end of statement
            int lineno = ts.getLineno();
            int t;
            do {
                t = ts.getToken();
            } while (t != Token.SEMI && t != Token.EOL &&
                     t != Token.EOF && t != Token.ERROR);
            return nf.createExprStatement(nf.createName("error"), lineno);
        }
    }

    /**
     * Whether the "catch (e: e instanceof Exception) { ... }" syntax
     * is implemented.
     */

    private Object statementHelper(TokenStream ts)
        throws IOException, ParserException
    {
        Object pn = null;

        // If skipsemi == true, don't add SEMI + EOL to source at the
        // end of this statment.  For compound statements, IF/FOR etc.
        boolean skipsemi = false;

        int tt;

        tt = ts.getToken();

        switch(tt) {
        case Token.IF: {
            skipsemi = true;

            decompiler.addToken(Token.IF);
            int lineno = ts.getLineno();
            Object cond = condition(ts);
            decompiler.addEOL(Token.LC);
            Object ifTrue = statement(ts);
            Object ifFalse = null;
            if (ts.matchToken(Token.ELSE)) {
                decompiler.addToken(Token.RC);
                decompiler.addToken(Token.ELSE);
                decompiler.addEOL(Token.LC);
                ifFalse = statement(ts);
            }
            decompiler.addEOL(Token.RC);
            pn = nf.createIf(cond, ifTrue, ifFalse, lineno);
            break;
        }

        case Token.SWITCH: {
            skipsemi = true;

            decompiler.addToken(Token.SWITCH);
            pn = nf.createSwitch(ts.getLineno());

            Object cur_case = null;  // to kill warning
            Object case_statements;

            mustMatchToken(ts, Token.LP, "msg.no.paren.switch");
            decompiler.addToken(Token.LP);
            nf.addChildToBack(pn, expr(ts, false));
            mustMatchToken(ts, Token.RP, "msg.no.paren.after.switch");
            decompiler.addToken(Token.RP);
            mustMatchToken(ts, Token.LC, "msg.no.brace.switch");
            decompiler.addEOL(Token.LC);

            while ((tt = ts.getToken()) != Token.RC && tt != Token.EOF) {
                switch(tt) {
                case Token.CASE:
                    decompiler.addToken(Token.CASE);
                    cur_case = nf.createUnary(Token.CASE, expr(ts, false));
                    decompiler.addEOL(Token.COLON);
                    break;

                case Token.DEFAULT:
                    cur_case = nf.createLeaf(Token.DEFAULT);
                    decompiler.addToken(Token.DEFAULT);
                    decompiler.addEOL(Token.COLON);
                    // XXX check that there isn't more than one default
                    break;

                default:
                    reportError(ts, "msg.bad.switch");
                    break;
                }
                mustMatchToken(ts, Token.COLON, "msg.no.colon.case");

                case_statements = nf.createLeaf(Token.BLOCK);

                while ((tt = ts.peekToken()) != Token.RC && tt != Token.CASE &&
                        tt != Token.DEFAULT && tt != Token.EOF)
                {
                    nf.addChildToBack(case_statements, statement(ts));
                }
                // assert cur_case
                nf.addChildToBack(cur_case, case_statements);

                nf.addChildToBack(pn, cur_case);
            }
            decompiler.addEOL(Token.RC);
            break;
        }

        case Token.WHILE: {
            skipsemi = true;

            decompiler.addToken(Token.WHILE);
            int lineno = ts.getLineno();
            Object cond = condition(ts);
            decompiler.addEOL(Token.LC);
            Object body = statement(ts);
            decompiler.addEOL(Token.RC);

            pn = nf.createWhile(cond, body, lineno);
            break;

        }

        case Token.DO: {
            decompiler.addToken(Token.DO);
            decompiler.addEOL(Token.LC);

            int lineno = ts.getLineno();

            Object body = statement(ts);

            decompiler.addToken(Token.RC);
            mustMatchToken(ts, Token.WHILE, "msg.no.while.do");
            decompiler.addToken(Token.WHILE);
            Object cond = condition(ts);

            pn = nf.createDoWhile(body, cond, lineno);
            break;
        }

        case Token.FOR: {
            skipsemi = true;

            decompiler.addToken(Token.FOR);
            int lineno = ts.getLineno();

            Object init;  // Node init is also foo in 'foo in Object'
            Object cond;  // Node cond is also object in 'foo in Object'
            Object incr = null; // to kill warning
            Object body;

            mustMatchToken(ts, Token.LP, "msg.no.paren.for");
            decompiler.addToken(Token.LP);
            tt = ts.peekToken();
            if (tt == Token.SEMI) {
                init = nf.createLeaf(Token.VOID);
            } else {
                if (tt == Token.VAR) {
                    // set init to a var list or initial
                    ts.getToken();    // throw away the 'var' token
                    init = variables(ts, true);
                }
                else {
                    init = expr(ts, true);
                }
            }

            tt = ts.peekToken();
            if (tt == Token.RELOP && ts.getOp() == Token.IN) {
                ts.matchToken(Token.RELOP);
                decompiler.addToken(Token.IN);
                // 'cond' is the object over which we're iterating
                cond = expr(ts, false);
            } else {  // ordinary for loop
                mustMatchToken(ts, Token.SEMI, "msg.no.semi.for");
                decompiler.addToken(Token.SEMI);
                if (ts.peekToken() == Token.SEMI) {
                    // no loop condition
                    cond = nf.createLeaf(Token.VOID);
                } else {
                    cond = expr(ts, false);
                }

                mustMatchToken(ts, Token.SEMI, "msg.no.semi.for.cond");
                decompiler.addToken(Token.SEMI);
                if (ts.peekToken() == Token.RP) {
                    incr = nf.createLeaf(Token.VOID);
                } else {
                    incr = expr(ts, false);
                }
            }

            mustMatchToken(ts, Token.RP, "msg.no.paren.for.ctrl");
            decompiler.addToken(Token.RP);
            decompiler.addEOL(Token.LC);
            body = statement(ts);
            decompiler.addEOL(Token.RC);

            if (incr == null) {
                // cond could be null if 'in obj' got eaten by the init node.
                pn = nf.createForIn(init, cond, body, lineno);
            } else {
                pn = nf.createFor(init, cond, incr, body, lineno);
            }
            break;
        }

        case Token.TRY: {
            int lineno = ts.getLineno();

            Object tryblock;
            Object catchblocks = null;
            Object finallyblock = null;

            skipsemi = true;
            decompiler.addToken(Token.TRY);
            decompiler.addEOL(Token.LC);
            tryblock = statement(ts);
            decompiler.addEOL(Token.RC);

            catchblocks = nf.createLeaf(Token.BLOCK);

            boolean sawDefaultCatch = false;
            int peek = ts.peekToken();
            if (peek == Token.CATCH) {
                while (ts.matchToken(Token.CATCH)) {
                    if (sawDefaultCatch) {
                        reportError(ts, "msg.catch.unreachable");
                    }
                    decompiler.addToken(Token.CATCH);
                    mustMatchToken(ts, Token.LP, "msg.no.paren.catch");
                    decompiler.addToken(Token.LP);

                    mustMatchToken(ts, Token.NAME, "msg.bad.catchcond");
                    String varName = ts.getString();
                    decompiler.addName(varName);

                    Object catchCond = null;
                    if (ts.matchToken(Token.IF)) {
                        decompiler.addToken(Token.IF);
                        catchCond = expr(ts, false);
                    } else {
                        sawDefaultCatch = true;
                    }

                    mustMatchToken(ts, Token.RP, "msg.bad.catchcond");
                    decompiler.addToken(Token.RP);
                    mustMatchToken(ts, Token.LC, "msg.no.brace.catchblock");
                    decompiler.addEOL(Token.LC);

                    nf.addChildToBack(catchblocks,
                        nf.createCatch(varName, catchCond,
                                       statements(ts),
                                       ts.getLineno()));

                    mustMatchToken(ts, Token.RC, "msg.no.brace.after.body");
                    decompiler.addEOL(Token.RC);
                }
            } else if (peek != Token.FINALLY) {
                mustMatchToken(ts, Token.FINALLY, "msg.try.no.catchfinally");
            }

            if (ts.matchToken(Token.FINALLY)) {
                decompiler.addToken(Token.FINALLY);
                decompiler.addEOL(Token.LC);
                finallyblock = statement(ts);
                decompiler.addEOL(Token.RC);
            }

            pn = nf.createTryCatchFinally(tryblock, catchblocks,
                                          finallyblock, lineno);

            break;
        }
        case Token.THROW: {
            int lineno = ts.getLineno();
            decompiler.addToken(Token.THROW);
            pn = nf.createThrow(expr(ts, false), lineno);
            if (lineno == ts.getLineno())
                checkWellTerminated(ts);
            break;
        }
        case Token.BREAK: {
            int lineno = ts.getLineno();

            decompiler.addToken(Token.BREAK);

            // matchLabel only matches if there is one
            String label = matchLabel(ts);
            if (label != null) {
                decompiler.addName(label);
            }
            pn = nf.createBreak(label, lineno);
            break;
        }
        case Token.CONTINUE: {
            int lineno = ts.getLineno();

            decompiler.addToken(Token.CONTINUE);

            // matchLabel only matches if there is one
            String label = matchLabel(ts);
            if (label != null) {
                decompiler.addName(label);
            }
            pn = nf.createContinue(label, lineno);
            break;
        }
        case Token.WITH: {
            skipsemi = true;

            decompiler.addToken(Token.WITH);
            int lineno = ts.getLineno();
            mustMatchToken(ts, Token.LP, "msg.no.paren.with");
            decompiler.addToken(Token.LP);
            Object obj = expr(ts, false);
            mustMatchToken(ts, Token.RP, "msg.no.paren.after.with");
            decompiler.addToken(Token.RP);
            decompiler.addEOL(Token.LC);

            Object body = statement(ts);

            decompiler.addEOL(Token.RC);

            pn = nf.createWith(obj, body, lineno);
            break;
        }
        case Token.VAR: {
            int lineno = ts.getLineno();
            pn = variables(ts, false);
            if (ts.getLineno() == lineno)
                checkWellTerminated(ts);
            break;
        }
        case Token.RETURN: {
            Object retExpr = null;

            decompiler.addToken(Token.RETURN);

            // bail if we're not in a (toplevel) function
            if ((ts.flags & ts.TSF_FUNCTION) == 0)
                reportError(ts, "msg.bad.return");

            /* This is ugly, but we don't want to require a semicolon. */
            ts.flags |= ts.TSF_REGEXP;
            tt = ts.peekTokenSameLine();
            ts.flags &= ~ts.TSF_REGEXP;

            int lineno = ts.getLineno();
            if (tt != Token.EOF && tt != Token.EOL && tt != Token.SEMI && tt != Token.RC) {
                retExpr = expr(ts, false);
                if (ts.getLineno() == lineno)
                    checkWellTerminated(ts);
                ts.flags |= ts.TSF_RETURN_EXPR;
            } else {
                ts.flags |= ts.TSF_RETURN_VOID;
            }

            // XXX ASSERT pn
            pn = nf.createReturn(retExpr, lineno);
            break;
        }
        case Token.LC:
            skipsemi = true;

            pn = statements(ts);
            mustMatchToken(ts, Token.RC, "msg.no.brace.block");
            break;

        case Token.ERROR:
            // Fall thru, to have a node for error recovery to work on
        case Token.EOL:
        case Token.SEMI:
            pn = nf.createLeaf(Token.VOID);
            skipsemi = true;
            break;

        case Token.FUNCTION: {
            pn = function(ts, FunctionNode.FUNCTION_EXPRESSION_STATEMENT);
            break;
        }

        default: {
                int lastExprType = tt;
                int tokenno = ts.getTokenno();
                ts.ungetToken(tt);
                int lineno = ts.getLineno();

                pn = expr(ts, false);

                if (ts.peekToken() == Token.COLON) {
                    /* check that the last thing the tokenizer returned was a
                     * NAME and that only one token was consumed.
                     */
                    if (lastExprType != Token.NAME || (ts.getTokenno() != tokenno))
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
                    decompiler.addEOL(Token.COLON);
                    return pn;
                }

                pn = nf.createExprStatement(pn, lineno);

                if (ts.getLineno() == lineno) {
                    checkWellTerminated(ts);
                }
                break;
            }
        }
        ts.matchToken(Token.SEMI);
        if (!skipsemi) {
            decompiler.addEOL(Token.SEMI);
        }

        return pn;
    }

    private Object variables(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = nf.createVariables(ts.getLineno());
        boolean first = true;

        decompiler.addToken(Token.VAR);

        for (;;) {
            Object name;
            Object init;
            mustMatchToken(ts, Token.NAME, "msg.bad.var");
            String s = ts.getString();

            if (!first)
                decompiler.addToken(Token.COMMA);
            first = false;

            decompiler.addName(s);
            currentScriptOrFn.addVar(s);
            name = nf.createName(s);

            // omitted check for argument hiding

            if (ts.matchToken(Token.ASSIGN)) {
                if (ts.getOp() != Token.NOP)
                    reportError(ts, "msg.bad.var.init");

                decompiler.addOp(Token.ASSIGN, Token.NOP);

                init = assignExpr(ts, inForInit);
                nf.addChildToBack(name, init);
            }
            nf.addChildToBack(pn, name);
            if (!ts.matchToken(Token.COMMA))
                break;
        }
        return pn;
    }

    private Object expr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = assignExpr(ts, inForInit);
        while (ts.matchToken(Token.COMMA)) {
            decompiler.addToken(Token.COMMA);
            pn = nf.createBinary(Token.COMMA, pn, assignExpr(ts, inForInit));
        }
        return pn;
    }

    private Object assignExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = condExpr(ts, inForInit);

        if (ts.matchToken(Token.ASSIGN)) {
            // omitted: "invalid assignment left-hand side" check.
            int op = ts.getOp();
            decompiler.addOp(Token.ASSIGN, op);
            pn = nf.createBinary(Token.ASSIGN, op, pn,
                                 assignExpr(ts, inForInit));
        }

        return pn;
    }

    private Object condExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object ifTrue;
        Object ifFalse;

        Object pn = orExpr(ts, inForInit);

        if (ts.matchToken(Token.HOOK)) {
            decompiler.addToken(Token.HOOK);
            ifTrue = assignExpr(ts, false);
            mustMatchToken(ts, Token.COLON, "msg.no.colon.cond");
            decompiler.addToken(Token.COLON);
            ifFalse = assignExpr(ts, inForInit);
            return nf.createTernary(pn, ifTrue, ifFalse);
        }

        return pn;
    }

    private Object orExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = andExpr(ts, inForInit);
        if (ts.matchToken(Token.OR)) {
            decompiler.addToken(Token.OR);
            pn = nf.createBinary(Token.OR, pn, orExpr(ts, inForInit));
        }

        return pn;
    }

    private Object andExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = bitOrExpr(ts, inForInit);
        if (ts.matchToken(Token.AND)) {
            decompiler.addToken(Token.AND);
            pn = nf.createBinary(Token.AND, pn, andExpr(ts, inForInit));
        }

        return pn;
    }

    private Object bitOrExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = bitXorExpr(ts, inForInit);
        while (ts.matchToken(Token.BITOR)) {
            decompiler.addToken(Token.BITOR);
            pn = nf.createBinary(Token.BITOR, pn, bitXorExpr(ts, inForInit));
        }
        return pn;
    }

    private Object bitXorExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = bitAndExpr(ts, inForInit);
        while (ts.matchToken(Token.BITXOR)) {
            decompiler.addToken(Token.BITXOR);
            pn = nf.createBinary(Token.BITXOR, pn, bitAndExpr(ts, inForInit));
        }
        return pn;
    }

    private Object bitAndExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = eqExpr(ts, inForInit);
        while (ts.matchToken(Token.BITAND)) {
            decompiler.addToken(Token.BITAND);
            pn = nf.createBinary(Token.BITAND, pn, eqExpr(ts, inForInit));
        }
        return pn;
    }

    private Object eqExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = relExpr(ts, inForInit);
        while (ts.matchToken(Token.EQOP)) {
            int op = ts.getOp();
            int decompilerOp = op;
            if (languageVersion == Context.VERSION_1_2) {
                if (op == Token.SHEQ) {
                    /*
                     * Emulate the C engine; if we're under version
                     * 1.2, then the == operator behaves like the ===
                     * operator (and the source is generated by
                     * decompiling a === opcode), so print the ===
                     * operator as ==.
                     */
                    decompilerOp = Token.EQ;
                } else if (op == Token.SHNE) {
                    decompilerOp = Token.NE;
                }
            }
            decompiler.addOp(Token.EQOP, decompilerOp);
            pn = nf.createBinary(Token.EQOP, op, pn, relExpr(ts, inForInit));
        }
        return pn;
    }

    private Object relExpr(TokenStream ts, boolean inForInit)
        throws IOException, ParserException
    {
        Object pn = shiftExpr(ts);
        while (ts.matchToken(Token.RELOP)) {
            int op = ts.getOp();
            if (inForInit && op == Token.IN) {
                ts.ungetToken(Token.RELOP);
                break;
            }
            decompiler.addOp(Token.RELOP, op);
            pn = nf.createBinary(Token.RELOP, op, pn, shiftExpr(ts));
        }
        return pn;
    }

    private Object shiftExpr(TokenStream ts)
        throws IOException, ParserException
    {
        Object pn = addExpr(ts);
        while (ts.matchToken(Token.SHOP)) {
            int op = ts.getOp();
            decompiler.addOp(Token.SHOP, op);
            pn = nf.createBinary(op, pn, addExpr(ts));
        }
        return pn;
    }

    private Object addExpr(TokenStream ts)
        throws IOException, ParserException
    {
        int tt;
        Object pn = mulExpr(ts);

        while ((tt = ts.getToken()) == Token.ADD || tt == Token.SUB) {
            decompiler.addToken(tt);
            // flushNewLines
            pn = nf.createBinary(tt, pn, mulExpr(ts));
        }
        ts.ungetToken(tt);

        return pn;
    }

    private Object mulExpr(TokenStream ts)
        throws IOException, ParserException
    {
        int tt;

        Object pn = unaryExpr(ts);

        while ((tt = ts.peekToken()) == Token.MUL ||
               tt == Token.DIV ||
               tt == Token.MOD) {
            tt = ts.getToken();
            decompiler.addToken(tt);
            pn = nf.createBinary(tt, pn, unaryExpr(ts));
        }

        return pn;
    }

    private Object unaryExpr(TokenStream ts)
        throws IOException, ParserException
    {
        int tt;

        ts.flags |= ts.TSF_REGEXP;
        tt = ts.getToken();
        ts.flags &= ~ts.TSF_REGEXP;

        switch(tt) {
        case Token.UNARYOP:
            int op = ts.getOp();
            decompiler.addOp(Token.UNARYOP, op);
            return nf.createUnary(Token.UNARYOP, op, unaryExpr(ts));

        case Token.ADD:
        case Token.SUB:
            decompiler.addOp(Token.UNARYOP, tt);
            return nf.createUnary(Token.UNARYOP, tt, unaryExpr(ts));

        case Token.INC:
        case Token.DEC:
            decompiler.addToken(tt);
            return nf.createUnary(tt, Token.PRE, memberExpr(ts, true));

        case Token.DELPROP:
            decompiler.addToken(Token.DELPROP);
            return nf.createUnary(Token.DELPROP, unaryExpr(ts));

        case Token.ERROR:
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
            if (((peeked = ts.peekToken()) == Token.INC ||
                 peeked == Token.DEC) &&
                ts.getLineno() == lineno)
            {
                int pf = ts.getToken();
                decompiler.addToken(pf);
                return nf.createUnary(pf, Token.POST, pn);
            }
            return pn;
        }
        return nf.createName("err"); // Only reached on error.  Try to continue.

    }

    private Object argumentList(TokenStream ts, Object listNode)
        throws IOException, ParserException
    {
        boolean matched;
        ts.flags |= ts.TSF_REGEXP;
        matched = ts.matchToken(Token.RP);
        ts.flags &= ~ts.TSF_REGEXP;
        if (!matched) {
            boolean first = true;
            do {
                if (!first)
                    decompiler.addToken(Token.COMMA);
                first = false;
                nf.addChildToBack(listNode, assignExpr(ts, false));
            } while (ts.matchToken(Token.COMMA));

            mustMatchToken(ts, Token.RP, "msg.no.paren.arg");
        }
        decompiler.addToken(Token.RP);
        return listNode;
    }

    private Object memberExpr(TokenStream ts, boolean allowCallSyntax)
        throws IOException, ParserException
    {
        int tt;

        Object pn;

        /* Check for new expressions. */
        ts.flags |= ts.TSF_REGEXP;
        tt = ts.peekToken();
        ts.flags &= ~ts.TSF_REGEXP;
        if (tt == Token.NEW) {
            /* Eat the NEW token. */
            ts.getToken();
            decompiler.addToken(Token.NEW);

            /* Make a NEW node to append to. */
            pn = nf.createLeaf(Token.NEW);
            nf.addChildToBack(pn, memberExpr(ts, false));

            if (ts.matchToken(Token.LP)) {
                decompiler.addToken(Token.LP);
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
            if (tt == Token.LC) {
                nf.addChildToBack(pn, primaryExpr(ts));
            }
        } else {
            pn = primaryExpr(ts);
        }

        return memberExprTail(ts, allowCallSyntax, pn);
    }

    private Object memberExprTail(TokenStream ts, boolean allowCallSyntax,
                                  Object pn)
        throws IOException, ParserException
    {
        int tt;
        while ((tt = ts.getToken()) > Token.EOF) {
            if (tt == Token.DOT) {
                decompiler.addToken(Token.DOT);
                mustMatchToken(ts, Token.NAME, "msg.no.name.after.dot");
                String s = ts.getString();
                decompiler.addName(s);
                pn = nf.createBinary(Token.DOT, pn,
                                     nf.createName(ts.getString()));
                /* pn = nf.createBinary(Token.DOT, pn, memberExpr(ts))
                 * is the version in Brendan's IR C version.  Not in ECMA...
                 * does it reflect the 'new' operator syntax he mentioned?
                 */
            } else if (tt == Token.LB) {
                decompiler.addToken(Token.LB);
                pn = nf.createBinary(Token.LB, pn, expr(ts, false));

                mustMatchToken(ts, Token.RB, "msg.no.bracket.index");
                decompiler.addToken(Token.RB);
            } else if (allowCallSyntax && tt == Token.LP) {
                /* make a call node */

                pn = nf.createUnary(Token.CALL, pn);
                decompiler.addToken(Token.LP);

                /* Add the arguments to pn, if any are supplied. */
                pn = argumentList(ts, pn);
            } else {
                ts.ungetToken(tt);

                break;
            }
        }
        return pn;
    }

    private Object primaryExpr(TokenStream ts)
        throws IOException, ParserException
    {
        int tt;

        Object pn;

        ts.flags |= ts.TSF_REGEXP;
        tt = ts.getToken();
        ts.flags &= ~ts.TSF_REGEXP;

        switch(tt) {

        case Token.FUNCTION:
            return function(ts, FunctionNode.FUNCTION_EXPRESSION);

        case Token.LB:
            {
                decompiler.addToken(Token.LB);
                pn = nf.createLeaf(Token.ARRAYLIT);

                ts.flags |= ts.TSF_REGEXP;
                boolean matched = ts.matchToken(Token.RB);
                ts.flags &= ~ts.TSF_REGEXP;

                if (!matched) {
                    boolean first = true;
                    do {
                        ts.flags |= ts.TSF_REGEXP;
                        tt = ts.peekToken();
                        ts.flags &= ~ts.TSF_REGEXP;

                        if (!first)
                            decompiler.addToken(Token.COMMA);
                        else
                            first = false;

                        if (tt == Token.RB) {  // to fix [,,,].length behavior...
                            break;
                        }

                        if (tt == Token.COMMA) {
                            nf.addChildToBack(pn, nf.createLeaf(Token.PRIMARY,
                                                                Token.UNDEFINED));
                        } else {
                            nf.addChildToBack(pn, assignExpr(ts, false));
                        }

                    } while (ts.matchToken(Token.COMMA));
                    mustMatchToken(ts, Token.RB, "msg.no.bracket.arg");
                }
                decompiler.addToken(Token.RB);
                return nf.createArrayLiteral(pn);
            }

        case Token.LC: {
            pn = nf.createLeaf(Token.OBJLIT);

            decompiler.addToken(Token.LC);
            if (!ts.matchToken(Token.RC)) {

                boolean first = true;
            commaloop:
                do {
                    Object property;

                    if (!first)
                        decompiler.addToken(Token.COMMA);
                    else
                        first = false;

                    tt = ts.getToken();
                    switch(tt) {
                        // map NAMEs to STRINGs in object literal context.
                    case Token.NAME:
                    case Token.STRING:
                        String s = ts.getString();
                        decompiler.addName(s);
                        property = nf.createString(ts.getString());
                        break;
                    case Token.NUMBER:
                        double n = ts.getNumber();
                        decompiler.addNumber(n);
                        property = nf.createNumber(n);
                        break;
                    case Token.RC:
                        // trailing comma is OK.
                        ts.ungetToken(tt);
                        break commaloop;
                    default:
                        reportError(ts, "msg.bad.prop");
                        break commaloop;
                    }
                    mustMatchToken(ts, Token.COLON, "msg.no.colon.prop");

                    // OBJLIT is used as ':' in object literal for
                    // decompilation to solve spacing ambiguity.
                    decompiler.addToken(Token.OBJLIT);
                    nf.addChildToBack(pn, property);
                    nf.addChildToBack(pn, assignExpr(ts, false));

                } while (ts.matchToken(Token.COMMA));

                mustMatchToken(ts, Token.RC, "msg.no.brace.prop");
            }
            decompiler.addToken(Token.RC);
            return nf.createObjectLiteral(pn);
        }

        case Token.LP:

            /* Brendan's IR-jsparse.c makes a new node tagged with
             * TOK_LP here... I'm not sure I understand why.  Isn't
             * the grouping already implicit in the structure of the
             * parse tree?  also TOK_LP is already overloaded (I
             * think) in the C IR as 'function call.'  */
            decompiler.addToken(Token.LP);
            pn = expr(ts, false);
            decompiler.addToken(Token.RP);
            mustMatchToken(ts, Token.RP, "msg.no.paren");
            return pn;

        case Token.NAME:
            String name = ts.getString();
            decompiler.addName(name);
            return nf.createName(name);

        case Token.NUMBER:
            double n = ts.getNumber();
            decompiler.addNumber(n);
            return nf.createNumber(n);

        case Token.STRING:
            String s = ts.getString();
            decompiler.addString(s);
            return nf.createString(s);

        case Token.REGEXP:
        {
            String flags = ts.regExpFlags;
            ts.regExpFlags = null;
            String re = ts.getString();
            decompiler.addRegexp(re, flags);
            int index = currentScriptOrFn.addRegexp(re, flags);
            return nf.createRegExp(index);
        }

        case Token.PRIMARY:
            int op = ts.getOp();
            decompiler.addOp(Token.PRIMARY, op);
            return nf.createLeaf(Token.PRIMARY, op);

        case Token.RESERVED:
            reportError(ts, "msg.reserved.id");
            break;

        case Token.ERROR:
            /* the scanner or one of its subroutines reported the error. */
            break;

        default:
            reportError(ts, "msg.syntax");
            break;

        }
        return null;    // should never reach here
    }

    private IRFactory nf;
    private int languageVersion = Context.VERSION_DEFAULT;
    private boolean allowMemberExprAsFunctionName = false;

    private boolean ok; // Did the parse encounter an error?

    private ScriptOrFnNode currentScriptOrFn;

    Decompiler decompiler;
}

// Exception to unwind
class ParserException extends Exception { }
