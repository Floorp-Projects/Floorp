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
 * Ethan Hugg
 * Terry Lucas
 * Mike McCabe
 * Milen Nankov
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

import java.io.Reader;
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

public class Parser
{
    public Parser(CompilerEnvirons compilerEnv)
    {
        this.compilerEnv = compilerEnv;
    }

    protected Decompiler createDecompiler(CompilerEnvirons compilerEnv)
    {
        return new Decompiler();
    }


    /*
     * Build a parse tree from the given sourceString.
     *
     * @return an Object representing the parsed
     * program.  If the parse fails, null will be returned.  (The
     * parse failure will result in a call to the ErrorReporter from
     * CompilerEnvirons.)
     */
    public ScriptOrFnNode parse(String sourceString,
                                String sourceLocation, int lineno)
    {
        this.ts = new TokenStream(compilerEnv, null, sourceString,
                                  sourceLocation, lineno);
        try {
            return parse();
        } catch (IOException ex) {
            // Should never happen
            throw new IllegalStateException();
        }
    }

    /*
     * Build a parse tree from the given sourceString.
     *
     * @return an Object representing the parsed
     * program.  If the parse fails, null will be returned.  (The
     * parse failure will result in a call to the ErrorReporter from
     * CompilerEnvirons.)
     */
    public ScriptOrFnNode parse(Reader sourceReader,
                                String sourceLocation, int lineno)
        throws IOException
    {
        this.ts = new TokenStream(compilerEnv, sourceReader, null,
                                  sourceLocation, lineno);
        return parse();
    }

    private void mustHaveXML()
    {
        if (!compilerEnv.isXmlAvailable()) {
            reportError("msg.XML.not.available");
        }
    }

    private void mustMatchToken(int toMatch, String messageId)
        throws IOException, ParserException
    {
        if (!ts.matchToken(toMatch)) {
            reportError(messageId);
        }
    }

    void reportError(String messageId)
    {
        this.ok = false;
        ts.reportCurrentLineError(Context.getMessage0(messageId));

        // Throw a ParserException exception to unwind the recursive descent
        // parse.
        throw new ParserException();
    }

    private ScriptOrFnNode parse()
        throws IOException
    {
        this.decompiler = createDecompiler(compilerEnv);
        this.nf = new IRFactory(this);
        currentScriptOrFn = nf.createScript();
        this.decompiler = decompiler;
        int sourceStartOffset = decompiler.getCurrentOffset();
        this.encodedSource = null;
        decompiler.addToken(Token.SCRIPT);

        this.ok = true;

        int baseLineno = ts.getLineno();  // line number where source starts

        /* so we have something to add nodes to until
         * we've collected all the source */
        Node pn = nf.createLeaf(Token.BLOCK);

        try {
            for (;;) {
                ts.allowRegExp = true;
                int tt = ts.getToken();
                ts.allowRegExp = false;

                if (tt <= Token.EOF) {
                    break;
                }

                Node n;
                if (tt == Token.FUNCTION) {
                    try {
                        n = function(FunctionNode.FUNCTION_STATEMENT);
                    } catch (ParserException e) {
                        this.ok = false;
                        break;
                    }
                } else {
                    ts.ungetToken(tt);
                    n = statement();
                }
                nf.addChildToBack(pn, n);
            }
        } catch (StackOverflowError ex) {
            String msg = Context.getMessage0("mag.too.deep.parser.recursion");
            throw Context.reportRuntimeError(msg, ts.getSourceName(),
                                             ts.getLineno(), null, 0);
        }

        if (!this.ok) {
            // XXX ts.clearPushback() call here?
            return null;
        }

        currentScriptOrFn.setSourceName(ts.getSourceName());
        currentScriptOrFn.setBaseLineno(baseLineno);
        currentScriptOrFn.setEndLineno(ts.getLineno());

        int sourceEndOffset = decompiler.getCurrentOffset();
        currentScriptOrFn.setEncodedSourceBounds(sourceStartOffset,
                                                 sourceEndOffset);

        nf.initScript(currentScriptOrFn, pn);

        if (compilerEnv.isGeneratingSource()) {
            encodedSource = decompiler.getEncodedSource();
        }
        this.decompiler = null; // It helps GC

        return currentScriptOrFn;
    }

    public String getEncodedSource()
    {
        return encodedSource;
    }

    public boolean eof()
    {
        return ts.eof();
    }

    boolean insideFunction()
    {
        return nestingOfFunction != 0;
    }

    /*
     * The C version of this function takes an argument list,
     * which doesn't seem to be needed for tree generation...
     * it'd only be useful for checking argument hiding, which
     * I'm not doing anyway...
     */
    private Node parseFunctionBody()
        throws IOException
    {
        ++nestingOfFunction;
        Node pn = nf.createBlock(ts.getLineno());
        try {
            int tt;
            while((tt = ts.peekToken()) > Token.EOF && tt != Token.RC) {
                Node n;
                if (tt == Token.FUNCTION) {
                    ts.getToken();
                    n = function(FunctionNode.FUNCTION_STATEMENT);
                } else {
                    n = statement();
                }
                nf.addChildToBack(pn, n);
            }
        } catch (ParserException e) {
            this.ok = false;
        } finally {
            --nestingOfFunction;
        }

        return pn;
    }

