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

import org.mozilla.javascript.ErrorReporter;
import org.mozilla.javascript.Context;
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
        Source source = new Source();

        int tt;          // last token from getToken();
        int baseLineno = ts.getLineno();  // line number where source starts

        /* so we have something to add nodes to until
         * we've collected all the source */
        Object tempBlock = nf.createLeaf(TokenStream.BLOCK);

        while (true) {
            ts.flags |= ts.TSF_REGEXP;
            tt = ts.getToken();
            ts.flags &= ~ts.TSF_REGEXP;

            if (tt <= ts.EOF) {
                break;
            }

            if (tt == ts.FUNCTION) {
                try {
                    nf.addChildToBack(tempBlock, function(ts, source, false));
                    /* function doesn't add its own final EOL,
                     * because it gets SEMI + EOL from Statement when it's
                     * a nested function; so we need to explicitly add an
                     * EOL here.
                     */
                    source.append((char)ts.EOL);
                    wellTerminated(ts, ts.FUNCTION);
                } catch (JavaScriptException e) {
                    this.ok = false;
                    break;
                }
            } else {
                ts.ungetToken(tt);
                nf.addChildToBack(tempBlock, statement(ts, source));
            }
        }

        if (!this.ok) {
            // XXX ts.clearPushback() call here?
            return null;
        }

        Object pn = nf.createScript(tempBlock, ts.getSourceName(),
                                    baseLineno, ts.getLineno(),
                                    source.buf.toString());
        return pn;
    }

    /*
     * The C version of this function takes an argument list,
     * which doesn't seem to be needed for tree generation...
     * it'd only be useful for checking argument hiding, which
     * I'm not doing anyway...
     */
    private Object parseFunctionBody(TokenStream ts, Source source)
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
                    nf.addChildToBack(pn, function(ts, source, false));
                    /* function doesn't add its own final EOL,
                     * because it gets SEMI + EOL from Statement when it's
                     * a nested function; so we need to explicitly add an
                     * EOL here.
                     */
                    source.append((char)ts.EOL);
                    wellTerminated(ts, ts.FUNCTION);       
                } else {
                    nf.addChildToBack(pn, statement(ts, source));
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

    private Object function(TokenStream ts, Source source, boolean isExpr)
        throws IOException, JavaScriptException
    {
        String name = null;
        Object args = nf.createLeaf(ts.LP);
        Object body;
        int baseLineno = ts.getLineno();  // line number where source starts

        // save a reference to the function in the enclosing source.
        source.append((char) ts.FUNCTION);
        source.append(source.functionNumber);
        source.functionNumber++;

        // make a new Source for the enclosed function
        source = new Source();

        // FUNCTION as the first token in a Source means it's a function
        // definition, and not a reference.
        source.append((char) ts.FUNCTION);

        if (ts.matchToken(ts.NAME)) {
            name = ts.getString();
            source.addString(ts.NAME, name);
        }
        else
            ; // it's an anonymous function

        mustMatchToken(ts, ts.LP, "msg.no.paren.parms");
        source.append((char) ts.LP);

        if (!ts.matchToken(ts.RP)) {
            boolean first = true;
            do {
                if (!first)
                    source.append((char)ts.COMMA);
                first = false;
                mustMatchToken(ts, ts.NAME, "msg.no.parm");
                String s = ts.getString();
                nf.addChildToBack(args, nf.createName(s));

                source.addString(ts.NAME, s);
            } while (ts.matchToken(ts.COMMA));

            mustMatchToken(ts, ts.RP, "msg.no.paren.after.parms");
        }
        source.append((char)ts.RP);

        mustMatchToken(ts, ts.LC, "msg.no.brace.body");
        source.append((char)ts.LC);
        source.append((char)ts.EOL);
        body = parseFunctionBody(ts, source);
        mustMatchToken(ts, ts.RC, "msg.no.brace.after.body");
        source.append((char)ts.RC);
        // skip the last EOL so nested functions work...

        // name might be null;
        return nf.createFunction(name, args, body,
                                 ts.getSourceName(),
                                 baseLineno, ts.getLineno(),
                                 source.buf.toString(),
                                 isExpr);
    }

    private Object statements(TokenStream ts, Source source)
        throws IOException
    {
        Object pn = nf.createBlock(ts.getLineno());

        int tt;
        while((tt = ts.peekToken()) > ts.EOF && tt != ts.RC) {
            nf.addChildToBack(pn, statement(ts, source));
        }

        return pn;
    }

    private Object condition(TokenStream ts, Source source)
        throws IOException, JavaScriptException
    {
        Object pn;
        mustMatchToken(ts, ts.LP, "msg.no.paren.cond");
        source.append((char)ts.LP);
        pn = expr(ts, source, false);
        mustMatchToken(ts, ts.RP, "msg.no.paren.after.cond");
        source.append((char)ts.RP);

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

    private Object statement(TokenStream ts, Source source) 
        throws IOException
    {
        try {
            return statementHelper(ts, source);
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
   
    private Object statementHelper(TokenStream ts, Source source)
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

            source.append((char)ts.IF);
            int lineno = ts.getLineno();
            Object cond = condition(ts, source);
            source.append((char)ts.LC);
            source.append((char)ts.EOL);
            Object ifTrue = statement(ts, source);
            Object ifFalse = null;
            if (ts.matchToken(ts.ELSE)) {
                source.append((char)ts.RC);
                source.append((char)ts.ELSE);
                source.append((char)ts.LC);
                source.append((char)ts.EOL);
                ifFalse = statement(ts, source);
            }
            source.append((char)ts.RC);
            source.append((char)ts.EOL);
            pn = nf.createIf(cond, ifTrue, ifFalse, lineno);
            break;
        }

        case TokenStream.SWITCH: {
            skipsemi = true;

            source.append((char)ts.SWITCH);
            pn = nf.createSwitch(ts.getLineno());

            Object cur_case = null;  // to kill warning
            Object case_statements;

            mustMatchToken(ts, ts.LP, "msg.no.paren.switch");
            source.append((char)ts.LP);
            nf.addChildToBack(pn, expr(ts, source, false));
            mustMatchToken(ts, ts.RP, "msg.no.paren.after.switch");
            source.append((char)ts.RP);
            mustMatchToken(ts, ts.LC, "msg.no.brace.switch");
            source.append((char)ts.LC);
            source.append((char)ts.EOL);

            while ((tt = ts.getToken()) != ts.RC && tt != ts.EOF) {
                switch(tt) {
                case TokenStream.CASE:
                    source.append((char)ts.CASE);
                    cur_case = nf.createUnary(ts.CASE, expr(ts, source, false));
                    source.append((char)ts.COLON);
                    source.append((char)ts.EOL);
                    break;

                case TokenStream.DEFAULT:
                    cur_case = nf.createLeaf(ts.DEFAULT);
                    source.append((char)ts.DEFAULT);
                    source.append((char)ts.COLON);
                    source.append((char)ts.EOL);
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
                    nf.addChildToBack(case_statements, statement(ts, source));
                }
                // assert cur_case
                nf.addChildToBack(cur_case, case_statements);

                nf.addChildToBack(pn, cur_case);
            }
            source.append((char)ts.RC);
            source.append((char)ts.EOL);
            break;
        }

        case TokenStream.WHILE: {
            skipsemi = true;

            source.append((char)ts.WHILE);
            int lineno = ts.getLineno();
            Object cond = condition(ts, source);
            source.append((char)ts.LC);
            source.append((char)ts.EOL);
            Object body = statement(ts, source);
            source.append((char)ts.RC);
            source.append((char)ts.EOL);

            pn = nf.createWhile(cond, body, lineno);
            break;

        }

        case TokenStream.DO: {
            source.append((char)ts.DO);
            source.append((char)ts.LC);
            source.append((char)ts.EOL);

            int lineno = ts.getLineno();

            Object body = statement(ts, source);

            source.append((char)ts.RC);
            mustMatchToken(ts, ts.WHILE, "msg.no.while.do");
            source.append((char)ts.WHILE);
            Object cond = condition(ts, source);

            pn = nf.createDoWhile(body, cond, lineno);
            break;
        }

        case TokenStream.FOR: {
            skipsemi = true;

            source.append((char)ts.FOR);
            int lineno = ts.getLineno();

            Object init;  // Node init is also foo in 'foo in Object'
            Object cond;  // Node cond is also object in 'foo in Object'
            Object incr = null; // to kill warning
            Object body;

            mustMatchToken(ts, ts.LP, "msg.no.paren.for");
            source.append((char)ts.LP);
            tt = ts.peekToken();
            if (tt == ts.SEMI) {
                init = nf.createLeaf(ts.VOID);
            } else {
                if (tt == ts.VAR) {
                    // set init to a var list or initial
                    ts.getToken();    // throw away the 'var' token
                    init = variables(ts, source, true);
                }
                else {
                    init = expr(ts, source, true);
                }
            }

            tt = ts.peekToken();
            if (tt == ts.RELOP && ts.getOp() == ts.IN) {
                ts.matchToken(ts.RELOP);
                source.append((char)ts.IN);
                // 'cond' is the object over which we're iterating
                cond = expr(ts, source, false);
            } else {  // ordinary for loop
                mustMatchToken(ts, ts.SEMI,
                               "msg.no.semi.for");
                source.append((char)ts.SEMI);
                if (ts.peekToken() == ts.SEMI) {
                    // no loop condition
                    cond = nf.createLeaf(ts.VOID);
                } else {
                    cond = expr(ts, source, false);
                }

                mustMatchToken(ts, ts.SEMI,
                               "msg.no.semi.for.cond");
                source.append((char)ts.SEMI);
                if (ts.peekToken() == ts.RP) {
                    incr = nf.createLeaf(ts.VOID);
                } else {
                    incr = expr(ts, source, false);
                }
            }

            mustMatchToken(ts, ts.RP, "msg.no.paren.for.ctrl");
            source.append((char)ts.RP);
            source.append((char)ts.LC);
            source.append((char)ts.EOL);
            body = statement(ts, source);
            source.append((char)ts.RC);
            source.append((char)ts.EOL);

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
            source.append((char)ts.TRY);
            source.append((char)ts.LC);
            source.append((char)ts.EOL);
            tryblock = statement(ts, source);
            source.append((char)ts.RC);
            source.append((char)ts.EOL);

            catchblocks = nf.createLeaf(TokenStream.BLOCK);

            boolean sawDefaultCatch = false;
            int peek = ts.peekToken();
            if (peek == ts.CATCH) {
                while (ts.matchToken(ts.CATCH)) {
                    if (sawDefaultCatch) {
                        reportError(ts, "msg.catch.unreachable");
                    }
                    source.append((char)ts.CATCH);
                    mustMatchToken(ts, ts.LP, "msg.no.paren.catch");
                    source.append((char)ts.LP);

                    mustMatchToken(ts, ts.NAME, "msg.bad.catchcond");
                    String varName = ts.getString();
                    source.addString(ts.NAME, varName);
                    
                    Object catchCond = null;
                    if (ts.matchToken(ts.IF)) {
                        source.append((char)ts.IF);
                        catchCond = expr(ts, source, false);
                    } else {
                        sawDefaultCatch = true;
                    }

                    mustMatchToken(ts, ts.RP, "msg.bad.catchcond");
                    source.append((char)ts.RP);
                    mustMatchToken(ts, ts.LC, "msg.no.brace.catchblock");
                    source.append((char)ts.LC);
                    source.append((char)ts.EOL);
                    
                    nf.addChildToBack(catchblocks, 
                        nf.createCatch(varName, catchCond, 
                                       statements(ts, source), 
                                       ts.getLineno()));

                    mustMatchToken(ts, ts.RC, "msg.no.brace.after.body");
                    source.append((char)ts.RC);
                    source.append((char)ts.EOL);
                }
            } else if (peek != ts.FINALLY) {
                mustMatchToken(ts, ts.FINALLY, "msg.try.no.catchfinally");
            }

            if (ts.matchToken(ts.FINALLY)) {
                source.append((char)ts.FINALLY);

                source.append((char)ts.LC);
                source.append((char)ts.EOL);
                finallyblock = statement(ts, source);
                source.append((char)ts.RC);
                source.append((char)ts.EOL);
            }

            pn = nf.createTryCatchFinally(tryblock, catchblocks,
                                          finallyblock, lineno);

            break;
        }
        case TokenStream.THROW: {
            int lineno = ts.getLineno();
            source.append((char)ts.THROW);
            pn = nf.createThrow(expr(ts, source, false), lineno);
            if (lineno == ts.getLineno())
                wellTerminated(ts, ts.ERROR);
            break;
        }
        case TokenStream.BREAK: {
            int lineno = ts.getLineno();

            source.append((char)ts.BREAK);

            // matchLabel only matches if there is one
            String label = matchLabel(ts);
            if (label != null) {
                source.addString(ts.NAME, label);
            }
            pn = nf.createBreak(label, lineno);
            break;
        }
        case TokenStream.CONTINUE: {
            int lineno = ts.getLineno();

            source.append((char)ts.CONTINUE);

            // matchLabel only matches if there is one
            String label = matchLabel(ts);
            if (label != null) {
                source.addString(ts.NAME, label);
            }
            pn = nf.createContinue(label, lineno);
            break;
        }
        case TokenStream.WITH: {
            skipsemi = true;

            source.append((char)ts.WITH);
            int lineno = ts.getLineno();
            mustMatchToken(ts, ts.LP, "msg.no.paren.with");
            source.append((char)ts.LP);
            Object obj = expr(ts, source, false);
            mustMatchToken(ts, ts.RP, "msg.no.paren.after.with");
            source.append((char)ts.RP);
            source.append((char)ts.LC);
            source.append((char)ts.EOL);

            Object body = statement(ts, source);

            source.append((char)ts.RC);
            source.append((char)ts.EOL);

            pn = nf.createWith(obj, body, lineno);
            break;
        }
        case TokenStream.VAR: {
            int lineno = ts.getLineno();
            pn = variables(ts, source, false);
            if (ts.getLineno() == lineno)
                wellTerminated(ts, ts.ERROR);
            break;
        }
        case TokenStream.RETURN: {
            Object retExpr = null;
            int lineno = 0;

            source.append((char)ts.RETURN);

            // bail if we're not in a (toplevel) function
            if ((ts.flags & ts.TSF_FUNCTION) == 0)
                reportError(ts, "msg.bad.return");

            /* This is ugly, but we don't want to require a semicolon. */
            ts.flags |= ts.TSF_REGEXP;
            tt = ts.peekTokenSameLine();
            ts.flags &= ~ts.TSF_REGEXP;

            if (tt != ts.EOF && tt != ts.EOL && tt != ts.SEMI && tt != ts.RC) {
                lineno = ts.getLineno();
                retExpr = expr(ts, source, false);
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

            pn = statements(ts, source);
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

                pn = expr(ts, source, false);

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
                    source.append((char)ts.COLON);
                    source.append((char)ts.EOL);
                    return pn;
                }

                if (lastExprType == ts.FUNCTION)
                    nf.setFunctionExpressionStatement(pn);

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
            source.append((char)ts.SEMI);
            source.append((char)ts.EOL);
        }

        return pn;
    }

    private Object variables(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = nf.createVariables(ts.getLineno());
        boolean first = true;

        source.append((char)ts.VAR);

        for (;;) {
            Object name;
            Object init;
            mustMatchToken(ts, ts.NAME, "msg.bad.var");
            String s = ts.getString();

            if (!first)
                source.append((char)ts.COMMA);
            first = false;

            source.addString(ts.NAME, s);
            name = nf.createName(s);

            // omitted check for argument hiding

            if (ts.matchToken(ts.ASSIGN)) {
                if (ts.getOp() != ts.NOP)
                    reportError(ts, "msg.bad.var.init");

                source.append((char)ts.ASSIGN);
                source.append((char)ts.NOP);

                init = assignExpr(ts, source, inForInit);
                nf.addChildToBack(name, init);
            }
            nf.addChildToBack(pn, name);
            if (!ts.matchToken(ts.COMMA))
                break;
        }
        return pn;
    }

    private Object expr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = assignExpr(ts, source, inForInit);
        while (ts.matchToken(ts.COMMA)) {
            source.append((char)ts.COMMA);
            pn = nf.createBinary(ts.COMMA, pn, assignExpr(ts, source,
                                                          inForInit));
        }
        return pn;
    }

    private Object assignExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = condExpr(ts, source, inForInit);

        if (ts.matchToken(ts.ASSIGN)) {
            // omitted: "invalid assignment left-hand side" check.
            source.append((char)ts.ASSIGN);
            source.append((char)ts.getOp());
            pn = nf.createBinary(ts.ASSIGN, ts.getOp(), pn,
                                 assignExpr(ts, source, inForInit));
        }

        return pn;
    }

    private Object condExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object ifTrue;
        Object ifFalse;

        Object pn = orExpr(ts, source, inForInit);

        if (ts.matchToken(ts.HOOK)) {
            source.append((char)ts.HOOK);
            ifTrue = assignExpr(ts, source, false);
            mustMatchToken(ts, ts.COLON, "msg.no.colon.cond");
            source.append((char)ts.COLON);
            ifFalse = assignExpr(ts, source, inForInit);
            return nf.createTernary(pn, ifTrue, ifFalse);
        }

        return pn;
    }

    private Object orExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = andExpr(ts, source, inForInit);
        if (ts.matchToken(ts.OR)) {
            source.append((char)ts.OR);
            pn = nf.createBinary(ts.OR, pn, orExpr(ts, source, inForInit));
        }

        return pn;
    }

    private Object andExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = bitOrExpr(ts, source, inForInit);
        if (ts.matchToken(ts.AND)) {
            source.append((char)ts.AND);
            pn = nf.createBinary(ts.AND, pn, andExpr(ts, source, inForInit));
        }

        return pn;
    }

    private Object bitOrExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = bitXorExpr(ts, source, inForInit);
        while (ts.matchToken(ts.BITOR)) {
            source.append((char)ts.BITOR);
            pn = nf.createBinary(ts.BITOR, pn, bitXorExpr(ts, source,
                                                          inForInit));
        }
        return pn;
    }

    private Object bitXorExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = bitAndExpr(ts, source, inForInit);
        while (ts.matchToken(ts.BITXOR)) {
            source.append((char)ts.BITXOR);
            pn = nf.createBinary(ts.BITXOR, pn, bitAndExpr(ts, source,
                                                           inForInit));
        }
        return pn;
    }

    private Object bitAndExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = eqExpr(ts, source, inForInit);
        while (ts.matchToken(ts.BITAND)) {
            source.append((char)ts.BITAND);
            pn = nf.createBinary(ts.BITAND, pn, eqExpr(ts, source, inForInit));
        }
        return pn;
    }

    private Object eqExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = relExpr(ts, source, inForInit);
        while (ts.matchToken(ts.EQOP)) {
            source.append((char)ts.EQOP);
            source.append((char)ts.getOp());
            pn = nf.createBinary(ts.EQOP, ts.getOp(), pn,
                                 relExpr(ts, source, inForInit));
        }
        return pn;
    }

    private Object relExpr(TokenStream ts, Source source, boolean inForInit)
        throws IOException, JavaScriptException
    {
        Object pn = shiftExpr(ts, source);
        while (ts.matchToken(ts.RELOP)) {
            int op = ts.getOp();
            if (inForInit && op == ts.IN) {
                ts.ungetToken(ts.RELOP);
                break;
            }
            source.append((char)ts.RELOP);
            source.append((char)op);
            pn = nf.createBinary(ts.RELOP, op, pn,
                                 shiftExpr(ts, source));
        }
        return pn;
    }

    private Object shiftExpr(TokenStream ts, Source source)
        throws IOException, JavaScriptException
    {
        Object pn = addExpr(ts, source);
        while (ts.matchToken(ts.SHOP)) {
            source.append((char)ts.SHOP);
            source.append((char)ts.getOp());
            pn = nf.createBinary(ts.getOp(), pn, addExpr(ts, source));
        }
        return pn;
    }

    private Object addExpr(TokenStream ts, Source source)
        throws IOException, JavaScriptException
    {
        int tt;
        Object pn = mulExpr(ts, source);

        while ((tt = ts.getToken()) == ts.ADD || tt == ts.SUB) {
            source.append((char)tt);
            // flushNewLines
            pn = nf.createBinary(tt, pn, mulExpr(ts, source));
        }
        ts.ungetToken(tt);

        return pn;
    }

    private Object mulExpr(TokenStream ts, Source source)
        throws IOException, JavaScriptException
    {
        int tt;

        Object pn = unaryExpr(ts, source);

        while ((tt = ts.peekToken()) == ts.MUL ||
               tt == ts.DIV ||
               tt == ts.MOD) {
            tt = ts.getToken();
            source.append((char)tt);
            pn = nf.createBinary(tt, pn, unaryExpr(ts, source));
        }


        return pn;
    }

    private Object unaryExpr(TokenStream ts, Source source)
        throws IOException, JavaScriptException
    {
        int tt;

        ts.flags |= ts.TSF_REGEXP;
        tt = ts.getToken();
        ts.flags &= ~ts.TSF_REGEXP;

        switch(tt) {
        case TokenStream.UNARYOP:
            source.append((char)ts.UNARYOP);
            source.append((char)ts.getOp());
            return nf.createUnary(ts.UNARYOP, ts.getOp(),
                                  unaryExpr(ts, source));

        case TokenStream.ADD:
        case TokenStream.SUB:
            source.append((char)ts.UNARYOP);
            source.append((char)tt);
            return nf.createUnary(ts.UNARYOP, tt, unaryExpr(ts, source));

        case TokenStream.INC:
        case TokenStream.DEC:
            source.append((char)tt);
            return nf.createUnary(tt, ts.PRE, memberExpr(ts, source, true));

        case TokenStream.DELPROP:
            source.append((char)ts.DELPROP);
            return nf.createUnary(ts.DELPROP, unaryExpr(ts, source));

        case TokenStream.ERROR:
            break;

        default:
            ts.ungetToken(tt);

            int lineno = ts.getLineno();

            Object pn = memberExpr(ts, source, true);

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
                source.append((char)pf);
                return nf.createUnary(pf, ts.POST, pn);
            }
            return pn;
        }
        return nf.createName("err"); // Only reached on error.  Try to continue.
        
    }

    private Object argumentList(TokenStream ts, Source source, Object listNode)
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
                    source.append((char)ts.COMMA);
                first = false;
                nf.addChildToBack(listNode, assignExpr(ts, source, false));
            } while (ts.matchToken(ts.COMMA));
            
            mustMatchToken(ts, ts.RP, "msg.no.paren.arg");
        }
        source.append((char)ts.RP);
        return listNode;
    }

    private Object memberExpr(TokenStream ts, Source source,
                              boolean allowCallSyntax)
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
            source.append((char)ts.NEW);

            /* Make a NEW node to append to. */
            pn = nf.createLeaf(ts.NEW);
            nf.addChildToBack(pn, memberExpr(ts, source, false));

            if (ts.matchToken(ts.LP)) {
                source.append((char)ts.LP);
                /* Add the arguments to pn, if any are supplied. */
                pn = argumentList(ts, source, pn);
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
                nf.addChildToBack(pn, primaryExpr(ts, source));
            }
        } else {
            pn = primaryExpr(ts, source);
        }

        lastExprEndLine = ts.getLineno();
        while ((tt = ts.getToken()) > ts.EOF) {
            if (tt == ts.DOT) {
                source.append((char)ts.DOT);
                mustMatchToken(ts, ts.NAME, "msg.no.name.after.dot");
                String s = ts.getString();
                source.addString(ts.NAME, s);
                pn = nf.createBinary(ts.DOT, pn,
                                     nf.createName(ts.getString()));
                /* pn = nf.createBinary(ts.DOT, pn, memberExpr(ts))
                 * is the version in Brendan's IR C version.  Not in ECMA...
                 * does it reflect the 'new' operator syntax he mentioned?
                 */
                lastExprEndLine = ts.getLineno();
            } else if (tt == ts.LB) {
                source.append((char)ts.LB);
                pn = nf.createBinary(ts.LB, pn, expr(ts, source, false));

                mustMatchToken(ts, ts.RB, "msg.no.bracket.index");
                source.append((char)ts.RB);
                lastExprEndLine = ts.getLineno();
            } else if (allowCallSyntax && tt == ts.LP) {
                /* make a call node */

                pn = nf.createUnary(ts.CALL, pn);
                source.append((char)ts.LP);
                
                /* Add the arguments to pn, if any are supplied. */
                pn = argumentList(ts, source, pn);
                lastExprEndLine = ts.getLineno();
            } else {
                ts.ungetToken(tt);

                break;
            }
        }

        return pn;
    }

    private Object primaryExpr(TokenStream ts, Source source)
        throws IOException, JavaScriptException
    {
        int tt;

        Object pn;

        ts.flags |= ts.TSF_REGEXP;
        tt = ts.getToken();
        ts.flags &= ~ts.TSF_REGEXP;

        switch(tt) {

        case TokenStream.FUNCTION:
            return function(ts, source, true);

        case TokenStream.LB:
            {
                source.append((char)ts.LB);
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
                            source.append((char)ts.COMMA);
                        else
                            first = false;

                        if (tt == ts.RB) {  // to fix [,,,].length behavior...
                            break;
                        }

                        if (tt == ts.COMMA) {
                            nf.addChildToBack(pn, nf.createLeaf(ts.PRIMARY,
                                                                ts.UNDEFINED));
                        } else {
                            nf.addChildToBack(pn, assignExpr(ts, source, false));
                        }

                    } while (ts.matchToken(ts.COMMA));
                    mustMatchToken(ts, ts.RB, "msg.no.bracket.arg");
                }
                source.append((char)ts.RB);
                return nf.createArrayLiteral(pn);
            }

        case TokenStream.LC: {
            pn = nf.createLeaf(ts.OBJLIT);

            source.append((char)ts.LC);
            if (!ts.matchToken(ts.RC)) {

                boolean first = true;
            commaloop:
                do {
                    Object property;

                    if (!first)
                        source.append((char)ts.COMMA);
                    else
                        first = false;

                    tt = ts.getToken();
                    switch(tt) {
                        // map NAMEs to STRINGs in object literal context.
                    case TokenStream.NAME:
                    case TokenStream.STRING:
                        String s = ts.getString();
                        source.addString(ts.NAME, s);
                        property = nf.createString(ts.getString());
                        break;
                    case TokenStream.NUMBER:
                        double n = ts.getNumber();
                        source.addNumber(n);
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
                    source.append((char)ts.OBJLIT);
                    nf.addChildToBack(pn, property);
                    nf.addChildToBack(pn, assignExpr(ts, source, false));

                } while (ts.matchToken(ts.COMMA));

                mustMatchToken(ts, ts.RC, "msg.no.brace.prop");
            }
            source.append((char)ts.RC);
            return nf.createObjectLiteral(pn);
        }

        case TokenStream.LP:

            /* Brendan's IR-jsparse.c makes a new node tagged with
             * TOK_LP here... I'm not sure I understand why.  Isn't
             * the grouping already implicit in the structure of the
             * parse tree?  also TOK_LP is already overloaded (I
             * think) in the C IR as 'function call.'  */
            source.append((char)ts.LP);
            pn = expr(ts, source, false);
            source.append((char)ts.RP);
            mustMatchToken(ts, ts.RP, "msg.no.paren");
            return pn;

        case TokenStream.NAME:
            String name = ts.getString();
            source.addString(ts.NAME, name);
            return nf.createName(name);

        case TokenStream.NUMBER:
            double n = ts.getNumber();
            source.addNumber(n);
            return nf.createNumber(n);

        case TokenStream.STRING:
            String s = ts.getString();
            source.addString(ts.STRING, s);
            return nf.createString(s);

        case TokenStream.OBJECT:
        {
            String flags = ts.regExpFlags;
            ts.regExpFlags = null;
            String re = ts.getString();
            source.addString(ts.OBJECT, '/' + re + '/' + flags);
            return nf.createRegExp(re, flags);
        }

        case TokenStream.PRIMARY:
            source.append((char)ts.PRIMARY);
            source.append((char)ts.getOp());
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

    private int lastExprEndLine; // Hack to handle function expr termination.
    private IRFactory nf;
    private ErrorReporter er;
    private boolean ok; // Did the parse encounter an error?
}

