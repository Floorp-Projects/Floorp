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
import java.util.Hashtable;

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
    // TokenInformation flags : currentFlaggedToken stores them together
    // with token type
    final static int
        CLEAR_TI_MASK  = 0xFFFF,   // mask to clear token information bits
        TI_AFTER_EOL   = 1 << 16,  // first token of the source line
        TI_CHECK_LABEL = 1 << 17;  // indicates to check for label

    CompilerEnvirons compilerEnv;
    private ErrorReporter errorReporter;
    private String sourceURI;

    private TokenStream ts;
    private int currentFlaggedToken;
    private int syntaxErrorCount;

    private IRFactory nf;

    private int nestingOfFunction;

    private Decompiler decompiler;
    private String encodedSource;

// The following are per function variables and should be saved/restored
// during function parsing.
// XXX Move to separated class?
    ScriptOrFnNode currentScriptOrFn;
    private int nestingOfWith;
    private Hashtable labelSet; // map of label names into nodes
    private ObjArray loopSet;
    private ObjArray loopAndSwitchSet;
// end of per function variables

    // Exception to unwind
    private static class ParserException extends RuntimeException
    {
    }

    public Parser(CompilerEnvirons compilerEnv, ErrorReporter errorReporter)
    {
        this.compilerEnv = compilerEnv;
        this.errorReporter = errorReporter;
    }

    protected Decompiler createDecompiler(CompilerEnvirons compilerEnv)
    {
        return new Decompiler();
    }

    void addWarning(String messageId, String messageArg)
    {
        String message = Context.getMessage1(messageId, messageArg);
        errorReporter.warning(message, sourceURI, ts.getLineno(),
                              ts.getLine(), ts.getOffset());
    }

    void addError(String messageId)
    {
        ++syntaxErrorCount;
        String message = Context.getMessage0(messageId);
        errorReporter.error(message, sourceURI, ts.getLineno(),
                            ts.getLine(), ts.getOffset());
    }

    void reportError(String messageId)
    {
        addError(messageId);

        // Throw a ParserException exception to unwind the recursive descent
        // parse.
        throw new ParserException();
    }

    private int peekToken(int flagsTS)
        throws IOException
    {
        int tt = currentFlaggedToken;
        if (tt == Token.EOF) {
            tt = ts.getToken(flagsTS);
            if (tt == Token.EOL) {
                do {
                    tt = ts.getToken(flagsTS);
                } while (tt == Token.EOL);
                tt |= TI_AFTER_EOL;
            }
            currentFlaggedToken = tt;
        }
        return tt & CLEAR_TI_MASK;
    }

    private int peekFlaggedToken(int flagsTS)
        throws IOException
    {
        peekToken(flagsTS);
        return currentFlaggedToken;
    }

    private void consumeToken()
    {
        currentFlaggedToken = Token.EOF;
    }

    private int nextToken(int flagsTS)
        throws IOException
    {
        int tt = peekToken(flagsTS);
        consumeToken();
        return tt;
    }

    private int nextFlaggedToken(int flagsTS)
        throws IOException
    {
        peekToken(flagsTS);
        int ttFlagged = currentFlaggedToken;
        consumeToken();
        return ttFlagged;
    }

    private boolean matchToken(int toMatch, int flagsTS)
        throws IOException
    {
        int tt = peekToken(flagsTS);
        if (tt != toMatch) {
            return false;
        }
        consumeToken();
        return true;
    }

    private int peekTokenOrEOL(int flagsTS)
        throws IOException
    {
        int tt = peekToken(flagsTS);
        // Check for last peeked token flags
        if ((currentFlaggedToken & TI_AFTER_EOL) != 0) {
            tt = Token.EOL;
        }
        return tt;
    }

    private void setCheckForLabel()
    {
        if ((currentFlaggedToken & CLEAR_TI_MASK) != Token.NAME)
            throw Kit.codeBug();
        currentFlaggedToken |= TI_CHECK_LABEL;
    }

    private void mustMatchToken(int toMatch, String messageId)
        throws IOException, ParserException
    {
        if (!matchToken(toMatch, 0)) {
            reportError(messageId);
        }
    }

    private void mustHaveXML()
    {
        if (!compilerEnv.isXmlAvailable()) {
            reportError("msg.XML.not.available");
        }
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

    private Node enterLoop(Node loopLabel)
    {
        Node loop = nf.createLoopNode(loopLabel, ts.getLineno());
        if (loopSet == null) {
            loopSet = new ObjArray();
            if (loopAndSwitchSet == null) {
                loopAndSwitchSet = new ObjArray();
            }
        }
        loopSet.push(loop);
        loopAndSwitchSet.push(loop);
        return loop;
    }

    private void exitLoop()
    {
        loopSet.pop();
        loopAndSwitchSet.pop();
    }

    private Node enterSwitch(Node switchSelector, int lineno, Node switchLabel)
    {
        Node switchNode = nf.createSwitch(switchSelector, lineno);
        if (loopAndSwitchSet == null) {
            loopAndSwitchSet = new ObjArray();
        }
        loopAndSwitchSet.push(switchNode);
        return switchNode;
    }

    private void exitSwitch()
    {
        loopAndSwitchSet.pop();
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
                                String sourceURI, int lineno)
    {
        this.sourceURI = sourceURI;
        this.ts = new TokenStream(this, null, sourceString, lineno);
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
                                String sourceURI, int lineno)
        throws IOException
    {
        this.sourceURI = sourceURI;
        this.ts = new TokenStream(this, sourceReader, null, lineno);
        return parse();
    }

    private ScriptOrFnNode parse()
        throws IOException
    {
        this.decompiler = createDecompiler(compilerEnv);
        this.nf = new IRFactory(this);
        currentScriptOrFn = nf.createScript();
        int sourceStartOffset = decompiler.getCurrentOffset();
        this.encodedSource = null;
        decompiler.addToken(Token.SCRIPT);

        this.currentFlaggedToken = Token.EOF;
        this.syntaxErrorCount = 0;

        int baseLineno = ts.getLineno();  // line number where source starts

        /* so we have something to add nodes to until
         * we've collected all the source */
        Node pn = nf.createLeaf(Token.BLOCK);

        try {
            for (;;) {
                int tt = peekToken(TokenStream.TSF_REGEXP);

                if (tt <= Token.EOF) {
                    break;
                }

                Node n;
                if (tt == Token.FUNCTION) {
                    consumeToken();
                    try {
                        n = function(FunctionNode.FUNCTION_STATEMENT);
                    } catch (ParserException e) {
                        break;
                    }
                } else {
                    n = statement();
                }
                nf.addChildToBack(pn, n);
            }
        } catch (StackOverflowError ex) {
            String msg = Context.getMessage0("mag.too.deep.parser.recursion");
            throw Context.reportRuntimeError(msg, sourceURI,
                                             ts.getLineno(), null, 0);
        }

        if (this.syntaxErrorCount != 0) {
            String msg = String.valueOf(this.syntaxErrorCount);
            msg = Context.getMessage1("msg.got.syntax.errors", msg);
            throw errorReporter.runtimeError(msg, sourceURI, baseLineno,
                                             null, 0);
        }

        currentScriptOrFn.setSourceName(sourceURI);
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
            bodyLoop: for (;;) {
                Node n;
                int tt = peekToken(0);
                switch (tt) {
                  case Token.ERROR:
                  case Token.EOF:
                  case Token.RC:
                    break bodyLoop;

                  case Token.FUNCTION:
                    consumeToken();
                    n = function(FunctionNode.FUNCTION_STATEMENT);
                    break;
                  default:
                    n = statement();
                    break;
                }
                nf.addChildToBack(pn, n);
            }
        } catch (ParserException e) {
            // Ignore it
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
        if (matchToken(Token.NAME, 0)) {
            name = ts.getString();
            decompiler.addName(name);
            if (!matchToken(Token.LP, 0)) {
                if (compilerEnv.isAllowMemberExprAsFunctionName()) {
                    // Extension to ECMA: if 'function <name>' does not follow
                    // by '(', assume <name> starts memberExpr
                    Node memberExprHead = nf.createName(name);
                    name = "";
                    memberExprNode = memberExprTail(false, memberExprHead);
                }
                mustMatchToken(Token.LP, "msg.no.paren.parms");
            }
        } else if (matchToken(Token.LP, 0)) {
            // Anonymous function
            name = "";
        } else {
            name = "";
            if (compilerEnv.isAllowMemberExprAsFunctionName()) {
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
        Hashtable savedLabelSet = labelSet;
        labelSet = null;
        ObjArray savedLoopSet = loopSet;
        loopSet = null;
        ObjArray savedLoopAndSwitchSet = loopAndSwitchSet;
        loopAndSwitchSet = null;

        Node body;
        String source;
        try {
            decompiler.addToken(Token.LP);
            if (!matchToken(Token.RP, 0)) {
                boolean first = true;
                do {
                    if (!first)
                        decompiler.addToken(Token.COMMA);
                    first = false;
                    mustMatchToken(Token.NAME, "msg.no.parm");
                    String s = ts.getString();
                    if (fnNode.hasParamOrVar(s)) {
                        addWarning("msg.dup.parms", s);
                    }
                    fnNode.addParam(s);
                    decompiler.addName(s);
                } while (matchToken(Token.COMMA, 0));

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
                 if (compilerEnv.getLanguageVersion() >= Context.VERSION_1_2) {
                    // function f() {} function g() {} is not allowed in 1.2
                    // or later but for compatibility with old scripts
                    // the check is done only if language is
                    // explicitly set.
                    //  XXX warning needed if version == VERSION_DEFAULT ?
                    int tt = peekTokenOrEOL(TokenStream.TSF_REGEXP);
                    if (tt == Token.FUNCTION) {
                         reportError("msg.no.semi.stmt");
                    }
                 }
                // Add EOL only if function is not part of expression
                // since it gets SEMI + EOL from Statement in that case
                decompiler.addToken(Token.EOL);
            }
        }
        finally {
            loopAndSwitchSet = savedLoopAndSwitchSet;
            loopSet = savedLoopSet;
            labelSet = savedLabelSet;
            nestingOfWith = savedNestingOfWith;
            currentScriptOrFn = savedScriptOrFn;
        }

        fnNode.setEncodedSourceBounds(functionSourceStart, functionSourceEnd);
        fnNode.setSourceName(sourceURI);
        fnNode.setBaseLineno(baseLineno);
        fnNode.setEndLineno(ts.getLineno());

        Node pn = nf.initFunction(fnNode, functionIndex, body, syntheticType);
        if (memberExprNode != null) {
            pn = nf.initFunction(fnNode, functionIndex, body, syntheticType);
            pn = nf.createAssignment(Token.ASSIGN, memberExprNode, pn);
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
        while((tt = peekToken(0)) > Token.EOF && tt != Token.RC) {
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

    // match a NAME; return null if no match.
    private Node matchJumpLabelName()
        throws IOException, ParserException
    {
        Node label = null;

        int tt = peekTokenOrEOL(0);
        if (tt == Token.NAME) {
            consumeToken();
            String name = ts.getString();
            decompiler.addName(name);
            if (labelSet != null) {
                label = (Node)labelSet.get(name);
            }
            if (label == null) {
                reportError("msg.undef.label");
            }
        }

        return label;
    }

    private Node statement()
        throws IOException
    {
        try {
            Node pn = statementHelper(null);
            if (pn != null) {
                return pn;
            }
        } catch (ParserException e) { }

        // skip to end of statement
        int lineno = ts.getLineno();
        guessingStatementEnd: for (;;) {
            int tt = peekTokenOrEOL(0);
            consumeToken();
            switch (tt) {
              case Token.ERROR:
              case Token.EOF:
              case Token.EOL:
              case Token.SEMI:
                break guessingStatementEnd;
            }
        }
        return nf.createExprStatement(nf.createName("error"), lineno);
    }

    /**
     * Whether the "catch (e: e instanceof Exception) { ... }" syntax
     * is implemented.
     */

    private Node statementHelper(Node statementLabel)
        throws IOException, ParserException
    {
        Node pn = null;

        int tt;

        tt = peekToken(0);

        switch(tt) {
          case Token.IF: {
            consumeToken();

            decompiler.addToken(Token.IF);
            int lineno = ts.getLineno();
            Node cond = condition();
            decompiler.addEOL(Token.LC);
            Node ifTrue = statement();
            Node ifFalse = null;
            if (matchToken(Token.ELSE, 0)) {
                decompiler.addToken(Token.RC);
                decompiler.addToken(Token.ELSE);
                decompiler.addEOL(Token.LC);
                ifFalse = statement();
            }
            decompiler.addEOL(Token.RC);
            pn = nf.createIf(cond, ifTrue, ifFalse, lineno);
            return pn;
          }

          case Token.SWITCH: {
            consumeToken();

            decompiler.addToken(Token.SWITCH);
            int lineno = ts.getLineno();
            mustMatchToken(Token.LP, "msg.no.paren.switch");
            decompiler.addToken(Token.LP);
            pn = enterSwitch(expr(false), lineno, statementLabel);
            try {
                mustMatchToken(Token.RP, "msg.no.paren.after.switch");
                decompiler.addToken(Token.RP);
                mustMatchToken(Token.LC, "msg.no.brace.switch");
                decompiler.addEOL(Token.LC);

                boolean hasDefault = false;
                switchLoop: for (;;) {
                    tt = nextToken(0);
                    Node caseExpression;
                    switch (tt) {
                      case Token.RC:
                        break switchLoop;

                      case Token.CASE:
                        decompiler.addToken(Token.CASE);
                        caseExpression = expr(false);
                        mustMatchToken(Token.COLON, "msg.no.colon.case");
                        decompiler.addEOL(Token.COLON);
                        break;

                      case Token.DEFAULT:
                        if (hasDefault) {
                            reportError("msg.double.switch.default");
                        }
                        decompiler.addToken(Token.DEFAULT);
                        hasDefault = true;
                        caseExpression = null;
                        mustMatchToken(Token.COLON, "msg.no.colon.case");
                        decompiler.addEOL(Token.COLON);
                        break;

                      default:
                        reportError("msg.bad.switch");
                        break switchLoop;
                    }

                    Node block = nf.createLeaf(Token.BLOCK);
                    while ((tt = peekToken(0)) != Token.RC
                           && tt != Token.CASE
                           && tt != Token.DEFAULT
                           && tt != Token.EOF)
                    {
                        nf.addChildToBack(block, statement());
                    }

                    // caseExpression == null => add default lable
                    nf.addSwitchCase(pn, caseExpression, block);
                }
                decompiler.addEOL(Token.RC);
                nf.closeSwitch(pn);
            } finally {
                exitSwitch();
            }
            return pn;
          }

          case Token.WHILE: {
            consumeToken();
            decompiler.addToken(Token.WHILE);

            Node loop = enterLoop(statementLabel);
            try {
                Node cond = condition();
                decompiler.addEOL(Token.LC);
                Node body = statement();
                decompiler.addEOL(Token.RC);
                pn = nf.createWhile(loop, cond, body);
            } finally {
                exitLoop();
            }
            return pn;
          }

          case Token.DO: {
            consumeToken();
            decompiler.addToken(Token.DO);
            decompiler.addEOL(Token.LC);

            Node loop = enterLoop(statementLabel);
            try {
                Node body = statement();
                decompiler.addToken(Token.RC);
                mustMatchToken(Token.WHILE, "msg.no.while.do");
                decompiler.addToken(Token.WHILE);
                Node cond = condition();
                pn = nf.createDoWhile(loop, body, cond);
            } finally {
                exitLoop();
            }
            // Always auto-insert semicon to follow SpiderMonkey:
            // It is required by EMAScript but is ignored by the rest of
            // world, see bug 238945
            matchToken(Token.SEMI, 0);
            decompiler.addEOL(Token.SEMI);
            return pn;
          }

          case Token.FOR: {
            consumeToken();
            boolean isForEach = false;
            decompiler.addToken(Token.FOR);

            Node loop = enterLoop(statementLabel);
            try {

                Node init;  // Node init is also foo in 'foo in Object'
                Node cond;  // Node cond is also object in 'foo in Object'
                Node incr = null; // to kill warning
                Node body;

                // See if this is a for each () instead of just a for ()
                if (matchToken(Token.NAME, 0)) {
                    decompiler.addName(ts.getString());
                    if (ts.getString().equals("each")) {
                        isForEach = true;
                    } else {
                        reportError("msg.no.paren.for");
                    }
                }

                mustMatchToken(Token.LP, "msg.no.paren.for");
                decompiler.addToken(Token.LP);
                tt = peekToken(0);
                if (tt == Token.SEMI) {
                    init = nf.createLeaf(Token.EMPTY);
                } else {
                    if (tt == Token.VAR) {
                        // set init to a var list or initial
                        consumeToken();    // consume the 'var' token
                        init = variables(true);
                    }
                    else {
                        init = expr(true);
                    }
                }

                if (matchToken(Token.IN, 0)) {
                    decompiler.addToken(Token.IN);
                    // 'cond' is the object over which we're iterating
                    cond = expr(false);
                } else {  // ordinary for loop
                    mustMatchToken(Token.SEMI, "msg.no.semi.for");
                    decompiler.addToken(Token.SEMI);
                    if (peekToken(0) == Token.SEMI) {
                        // no loop condition
                        cond = nf.createLeaf(Token.EMPTY);
                    } else {
                        cond = expr(false);
                    }

                    mustMatchToken(Token.SEMI, "msg.no.semi.for.cond");
                    decompiler.addToken(Token.SEMI);
                    if (peekToken(0) == Token.RP) {
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
                    // cond could be null if 'in obj' got eaten
                    // by the init node.
                    pn = nf.createForIn(loop, init, cond, body, isForEach);
                } else {
                    pn = nf.createFor(loop, init, cond, incr, body);
                }
            } finally {
                exitLoop();
            }
            return pn;
          }

          case Token.TRY: {
            consumeToken();
            int lineno = ts.getLineno();

            Node tryblock;
            Node catchblocks = null;
            Node finallyblock = null;

            decompiler.addToken(Token.TRY);
            decompiler.addEOL(Token.LC);
            tryblock = statement();
            decompiler.addEOL(Token.RC);

            catchblocks = nf.createLeaf(Token.BLOCK);

            boolean sawDefaultCatch = false;
            int peek = peekToken(0);
            if (peek == Token.CATCH) {
                while (matchToken(Token.CATCH, 0)) {
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
                    if (matchToken(Token.IF, 0)) {
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

            if (matchToken(Token.FINALLY, 0)) {
                decompiler.addToken(Token.FINALLY);
                decompiler.addEOL(Token.LC);
                finallyblock = statement();
                decompiler.addEOL(Token.RC);
            }

            pn = nf.createTryCatchFinally(tryblock, catchblocks,
                                          finallyblock, lineno);

            return pn;
          }

          case Token.THROW: {
            consumeToken();
            int lineno = ts.getLineno();
            decompiler.addToken(Token.THROW);
            pn = nf.createThrow(expr(false), lineno);
            break;
          }

          case Token.BREAK: {
            consumeToken();
            int lineno = ts.getLineno();

            decompiler.addToken(Token.BREAK);

            // matchJumpLabelName only matches if there is one
            Node breakStatement = matchJumpLabelName();
            if (breakStatement == null) {
                if (loopAndSwitchSet == null || loopAndSwitchSet.size() == 0) {
                    reportError("msg.bad.break");
                    return null;
                }
                breakStatement = (Node)loopAndSwitchSet.peek();
            }
            pn = nf.createBreak(breakStatement, lineno);
            break;
          }

          case Token.CONTINUE: {
            consumeToken();
            int lineno = ts.getLineno();

            decompiler.addToken(Token.CONTINUE);

            Node loop;
            // matchJumpLabelName only matches if there is one
            Node label = matchJumpLabelName();
            if (label == null) {
                if (loopSet == null || loopSet.size() == 0) {
                    reportError("msg.continue.outside");
                    return null;
                }
                loop = (Node)loopSet.peek();
            } else {
                loop = nf.getLabelLoop(label);
                if (loop == null) {
                    reportError("msg.continue.nonloop");
                    return null;
                }
            }
            pn = nf.createContinue(loop, lineno);
            break;
          }

          case Token.WITH: {
            consumeToken();

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
            return pn;
          }

          case Token.VAR: {
            consumeToken();
            pn = variables(false);
            break;
          }

          case Token.RETURN: {
            if (!insideFunction()) {
                reportError("msg.bad.return");
            }
            consumeToken();
            decompiler.addToken(Token.RETURN);
            int lineno = ts.getLineno();

            Node retExpr;
            /* This is ugly, but we don't want to require a semicolon. */
            tt = peekTokenOrEOL(TokenStream.TSF_REGEXP);
            switch (tt) {
              case Token.SEMI:
              case Token.RC:
              case Token.EOF:
              case Token.EOL:
              case Token.ERROR:
                retExpr = null;
                break;
              default:
                retExpr = expr(false);
            }
            pn = nf.createReturn(retExpr, lineno);
            break;
          }

          case Token.LC:
            consumeToken();
            if (statementLabel != null) {
                decompiler.addToken(Token.LC);
            }
            pn = statements();
            mustMatchToken(Token.RC, "msg.no.brace.block");
            if (statementLabel != null) {
                decompiler.addEOL(Token.RC);
            }
            return pn;

          case Token.ERROR:
            // Fall thru, to have a node for error recovery to work on
          case Token.SEMI:
            consumeToken();
            pn = nf.createLeaf(Token.EMPTY);
            return pn;

          case Token.FUNCTION: {
            consumeToken();
            pn = function(FunctionNode.FUNCTION_EXPRESSION_STATEMENT);
            return pn;
          }

          case Token.DEFAULT :
            consumeToken();
            mustHaveXML();

            decompiler.addToken(Token.DEFAULT);
            int nsLine = ts.getLineno();

            if (!(matchToken(Token.NAME, 0)
                  && ts.getString().equals("xml")))
            {
                reportError("msg.bad.namespace");
            }
            decompiler.addName(ts.getString());

            if (!(matchToken(Token.NAME, 0)
                  && ts.getString().equals("namespace")))
            {
                reportError("msg.bad.namespace");
            }
            decompiler.addName(ts.getString());

            if (!matchToken(Token.ASSIGN, 0)) {
                reportError("msg.bad.namespace");
            }
            decompiler.addToken(Token.ASSIGN);

            Node expr = expr(false);
            pn = nf.createDefaultNamespace(expr, nsLine);
            break;

          case Token.NAME: {
            int lineno = ts.getLineno();
            String name = ts.getString();
            setCheckForLabel();
            pn = expr(false);
            if (pn.getType() != Token.LABEL) {
                pn = nf.createExprStatement(pn, lineno);
            } else {
                // Parsed the label: push back token should be
                // colon that primaryExpr left untouched.
                if (peekToken(0) != Token.COLON) Kit.codeBug();
                consumeToken();
                // depend on decompiling lookahead to guess that that
                // last name was a label.
                decompiler.addName(name);
                decompiler.addEOL(Token.COLON);

                if (labelSet == null) {
                    labelSet = new Hashtable();
                } else if (labelSet.containsKey(name)) {
                    reportError("msg.dup.label");
                }

                boolean firstLabel;
                if (statementLabel == null) {
                    firstLabel = true;
                    statementLabel = pn;
                } else {
                    // Discard multiple label nodes and use only
                    // the first: it allows to simplify IRFactory
                    firstLabel = false;
                }
                labelSet.put(name, statementLabel);
                try {
                    pn = statementHelper(statementLabel);
                } finally {
                    labelSet.remove(name);
                }
                if (firstLabel) {
                    pn = nf.createLabeledStatement(statementLabel, pn);
                }
                return pn;
            }
            break;
          }

          default: {
            int lineno = ts.getLineno();
            pn = expr(false);
            pn = nf.createExprStatement(pn, lineno);
            break;
          }
        }

        int ttFlagged = peekFlaggedToken(TokenStream.TSF_REGEXP);
        switch (ttFlagged & CLEAR_TI_MASK) {
          case Token.SEMI:
            // Consume ';' as a part of expression
            consumeToken();
            break;
          case Token.ERROR:
          case Token.EOF:
          case Token.RC:
            // Autoinsert ;
            break;
          default:
            if ((ttFlagged & TI_AFTER_EOL) == 0) {
                // Report error if no EOL or autoinsert ; otherwise
                reportError("msg.no.semi.stmt");
            }
            break;
        }
        decompiler.addEOL(Token.SEMI);

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

            if (matchToken(Token.ASSIGN, 0)) {
                decompiler.addToken(Token.ASSIGN);

                init = assignExpr(inForInit);
                nf.addChildToBack(name, init);
            }
            nf.addChildToBack(pn, name);
            if (!matchToken(Token.COMMA, 0))
                break;
        }
        return pn;
    }

    private Node expr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = assignExpr(inForInit);
        while (matchToken(Token.COMMA, 0)) {
            decompiler.addToken(Token.COMMA);
            pn = nf.createBinary(Token.COMMA, pn, assignExpr(inForInit));
        }
        return pn;
    }

    private Node assignExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = condExpr(inForInit);

        int tt = peekToken(0);
        if (Token.FIRST_ASSIGN <= tt && tt <= Token.LAST_ASSIGN) {
            consumeToken();
            decompiler.addToken(tt);
            pn = nf.createAssignment(tt, pn, assignExpr(inForInit));
        }

        return pn;
    }

    private Node condExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node ifTrue;
        Node ifFalse;

        Node pn = orExpr(inForInit);

        if (matchToken(Token.HOOK, 0)) {
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
        if (matchToken(Token.OR, 0)) {
            decompiler.addToken(Token.OR);
            pn = nf.createBinary(Token.OR, pn, orExpr(inForInit));
        }

        return pn;
    }

    private Node andExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = bitOrExpr(inForInit);
        if (matchToken(Token.AND, 0)) {
            decompiler.addToken(Token.AND);
            pn = nf.createBinary(Token.AND, pn, andExpr(inForInit));
        }

        return pn;
    }

    private Node bitOrExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = bitXorExpr(inForInit);
        while (matchToken(Token.BITOR, 0)) {
            decompiler.addToken(Token.BITOR);
            pn = nf.createBinary(Token.BITOR, pn, bitXorExpr(inForInit));
        }
        return pn;
    }

    private Node bitXorExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = bitAndExpr(inForInit);
        while (matchToken(Token.BITXOR, 0)) {
            decompiler.addToken(Token.BITXOR);
            pn = nf.createBinary(Token.BITXOR, pn, bitAndExpr(inForInit));
        }
        return pn;
    }

    private Node bitAndExpr(boolean inForInit)
        throws IOException, ParserException
    {
        Node pn = eqExpr(inForInit);
        while (matchToken(Token.BITAND, 0)) {
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
            int tt = peekToken(0);
            switch (tt) {
              case Token.EQ:
              case Token.NE:
              case Token.SHEQ:
              case Token.SHNE:
                consumeToken();
                int decompilerToken = tt;
                int parseToken = tt;
                if (compilerEnv.getLanguageVersion() == Context.VERSION_1_2) {
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
            int tt = peekToken(0);
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
                consumeToken();
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
            int tt = peekToken(0);
            switch (tt) {
              case Token.LSH:
              case Token.URSH:
              case Token.RSH:
                consumeToken();
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
            int tt = peekToken(0);
            if (tt == Token.ADD || tt == Token.SUB) {
                consumeToken();
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
            int tt = peekToken(0);
            switch (tt) {
              case Token.MUL:
              case Token.DIV:
              case Token.MOD:
                consumeToken();
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

        tt = peekToken(TokenStream.TSF_REGEXP);

        switch(tt) {
        case Token.VOID:
        case Token.NOT:
        case Token.BITNOT:
        case Token.TYPEOF:
            consumeToken();
            decompiler.addToken(tt);
            return nf.createUnary(tt, unaryExpr());

        case Token.ADD:
            consumeToken();
            // Convert to special POS token in decompiler and parse tree
            decompiler.addToken(Token.POS);
            return nf.createUnary(Token.POS, unaryExpr());

        case Token.SUB:
            consumeToken();
            // Convert to special NEG token in decompiler and parse tree
            decompiler.addToken(Token.NEG);
            return nf.createUnary(Token.NEG, unaryExpr());

        case Token.INC:
        case Token.DEC:
            consumeToken();
            decompiler.addToken(tt);
            return nf.createIncDec(tt, false, memberExpr(true));

        case Token.DELPROP:
            consumeToken();
            decompiler.addToken(Token.DELPROP);
            return nf.createUnary(Token.DELPROP, unaryExpr());

        case Token.ERROR:
            consumeToken();
            break;

        // XML stream encountered in expression.
        case Token.LT:
            if (compilerEnv.isXmlAvailable()) {
                consumeToken();
                Node pn = xmlInitializer();
                return memberExprTail(true, pn);
            }
            // Fall thru to the default handling of RELOP

        default:
            Node pn = memberExpr(true);

            // Don't look across a newline boundary for a postfix incop.
            tt = peekTokenOrEOL(0);
            if (tt == Token.INC || tt == Token.DEC) {
                consumeToken();
                decompiler.addToken(tt);
                return nf.createIncDec(tt, true, pn);
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
                expr = (peekToken(0) == Token.RC)
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
                reportError("msg.syntax");
                return null;
            }
        }
    }

    private void argumentList(Node listNode)
        throws IOException, ParserException
    {
        boolean matched;
        matched = matchToken(Token.RP, TokenStream.TSF_REGEXP);
        if (!matched) {
            boolean first = true;
            do {
                if (!first)
                    decompiler.addToken(Token.COMMA);
                first = false;
                nf.addChildToBack(listNode, assignExpr(false));
            } while (matchToken(Token.COMMA, 0));

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
        tt = peekToken(TokenStream.TSF_REGEXP);
        if (tt == Token.NEW) {
            /* Eat the NEW token. */
            consumeToken();
            decompiler.addToken(Token.NEW);

            /* Make a NEW node to append to. */
            pn = nf.createCallOrNew(Token.NEW, memberExpr(false));

            if (matchToken(Token.LP, 0)) {
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
            tt = peekToken(0);
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
      tailLoop:
        for (;;) {
            int tt = peekToken(0);
            switch (tt) {

              case Token.DOT: {
                consumeToken();
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
                consumeToken();
                mustHaveXML();
                decompiler.addToken(Token.DOTDOT);
                Node n = nameOrPropertyIdentifier();
                pn = nf.createBinary(Token.DOTDOT, pn, n);
                break;
              }

              case Token.DOTQUERY:
                consumeToken();
                mustHaveXML();
                decompiler.addToken(Token.DOTQUERY);
                pn = nf.createDotQuery(pn, expr(false), ts.getLineno());
                mustMatchToken(Token.RP, "msg.no.paren");
                break;

              case Token.LB:
                consumeToken();
                decompiler.addToken(Token.LB);
                pn = nf.createBinary(Token.LB, pn, expr(false));
                mustMatchToken(Token.RB, "msg.no.bracket.index");
                decompiler.addToken(Token.RB);
                break;

              case Token.LP:
                if (!allowCallSyntax) {
                    break tailLoop;
                }
                consumeToken();
                decompiler.addToken(Token.LP);
                pn = nf.createCallOrNew(Token.CALL, pn);
                /* Add the arguments to pn, if any are supplied. */
                argumentList(pn);
                break;

              default:
                break tailLoop;
            }
        }
        return pn;
    }

    private Node nameOrPropertyIdentifier()
        throws IOException, ParserException
    {
        Node pn;
        int tt = nextToken(0);

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
        int tt = nextToken(0);

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
        if (matchToken(Token.COLONCOLON, 0)) {
            decompiler.addToken(Token.COLONCOLON);

            Node pn;
            int tt = nextToken(0);

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
        Node pn;

        int ttFlagged = nextFlaggedToken(TokenStream.TSF_REGEXP);
        int tt = ttFlagged & CLEAR_TI_MASK;

        switch(tt) {

          case Token.FUNCTION:
            return function(FunctionNode.FUNCTION_EXPRESSION);

          case Token.LB: {
            ObjArray elems = new ObjArray();
            int skipCount = 0;
            decompiler.addToken(Token.LB);
            boolean after_lb_or_comma = true;
            for (;;) {
                tt = peekToken(TokenStream.TSF_REGEXP);

                if (tt == Token.COMMA) {
                    consumeToken();
                    decompiler.addToken(Token.COMMA);
                    if (!after_lb_or_comma) {
                        after_lb_or_comma = true;
                    } else {
                        elems.add(null);
                        ++skipCount;
                    }
                } else if (tt == Token.RB) {
                    consumeToken();
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
            if (!matchToken(Token.RC, 0)) {

                boolean first = true;
            commaloop:
                do {
                    Object property;

                    if (!first)
                        decompiler.addToken(Token.COMMA);
                    else
                        first = false;

                    tt = peekToken(0);
                    switch(tt) {
                      case Token.NAME:
                      case Token.STRING:
                        consumeToken();
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
                        consumeToken();
                        double n = ts.getNumber();
                        decompiler.addNumber(n);
                        property = ScriptRuntime.getIndexObject(n);
                        break;

                      case Token.RC:
                        // trailing comma is OK.
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
                } while (matchToken(Token.COMMA, 0));

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
            if ((ttFlagged & TI_CHECK_LABEL) != 0) {
                if (peekToken(0) == Token.COLON) {
                    // Do not consume colon, it is used as unwind indicator
                    // to return to statementHelper.
                    // XXX Better way?
                    return nf.createLabel(ts.getLineno());
                }
            }

            decompiler.addName(name);
            if (compilerEnv.isXmlAvailable()) {
                pn = nameOrQualifiedName(name, true);
            } else {
                pn = nf.createName(name);
            }
            return pn;
          }

          case Token.NUMBER: {
            double n = ts.getNumber();
            decompiler.addNumber(n);
            return nf.createNumber(n);
          }

          case Token.STRING: {
            String s = ts.getString();
            decompiler.addString(s);
            return nf.createString(s);
          }

          case Token.REGEXP: {
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

          case Token.EOF:
            reportError("msg.unexpected.eof");
            break;

          default:
            reportError("msg.syntax");
            break;
        }
        return null;    // should never reach here
    }

}