    private Node function(int functionType)
        throws IOException, ParserException
    {
        int syntheticType = functionType;
        int baseLineno = ts.getLineno();  // line number where source starts

        int functionSourceStart = decompiler.markFunctionStart(functionType);
        String name;
        Node memberExprNode = null;
        if (ts.matchToken(Token.NAME)) {
            name = ts.getString();
            decompiler.addName(name);
            if (!ts.matchToken(Token.LP)) {
                if (compilerEnv.allowMemberExprAsFunctionName) {
                    // Extension to ECMA: if 'function <name>' does not follow
                    // by '(', assume <name> starts memberExpr
                    Node memberExprHead = nf.createName(name);
                    name = "";
                    memberExprNode = memberExprTail(false, memberExprHead);
                }
                mustMatchToken(Token.LP, "msg.no.paren.parms");
            }
        } else if (ts.matchToken(Token.LP)) {
            // Anonymous function
            name = "";
        } else {
            name = "";
            if (compilerEnv.allowMemberExprAsFunctionName) {
                // Note that memberExpr can not start with '(' like
                // in function (1+2).toString(), because 'function (' already
                // processed as anonymous function
                memberExprNode = memberExpr(false);
            }
            mustMatchToken(Token.LP, "msg.no.paren.parms");
        }

        if (memberExprNode != null) {
            syntheticType = FunctionNode.FUNCTION_EXPRESSION;
        }

        boolean nested = insideFunction();

        FunctionNode fnNode = nf.createFunction(name);
        if (nested) {
            // Nested functions must check their 'this' value to insure
            // it is not an activation object: see 10.1.6 Activation Object
            fnNode.setCheckThis();
        }
        if (nested || nestingOfWith > 0) {
            // 1. Nested functions are not affected by the dynamic scope flag
            // as dynamic scope is already a parent of their scope.
            // 2. Functions defined under the with statement also immune to
            // this setup, in which case dynamic scope is ignored in favor
            // of with object.
            fnNode.setIgnoreDynamicScope();
        }

        int functionIndex = currentScriptOrFn.addFunction(fnNode);

        int functionSourceEnd;

        ScriptOrFnNode savedScriptOrFn = currentScriptOrFn;
        currentScriptOrFn = fnNode;
        int savedNestingOfWith = nestingOfWith;
        nestingOfWith = 0;

        Node body;
        String source;
        try {
            decompiler.addToken(Token.LP);
            if (!ts.matchToken(Token.RP)) {
                boolean first = true;
                do {
                    if (!first)
                        decompiler.addToken(Token.COMMA);
                    first = false;
                    mustMatchToken(Token.NAME, "msg.no.parm");
                    String s = ts.getString();
                    if (fnNode.hasParamOrVar(s)) {
                        ts.reportCurrentLineWarning(Context.getMessage1(
                            "msg.dup.parms", s));
                    }
                    fnNode.addParam(s);
                    decompiler.addName(s);
                } while (ts.matchToken(Token.COMMA));

                mustMatchToken(Token.RP, "msg.no.paren.after.parms");
            }
            decompiler.addToken(Token.RP);

            mustMatchToken(Token.LC, "msg.no.brace.body");
            decompiler.addEOL(Token.LC);
            body = parseFunctionBody();
            mustMatchToken(Token.RC, "msg.no.brace.after.body");

            decompiler.addToken(Token.RC);
            functionSourceEnd = decompiler.markFunctionEnd(functionSourceStart);
            if (functionType != FunctionNode.FUNCTION_EXPRESSION) {
                checkWellTerminatedFunction();
                // Add EOL only if function is not part of expression
                // since it gets SEMI + EOL from Statement in that case
                decompiler.addToken(Token.EOL);
            }
        }
        finally {
            currentScriptOrFn = savedScriptOrFn;
            nestingOfWith = savedNestingOfWith;
        }

        fnNode.setEncodedSourceBounds(functionSourceStart, functionSourceEnd);
        fnNode.setSourceName(ts.getSourceName());
        fnNode.setBaseLineno(baseLineno);
        fnNode.setEndLineno(ts.getLineno());

        Node pn = nf.initFunction(fnNode, functionIndex, body, syntheticType);
        if (memberExprNode != null) {
            pn = nf.initFunction(fnNode, functionIndex, body, syntheticType);
            pn = nf.createAssignment(memberExprNode, pn);
            if (functionType != FunctionNode.FUNCTION_EXPRESSION) {
                // XXX check JScript behavior: should it be createExprStatement?
                pn = nf.createExprStatementNoReturn(pn, baseLineno);
            }
        }
        return pn;
    }

    private Node statements()
        throws IOException
    {
        Node pn = nf.createBlock(ts.getLineno());

        int tt;
        while((tt = ts.peekToken()) > Token.EOF && tt != Token.RC) {
            nf.addChildToBack(pn, statement());
        }

        return pn;
    }

    private Node condition()
        throws IOException, ParserException
    {
        Node pn;
        mustMatchToken(Token.LP, "msg.no.paren.cond");
        decompiler.addToken(Token.LP);
        pn = expr(false);
        mustMatchToken(Token.RP, "msg.no.paren.after.cond");
        decompiler.addToken(Token.RP);

        // there's a check here in jsparse.c that corrects = to ==

        return pn;
    }