/**
 * This class saves decompilation information about the source.
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

 * OPT source info collection is a potential performance bottleneck;
 * Source wraps a java.lang.StringBuffer, which is synchronized.  It
 * might be faster to implement Source with its own char buffer and
 * toString method.

 */

final class Source {
    Source() {
        // OPT the default 16 is probably too small, but it's not
        // clear just what size would work best for most javascript.
        // It'd be nice to know what characterizes the javascript
        // that's out there.
        buf = new StringBuffer(64);
    }

    void append(char c) {
        buf.append(c);
    }

    void addString(int type, String str) {
        buf.append((char)type);
        // java string length < 2^16?
        buf.append((char)str.length());
        buf.append(str);
    }

    void addNumber(double n) {
        buf.append((char)TokenStream.NUMBER);

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
            buf.append('D');
            lbits = Double.doubleToLongBits(n);

            buf.append((char)((lbits >> 48) & 0xFFFF));
            buf.append((char)((lbits >> 32) & 0xFFFF));
            buf.append((char)((lbits >> 16) & 0xFFFF));
            buf.append((char)(lbits & 0xFFFF));
        } else {
            // will it fit in a char?
            // (we can ignore negative values, bc they're already prefixed
            //  by UNARYOP SUB)
            // this gives a short encoding for integer values up to 2^16.
            if (lbits <= Character.MAX_VALUE) {
                buf.append('S');
                buf.append((char)lbits);
            } else { // Integral, but won't fit in a char. Store as a long.
                buf.append('J');
                buf.append((char)((lbits >> 48) & 0xFFFF));
                buf.append((char)((lbits >> 32) & 0xFFFF));
                buf.append((char)((lbits >> 16) & 0xFFFF));
                buf.append((char)(lbits & 0xFFFF));
            }
        }
    }

    char functionNumber;
    StringBuffer buf;
}