    private void checkWellTerminated()
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
            if (compilerEnv.languageVersion < Context.VERSION_1_2) {
              /*
               * Checking against version < 1.2 and version >= 1.0
               * in the above line breaks old javascript, so we keep it
               * this way for now... XXX warning needed?
               */
                return;
            }
        }
        reportError("msg.no.semi.stmt");
    }

    private void checkWellTerminatedFunction()
        throws IOException, ParserException
    {
        if (compilerEnv.languageVersion < Context.VERSION_1_2) {
            // See comments in checkWellTerminated
             return;
        }
        checkWellTerminated();
    }

    // match a NAME; return null if no match.
    private String matchLabel()
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
            checkWellTerminated();

        return label;
    }

    private Node statement()
        throws IOException
    {
        try {
            return statementHelper();
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

    private Node statementHelper()
        throws IOException, ParserException
    {
        Node pn = null;

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
            Node cond = condition();
            decompiler.addEOL(Token.LC);
            Node ifTrue = statement();
            Node ifFalse = null;
            if (ts.matchToken(Token.ELSE)) {
                decompiler.addToken(Token.RC);
                decompiler.addToken(Token.ELSE);
                decompiler.addEOL(Token.LC);
                ifFalse = statement();
            }
            decompiler.addEOL(Token.RC);
            pn = nf.createIf(cond, ifTrue, ifFalse, lineno);
            break;
        }

        case Token.SWITCH: {
            skipsemi = true;

            decompiler.addToken(Token.SWITCH);
            pn = nf.createSwitch(ts.getLineno());

            Node cur_case = null;  // to kill warning
            Node case_statements;

            mustMatchToken(Token.LP, "msg.no.paren.switch");
            decompiler.addToken(Token.LP);
            nf.addChildToBack(pn, expr(false));
            mustMatchToken(Token.RP, "msg.no.paren.after.switch");
            decompiler.addToken(Token.RP);
            mustMatchToken(Token.LC, "msg.no.brace.switch");
            decompiler.addEOL(Token.LC);

            while ((tt = ts.getToken()) != Token.RC && tt != Token.EOF) {
                switch(tt) {
                case Token.CASE:
                    decompiler.addToken(Token.CASE);
                    cur_case = nf.createUnary(Token.CASE, expr(false));
                    decompiler.addEOL(Token.COLON);
                    break;

                case Token.DEFAULT:
                    cur_case = nf.createLeaf(Token.DEFAULT);
                    decompiler.addToken(Token.DEFAULT);
                    decompiler.addEOL(Token.COLON);
                    // XXX check that there isn't more than one default
                    break;

                default:
                    reportError("msg.bad.switch");
                    break;
                }
                mustMatchToken(Token.COLON, "msg.no.colon.case");

                case_statements = nf.createLeaf(Token.BLOCK);

                while ((tt = ts.peekToken()) != Token.RC && tt != Token.CASE &&
                        tt != Token.DEFAULT && tt != Token.EOF)
                {
                    nf.addChildToBack(case_statements, statement());
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
            Node cond = condition();
            decompiler.addEOL(Token.LC);
            Node body = statement();
            decompiler.addEOL(Token.RC);

            pn = nf.createWhile(cond, body, lineno);
            break;

        }

        case Token.DO: {
            decompiler.addToken(Token.DO);
            decompiler.addEOL(Token.LC);

            int lineno = ts.getLineno();

            Node body = statement();

            decompiler.addToken(Token.RC);
            mustMatchToken(Token.WHILE, "msg.no.while.do");
            decompiler.addToken(Token.WHILE);
            Node cond = condition();

            pn = nf.createDoWhile(body, cond, lineno);
            break;
        }

        case Token.FOR: {
            boolean isForEach = false;
            skipsemi = true;

            decompiler.addToken(Token.FOR);
            int lineno = ts.getLineno();

            Node init;  // Node init is also foo in 'foo in Object'
            Node cond;  // Node cond is also object in 'foo in Object'
            Node incr = null; // to kill warning
            Node body;

            // See if this is a for each () instead of just a for ()
            if (ts.matchToken(Token.NAME)) {
                decompiler.addName(ts.getString());
                if (ts.getString().equals("each")) {
                    isForEach = true;
                } else {
                    reportError("msg.no.paren.for");
                }
            }

            mustMatchToken(Token.LP, "msg.no.paren.for");
            decompiler.addToken(Token.LP);
            tt = ts.peekToken();
            if (tt == Token.SEMI) {
                init = nf.createLeaf(Token.EMPTY);
            } else {
                if (tt == Token.VAR) {
                    // set init to a var list or initial
                    ts.getToken();    // throw away the 'var' token
                    init = variables(true);
                }
                else {
                    init = expr(true);
                }
            }

            if (ts.matchToken(Token.IN)) {
                decompiler.addToken(Token.IN);
                // 'cond' is the object over which we're iterating
                cond = expr(false);
            } else {  // ordinary for loop
                mustMatchToken(Token.SEMI, "msg.no.semi.for");
                decompiler.addToken(Token.SEMI);
                if (ts.peekToken() == Token.SEMI) {
                    // no loop condition
                    cond = nf.createLeaf(Token.EMPTY);
                } else {
                    cond = expr(false);
                }

                mustMatchToken(Token.SEMI, "msg.no.semi.for.cond");
                decompiler.addToken(Token.SEMI);
                if (ts.peekToken() == Token.RP) {
                    incr = nf.createLeaf(Token.EMPTY);
                } else {
                    incr = expr(false);
                }
            }

            mustMatchToken(Token.RP, "msg.no.paren.for.ctrl");
            decompiler.addToken(Token.RP);
            decompiler.addEOL(Token.LC);
            body = statement();
            decompiler.addEOL(Token.RC);

            if (incr == null) {
                // cond could be null if 'in obj' got eaten by the init node.
                pn = nf.createForIn(init, cond, body, isForEach, lineno);
            } else {
                pn = nf.createFor(init, cond, incr, body, lineno);
            }
            break;
        }

        case Token.TRY: {
            int lineno = ts.getLineno();

            Node tryblock;
            Node catchblocks = null;
            Node finallyblock = null;

            skipsemi = true;
            decompiler.addToken(Token.TRY);
            decompiler.addEOL(Token.LC);
            tryblock = statement();
            decompiler.addEOL(Token.RC);

            catchblocks = nf.createLeaf(Token.BLOCK);

            boolean sawDefaultCatch = false;
            int peek = ts.peekToken();
            if (peek == Token.CATCH) {
                while (ts.matchToken(Token.CATCH)) {
                    if (sawDefaultCatch) {
                        reportError("msg.catch.unreachable");
                    }
                    decompiler.addToken(Token.CATCH);
                    mustMatchToken(Token.LP, "msg.no.paren.catch");
                    decompiler.addToken(Token.LP);

                    mustMatchToken(Token.NAME, "msg.bad.catchcond");
                    String varName = ts.getString();
                    decompiler.addName(varName);

                    Node catchCond = null;
                    if (ts.matchToken(Token.IF)) {
                        decompiler.addToken(Token.IF);
                        catchCond = expr(false);
                    } else {
                        sawDefaultCatch = true;
                    }

                    mustMatchToken(Token.RP, "msg.bad.catchcond");
                    decompiler.addToken(Token.RP);
                    mustMatchToken(Token.LC, "msg.no.brace.catchblock");
                    decompiler.addEOL(Token.LC);

                    nf.addChildToBack(catchblocks,
                        nf.createCatch(varName, catchCond,
                                       statements(),
                                       ts.getLineno()));

                    mustMatchToken(Token.RC, "msg.no.brace.after.body");
                    decompiler.addEOL(Token.RC);
                }
            } else if (peek != Token.FINALLY) {
                mustMatchToken(Token.FINALLY, "msg.try.no.catchfinally");
            }

            if (ts.matchToken(Token.FINALLY)) {
                decompiler.addToken(Token.FINALLY);
                decompiler.addEOL(Token.LC);
                finallyblock = statement();
                decompiler.addEOL(Token.RC);
            }

            pn = nf.createTryCatchFinally(tryblock, catchblocks,
                                          finallyblock, lineno);

            break;
        }
        case Token.THROW: {
            int lineno = ts.getLineno();
            decompiler.addToken(Token.THROW);
            pn = nf.createThrow(expr(false), lineno);
            if (lineno == ts.getLineno())
                checkWellTerminated();
            break;
        }
        case Token.BREAK: {
            int lineno = ts.getLineno();

            decompiler.addToken(Token.BREAK);

            // matchLabel only matches if there is one
            String label = matchLabel();
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
            String label = matchLabel();
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
            mustMatchToken(Token.LP, "msg.no.paren.with");
            decompiler.addToken(Token.LP);
            Node obj = expr(false);
            mustMatchToken(Token.RP, "msg.no.paren.after.with");
            decompiler.addToken(Token.RP);
            decompiler.addEOL(Token.LC);

            ++nestingOfWith;
            Node body;
            try {
                body = statement();
            } finally {
                --nestingOfWith;
            }

            decompiler.addEOL(Token.RC);

            pn = nf.createWith(obj, body, lineno);
            break;
        }
        case Token.VAR: {
            int lineno = ts.getLineno();
            pn = variables(false);
            if (ts.getLineno() == lineno)
                checkWellTerminated();
            break;
        }
        case Token.RETURN: {
            Node retExpr = null;

            decompiler.addToken(Token.RETURN);

            if (!insideFunction())
                reportError("msg.bad.return");

            /* This is ugly, but we don't want to require a semicolon. */
            ts.allowRegExp = true;
            tt = ts.peekTokenSameLine();
            ts.allowRegExp = false;

            int lineno = ts.getLineno();
            if (tt != Token.EOF && tt != Token.EOL && tt != Token.SEMI && tt != Token.RC) {
                retExpr = expr(false);
                if (ts.getLineno() == lineno)
                    checkWellTerminated();
            }

            // XXX ASSERT pn
            pn = nf.createReturn(retExpr, lineno);
            break;
        }
        case Token.LC:
            skipsemi = true;

            pn = statements();
            mustMatchToken(Token.RC, "msg.no.brace.block");
            break;

        case Token.ERROR:
            // Fall thru, to have a node for error recovery to work on
        case Token.EOL:
        case Token.SEMI:
            pn = nf.createLeaf(Token.EMPTY);
            skipsemi = true;
            break;

        case Token.FUNCTION: {
            pn = function(FunctionNode.FUNCTION_EXPRESSION_STATEMENT);
            break;
        }

        case Token.DEFAULT :
            mustHaveXML();
            int nsLine = ts.getLineno();

            if (!(ts.matchToken(Token.NAME)
                  && ts.getString().equals("xml")))
            {
                reportError("msg.bad.namespace");
            }
            decompiler.addName(ts.getString());

            if (!(ts.matchToken(Token.NAME)
                  && ts.getString().equals("namespace")))
            {
                reportError("msg.bad.namespace");
            }
            decompiler.addName(ts.getString());

            if (!ts.matchToken(Token.ASSIGN)) {
                reportError("msg.bad.namespace");
            }
            decompiler.addToken(Token.ASSIGN);

            Node expr = expr(false);
            pn = nf.createDefaultNamespace(expr, nsLine);
            break;

        default: {
                int lastExprType = tt;
                int tokenno = ts.getTokenno();
                ts.ungetToken(tt);
                int lineno = ts.getLineno();

                pn = expr(false);

                if (ts.peekToken() == Token.COLON) {
                    /* check that the last thing the tokenizer returned was a
                     * NAME and that only one token was consumed.
                     */
                    if (lastExprType != Token.NAME || (ts.getTokenno() != tokenno))
                        reportError("msg.bad.label");

                    ts.getToken();  // eat the COLON

                    /* in the C source, the label is associated with the
                     * statement that follows:
                     *                nf.addChildToBack(pn, statement());
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
                    checkWellTerminated();
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

    private Node variables(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = nf.createVariables(ts.getLineno());
        boolean first = true;

        decompiler.addToken(Token.VAR);

        for (;;) {
            Node name;
            Node init;
            mustMatchToken(Token.NAME, "msg.bad.var");
            String s = ts.getString();

            if (!first)
                decompiler.addToken(Token.COMMA);
            first = false;

            decompiler.addName(s);
            currentScriptOrFn.addVar(s);
            name = nf.createName(s);

            // omitted check for argument hiding

            if (ts.matchToken(Token.ASSIGN)) {
                decompiler.addToken(Token.ASSIGN);

                init = assignExpr(inForInit);
                nf.addChildToBack(name, init);
            }
            nf.addChildToBack(pn, name);
            if (!ts.matchToken(Token.COMMA))
                break;
        }
        return pn;
    }

    private Node expr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = assignExpr(inForInit);
        while (ts.matchToken(Token.COMMA)) {
            decompiler.addToken(Token.COMMA);
            pn = nf.createBinary(Token.COMMA, pn, assignExpr(inForInit));
        }
        return pn;
    }

    private Node assignExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = condExpr(inForInit);

        int tt = ts.peekToken();
        // omitted: "invalid assignment left-hand side" check.
        if (tt == Token.ASSIGN) {
            ts.getToken();
            decompiler.addToken(Token.ASSIGN);
            pn = nf.createAssignment(pn, assignExpr(inForInit));
        } else if (tt == Token.ASSIGNOP) {
            ts.getToken();
            int op = ts.getOp();
            decompiler.addAssignOp(op);
            pn = nf.createAssignmentOp(op, pn, assignExpr(inForInit));
        }

        return pn;
    }

    private Node condExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node ifTrue;
        Node ifFalse;

        Node pn = orExpr(inForInit);

        if (ts.matchToken(Token.HOOK)) {
            decompiler.addToken(Token.HOOK);
            ifTrue = assignExpr(false);
            mustMatchToken(Token.COLON, "msg.no.colon.cond");
            decompiler.addToken(Token.COLON);
            ifFalse = assignExpr(inForInit);
            return nf.createCondExpr(pn, ifTrue, ifFalse);
        }

        return pn;
    }

    private Node orExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = andExpr(inForInit);
        if (ts.matchToken(Token.OR)) {
            decompiler.addToken(Token.OR);
            pn = nf.createBinary(Token.OR, pn, orExpr(inForInit));
        }

        return pn;
    }

    private Node andExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = bitOrExpr(inForInit);
        if (ts.matchToken(Token.AND)) {
            decompiler.addToken(Token.AND);
            pn = nf.createBinary(Token.AND, pn, andExpr(inForInit));
        }

        return pn;
    }

    private Node bitOrExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = bitXorExpr(inForInit);
        while (ts.matchToken(Token.BITOR)) {
            decompiler.addToken(Token.BITOR);
            pn = nf.createBinary(Token.BITOR, pn, bitXorExpr(inForInit));
        }
        return pn;
    }

    private Node bitXorExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = bitAndExpr(inForInit);
        while (ts.matchToken(Token.BITXOR)) {
            decompiler.addToken(Token.BITXOR);
            pn = nf.createBinary(Token.BITXOR, pn, bitAndExpr(inForInit));
        }
        return pn;
    }

    private Node bitAndExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = eqExpr(inForInit);
        while (ts.matchToken(Token.BITAND)) {
            decompiler.addToken(Token.BITAND);
            pn = nf.createBinary(Token.BITAND, pn, eqExpr(inForInit));
        }
        return pn;
    }

    private Node eqExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = relExpr(inForInit);
        for (;;) {
            int tt = ts.peekToken();
            switch (tt) {
              case Token.EQ:
              case Token.NE:
              case Token.SHEQ:
              case Token.SHNE:
                ts.getToken();
                int decompilerToken = tt;
                int parseToken = tt;
                if (compilerEnv.languageVersion == Context.VERSION_1_2) {
                    // JavaScript 1.2 uses shallow equality for == and != .
                    // In addition, convert === and !== for decompiler into
                    // == and != since the decompiler is supposed to show
                    // canonical source and in 1.2 ===, !== are allowed
                    // only as an alias to ==, !=.
                    switch (tt) {
                      case Token.EQ:
                        parseToken = Token.SHEQ;
                        break;
                      case Token.NE:
                        parseToken = Token.SHNE;
                        break;
                      case Token.SHEQ:
                        decompilerToken = Token.EQ;
                        break;
                      case Token.SHNE:
                        decompilerToken = Token.NE;
                        break;
                    }
                }
                decompiler.addToken(decompilerToken);
                pn = nf.createBinary(parseToken, pn, relExpr(inForInit));
                continue;
            }
            break;
        }
        return pn;
    }

    private Node relExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = shiftExpr();
        for (;;) {
            int tt = ts.peekToken();
            switch (tt) {
              case Token.IN:
                if (inForInit)
                    break;
                // fall through
              case Token.INSTANCEOF:
              case Token.LE:
              case Token.LT:
              case Token.GE:
              case Token.GT:
                ts.getToken();
                decompiler.addToken(tt);
                pn = nf.createBinary(tt, pn, shiftExpr());
                continue;
            }
            break;
        }
        return pn;
    }

    private Node shiftExpr()
        throws IOException, ParserException
    {
        Node pn = addExpr();
        for (;;) {
            int tt = ts.peekToken();
            switch (tt) {
              case Token.LSH:
              case Token.URSH:
              case Token.RSH:
                ts.getToken();
                decompiler.addToken(tt);
                pn = nf.createBinary(tt, pn, addExpr());
                continue;
            }
            break;
        }
        return pn;
    }

    private Node addExpr()
        throws IOException, ParserException
    {
        Node pn = mulExpr();
        for (;;) {
            int tt = ts.peekToken();
            if (tt == Token.ADD || tt == Token.SUB) {
                ts.getToken();
                decompiler.addToken(tt);
                // flushNewLines
                pn = nf.createBinary(tt, pn, mulExpr());
                continue;
            }
            break;
        }

        return pn;
    }

    private Node mulExpr()
        throws IOException, ParserException
    {
        Node pn = unaryExpr();
        for (;;) {
            int tt = ts.peekToken();
            switch (tt) {
              case Token.MUL:
              case Token.DIV:
              case Token.MOD:
                ts.getToken();
                decompiler.addToken(tt);
                pn = nf.createBinary(tt, pn, unaryExpr());
                continue;
            }
            break;
        }

        return pn;
    }

    private Node unaryExpr()
        throws IOException, ParserException
    {
        int tt;

        ts.allowRegExp = true;
        tt = ts.getToken();
        ts.allowRegExp = false;

        switch(tt) {
        case Token.VOID:
        case Token.NOT:
        case Token.BITNOT:
        case Token.TYPEOF:
            decompiler.addToken(tt);
            return nf.createUnary(tt, unaryExpr());

        case Token.ADD:
            // Convert to special POS token in decompiler and parse tree
            decompiler.addToken(Token.POS);
            return nf.createUnary(Token.POS, unaryExpr());

        case Token.SUB:
            // Convert to special NEG token in decompiler and parse tree
            decompiler.addToken(Token.NEG);
            return nf.createUnary(Token.NEG, unaryExpr());

        case Token.INC:
        case Token.DEC:
            decompiler.addToken(tt);
            return nf.createIncDec(tt, false, memberExpr(true));

        case Token.DELPROP:
            decompiler.addToken(Token.DELPROP);
            return nf.createUnary(Token.DELPROP, unaryExpr());

        case Token.ERROR:
            break;

        // XML stream encountered in expression.
        case Token.LT:
            if (compilerEnv.isXmlAvailable()) {
                Node pn = xmlInitializer();
                return memberExprTail(true, pn);
            }
            // Fall thru to the default handling of RELOP

        default:
            ts.ungetToken(tt);

            int lineno = ts.getLineno();

            Node pn = memberExpr(true);

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
                return nf.createIncDec(pf, true, pn);
            }
            return pn;
        }
        return nf.createName("err"); // Only reached on error.  Try to continue.

    }

    private Node xmlInitializer() throws IOException
    {
        int tt = ts.getFirstXMLToken();
        if (tt != Token.XML && tt != Token.XMLEND) {
            reportError("msg.syntax");
            return null;
        }

        /* Make a NEW node to append to. */
        Node pnXML = nf.createLeaf(Token.NEW);
        decompiler.addToken(Token.NEW);
        decompiler.addToken(Token.DOT);

        String xml = ts.getString();
        boolean fAnonymous = xml.trim().startsWith("<>");

        decompiler.addName(fAnonymous ? "XMLList" : "XML");
        Node pn = nf.createName(fAnonymous ? "XMLList" : "XML");
        nf.addChildToBack(pnXML, pn);

        pn = null;
        Node expr;
        for (;;tt = ts.getNextXMLToken()) {
            switch (tt) {
            case Token.XML:
                xml = ts.getString();
                decompiler.addString(xml);
                mustMatchToken(Token.LC, "msg.syntax");
                decompiler.addToken(Token.LC);
                expr = (ts.peekToken() == Token.RC)
                    ? nf.createString("")
                    : expr(false);
                mustMatchToken(Token.RC, "msg.syntax");
                decompiler.addToken(Token.RC);
                if (pn == null) {
                    pn = nf.createString(xml);
                } else {
                    pn = nf.createBinary(Token.ADD, pn, nf.createString(xml));
                }
                if (ts.isXMLAttribute()) {
                    pn = nf.createBinary(Token.ADD, pn, nf.createString("\""));
                    expr = nf.createUnary(Token.ESCXMLATTR, expr);
                    pn = nf.createBinary(Token.ADD, pn, expr);
                    pn = nf.createBinary(Token.ADD, pn, nf.createString("\""));
                } else {
                    expr = nf.createUnary(Token.ESCXMLTEXT, expr);
                    pn = nf.createBinary(Token.ADD, pn, expr);
                }
                break;
            case Token.XMLEND:
                xml = ts.getString();
                decompiler.addString(xml);
                if (pn == null) {
                    pn = nf.createString(xml);
                } else {
                    pn = nf.createBinary(Token.ADD, pn, nf.createString(xml));
                }

                nf.addChildToBack(pnXML, pn);
                return pnXML;
            default:
                ts.ungetToken(tt);
                reportError("msg.syntax");
                return null;
            }
        }
    }

    private void argumentList(Node listNode)
        throws IOException, ParserException
    {
        boolean matched;
        ts.allowRegExp = true;
        matched = ts.matchToken(Token.RP);
        ts.allowRegExp = false;
        if (!matched) {
            boolean first = true;
            do {
                if (!first)
                    decompiler.addToken(Token.COMMA);
                first = false;
                nf.addChildToBack(listNode, assignExpr(false));
            } while (ts.matchToken(Token.COMMA));

            mustMatchToken(Token.RP, "msg.no.paren.arg");
        }
        decompiler.addToken(Token.RP);
    }

    private Node memberExpr(boolean allowCallSyntax)
        throws IOException, ParserException
    {
        int tt;

        Node pn;

        /* Check for new expressions. */
        ts.allowRegExp = true;
        tt = ts.peekToken();
        ts.allowRegExp = false;
        if (tt == Token.NEW) {
            /* Eat the NEW token. */
            ts.getToken();
            decompiler.addToken(Token.NEW);

            /* Make a NEW node to append to. */
            pn = nf.createCallOrNew(Token.NEW, memberExpr(false));

            if (ts.matchToken(Token.LP)) {
                decompiler.addToken(Token.LP);
                /* Add the arguments to pn, if any are supplied. */
                argumentList(pn);
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
                nf.addChildToBack(pn, primaryExpr());
            }
        } else {
            pn = primaryExpr();
        }

        return memberExprTail(allowCallSyntax, pn);
    }

    private Node memberExprTail(boolean allowCallSyntax, Node pn)
        throws IOException, ParserException
    {
        int tt;
      tailLoop:
        while ((tt = ts.getToken()) != Token.EOF) {
            switch (tt) {
              case Token.DOT: {
                decompiler.addToken(Token.DOT);
                Node n;
                if (compilerEnv.isXmlAvailable()) {
                    n = nameOrPropertyIdentifier();
                } else {
                    mustMatchToken(Token.NAME, "msg.no.name.after.dot");
                    String s = ts.getString();
                    decompiler.addName(s);
                    n = nf.createName(s);
                }
                pn = nf.createBinary(Token.DOT, pn, n);
                break;
              }

              case Token.DOTDOT: {
                mustHaveXML();
                decompiler.addToken(Token.DOTDOT);
                Node n = nameOrPropertyIdentifier();
                pn = nf.createBinary(Token.DOTDOT, pn, n);
                break;
              }

              case Token.DOTQUERY:
                mustHaveXML();
                decompiler.addToken(Token.DOTQUERY);
                pn = nf.createDotQuery(pn, expr(false), ts.getLineno());
                mustMatchToken(Token.RP, "msg.no.paren");
                break;

              case Token.LB:
                decompiler.addToken(Token.LB);
                pn = nf.createBinary(Token.LB, pn, expr(false));
                mustMatchToken(Token.RB, "msg.no.bracket.index");
                decompiler.addToken(Token.RB);
                break;

              case Token.LP:
                if (!allowCallSyntax) {
                    ts.ungetToken(tt);
                    break tailLoop;
                }
                decompiler.addToken(Token.LP);
                pn = nf.createCallOrNew(Token.CALL, pn);
                /* Add the arguments to pn, if any are supplied. */
                argumentList(pn);
                break;

              default:
                ts.ungetToken(tt);
                break tailLoop;
            }
        }
        return pn;
    }

    private Node nameOrPropertyIdentifier()
        throws IOException, ParserException
    {
        Node pn;
        int tt = ts.getToken();

        switch (tt) {
          // handles: name, ns::name, ns::*, ns::[expr]
          case Token.NAME: {
            String s = ts.getString();
            decompiler.addName(s);
            pn = nameOrQualifiedName(s, false);
            break;
          }

          // handles: *, *::name, *::*, *::[expr]
          case Token.MUL:
            decompiler.addName("*");
            pn = nameOrQualifiedName("*", false);
            break;

          // handles: '@attr', '@ns::attr', '@ns::*', '@ns::*',
          //          '@::attr', '@::*', '@*', '@*::attr', '@*::*'
          case Token.XMLATTR:
            decompiler.addToken(Token.XMLATTR);
            pn = attributeIdentifier();
            break;

          default:
            reportError("msg.no.name.after.dot");
            pn = nf.createName("?");
        }

        return pn;
    }

    /*
     * Xml attribute expression:
     *   '@attr', '@ns::attr', '@ns::*', '@ns::*', '@*', '@*::attr', '@*::*'
     */
    private Node attributeIdentifier()
        throws IOException
    {
        Node pn;
        int tt = ts.getToken();

        switch (tt) {
          // handles: @name, @ns::name, @ns::*, @ns::[expr]
          case Token.NAME: {
            String s = ts.getString();
            decompiler.addName(s);
            pn = nf.createAttributeName(nameOrQualifiedName(s, false));
            break;
          }

          // handles: @*, @*::name, @*::*, @*::[expr]
          case Token.MUL:
            decompiler.addName("*");
            pn = nf.createAttributeName(nameOrQualifiedName("*", false));
            break;

          // handles @[expr]
          case Token.LB:
            decompiler.addToken(Token.LB);
            pn = nf.createAttributeExpr(expr(false));
            mustMatchToken(Token.RB, "msg.no.bracket.index");
            decompiler.addToken(Token.RB);
            break;

          default:
            reportError("msg.no.name.after.xmlAttr");
            pn = nf.createAttributeExpr(nf.createString("?"));
            break;
        }

        return pn;
    }

    /**
     * Check if :: follows name in which case it becomes qualified name
     */
    private Node nameOrQualifiedName(String name, boolean primaryContext)
        throws IOException, ParserException
    {
      colonColonCheck:
        if (ts.matchToken(Token.COLONCOLON)) {
            decompiler.addToken(Token.COLONCOLON);

            Node pn;
            int tt = ts.getToken();

            switch (tt) {
              // handles name::name
              case Token.NAME: {
                String s = ts.getString();
                decompiler.addName(s);
                pn = nf.createQualifiedName(name, s);
                break;
              }

              // handles name::*
              case Token.MUL:
                decompiler.addName("*");
                pn = nf.createQualifiedName(name, "*");
                break;

              // handles name::[expr]
              case Token.LB:
                decompiler.addToken(Token.LB);
                pn = nf.createQualifiedExpr(name, expr(false));
                mustMatchToken(Token.RB, "msg.no.bracket.index");
                decompiler.addToken(Token.RB);
                break;

              default:
                reportError("msg.no.name.after.coloncolon");
                break colonColonCheck;
            }

            if (primaryContext) {
                pn = nf.createXMLPrimary(pn);
            }
            return pn;
        }

        return nf.createName(name);
    }

    private Node primaryExpr()
        throws IOException, ParserException
    {
        int tt;

        Node pn;

        ts.allowRegExp = true;
        tt = ts.getToken();
        ts.allowRegExp = false;

        switch(tt) {

        case Token.FUNCTION:
            return function(FunctionNode.FUNCTION_EXPRESSION);

        case Token.LB:
            {
                ObjArray elems = new ObjArray();
                int skipCount = 0;
                decompiler.addToken(Token.LB);
                boolean after_lb_or_comma = true;
                for (;;) {
                    ts.allowRegExp = true;
                    tt = ts.peekToken();
                    ts.allowRegExp = false;

                    if (tt == Token.COMMA) {
                        ts.getToken();
                        decompiler.addToken(Token.COMMA);
                        if (!after_lb_or_comma) {
                            after_lb_or_comma = true;
                        } else {
                            elems.add(null);
                            ++skipCount;
                        }
                    } else if (tt == Token.RB) {
                        ts.getToken();
                        decompiler.addToken(Token.RB);
                        break;
                    } else {
                        if (!after_lb_or_comma) {
                            reportError("msg.no.bracket.arg");
                        }
                        elems.add(assignExpr(false));
                        after_lb_or_comma = false;
                    }
                }
                return nf.createArrayLiteral(elems, skipCount);
            }

        case Token.LC: {
            ObjArray elems = new ObjArray();
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
                    case Token.NAME:
                    case Token.STRING:
                        // map NAMEs to STRINGs in object literal context
                        // but tell the decompiler the proper type
                        String s = ts.getString();
                        if (tt == Token.NAME) {
                            decompiler.addName(s);
                        } else {
                            decompiler.addString(s);
                        }
                        property = ScriptRuntime.getIndexObject(s);
                        break;
                    case Token.NUMBER:
                        double n = ts.getNumber();
                        decompiler.addNumber(n);
                        property = ScriptRuntime.getIndexObject(n);
                        break;
                    case Token.RC:
                        // trailing comma is OK.
                        ts.ungetToken(tt);
                        break commaloop;
                    default:
                        reportError("msg.bad.prop");
                        break commaloop;
                    }
                    mustMatchToken(Token.COLON, "msg.no.colon.prop");

                    // OBJLIT is used as ':' in object literal for
                    // decompilation to solve spacing ambiguity.
                    decompiler.addToken(Token.OBJECTLIT);
                    elems.add(property);
                    elems.add(assignExpr(false));
                } while (ts.matchToken(Token.COMMA));

                mustMatchToken(Token.RC, "msg.no.brace.prop");
            }
            decompiler.addToken(Token.RC);
            return nf.createObjectLiteral(elems);
        }

        case Token.LP:

            /* Brendan's IR-jsparse.c makes a new node tagged with
             * TOK_LP here... I'm not sure I understand why.  Isn't
             * the grouping already implicit in the structure of the
             * parse tree?  also TOK_LP is already overloaded (I
             * think) in the C IR as 'function call.'  */
            decompiler.addToken(Token.LP);
            pn = expr(false);
            decompiler.addToken(Token.RP);
            mustMatchToken(Token.RP, "msg.no.paren");
            return pn;

        case Token.XMLATTR:
            mustHaveXML();
            decompiler.addToken(Token.XMLATTR);
            pn = attributeIdentifier();
            return nf.createXMLPrimary(pn);

        case Token.NAME: {
            String name = ts.getString();
            decompiler.addName(name);
            if (compilerEnv.isXmlAvailable()) {
                pn = nameOrQualifiedName(name, true);
            } else {
                pn = nf.createName(name);
            }
            return pn;
        }

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

        case Token.NULL:
        case Token.THIS:
        case Token.FALSE:
        case Token.TRUE:
            decompiler.addToken(tt);
            return nf.createLeaf(tt);

        case Token.RESERVED:
            reportError("msg.reserved.id");
            break;

        case Token.ERROR:
            /* the scanner or one of its subroutines reported the error. */
            break;

        default:
            reportError("msg.syntax");
            break;

        }
        return null;    // should never reach here
    }

    CompilerEnvirons compilerEnv;
    private TokenStream ts;

    private IRFactory nf;

    private boolean ok; // Did the parse encounter an error?

    ScriptOrFnNode currentScriptOrFn;

    private int nestingOfFunction;
    private int nestingOfWith;

    private Decompiler decompiler;
    private String encodedSource;

}

// Exception to unwind
class ParserException extends RuntimeException { }
