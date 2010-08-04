/* -*- Mode: JS; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set sw=4 ts=4 et tw=78:
 * ***** BEGIN LICENSE BLOCK *****
 *
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Narcissus JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Brendan Eich <brendan@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Narcissus - JS implemented in JS.
 *
 * Parser.
 */

/*
 * The vanilla AST builder.
 */

Narcissus.jsparse = (function() {

    var jslex = Narcissus.jslex;
    var jsdefs = Narcissus.jsdefs;
    
    // Set constants in the local scope.
    eval(jsdefs.consts);
    
    VanillaBuilder = function VanillaBuilder() {
    }
    
    VanillaBuilder.prototype = {
        IF$build: function(t) {
            return new Node(t, IF);
        },
    
        IF$setCondition: function(n, e) {
            n.condition = e;
        },
    
        IF$setThenPart: function(n, s) {
            n.thenPart = s;
        },
    
        IF$setElsePart: function(n, s) {
            n.elsePart = s;
        },
    
        IF$finish: function(n) {
        },
    
        SWITCH$build: function(t) {
            var n = new Node(t, SWITCH);
            n.cases = [];
            n.defaultIndex = -1;
            return n;
        },
    
        SWITCH$setDiscriminant: function(n, e) {
            n.discriminant = e;
        },
    
        SWITCH$setDefaultIndex: function(n, i) {
            n.defaultIndex = i;
        },
    
        SWITCH$addCase: function(n, n2) {
            n.cases.push(n2);
        },
    
        SWITCH$finish: function(n) {
        },
    
        CASE$build: function(t) {
            return new Node(t, CASE);
        },
    
        CASE$setLabel: function(n, e) {
            n.caseLabel = e;
        },
    
        CASE$initializeStatements: function(n, t) {
            n.statements = new Node(t, BLOCK);
        },
    
        CASE$addStatement: function(n, s) {
            n.statements.push(s);
        },
    
        CASE$finish: function(n) {
        },
    
        DEFAULT$build: function(t, p) {
            return new Node(t, DEFAULT);
        },
    
        DEFAULT$initializeStatements: function(n, t) {
            n.statements = new Node(t, BLOCK);
        },
    
        DEFAULT$addStatement: function(n, s) {
            n.statements.push(s);
        },
    
        DEFAULT$finish: function(n) {
        },
    
        FOR$build: function(t) {
            var n = new Node(t, FOR);
            n.isLoop = true;
            n.isEach = false;
            return n;
        },
    
        FOR$rebuildForEach: function(n) {
            n.isEach = true;
        },
    
        // NB. This function is called after rebuildForEach, if that's called
        // at all.
        FOR$rebuildForIn: function(n) {
            n.type = FOR_IN;
        },
    
        FOR$setCondition: function(n, e) {
            n.condition = e;
        },
    
        FOR$setSetup: function(n, e) {
            n.setup = e || null;
        },
    
        FOR$setUpdate: function(n, e) {
            n.update = e;
        },
    
        FOR$setObject: function(n, e) {
            n.object = e;
        },
    
        FOR$setIterator: function(n, e, e2) {
            n.iterator = e;
            n.varDecl = e2;
        },
    
        FOR$setBody: function(n, s) {
            n.body = s;
        },
    
        FOR$finish: function(n) {
        },
    
        WHILE$build: function(t) {
            var n = new Node(t, WHILE);
            n.isLoop = true;
            return n;
        },
    
        WHILE$setCondition: function(n, e) {
            n.condition = e;
        },
    
        WHILE$setBody: function(n, s) {
            n.body = s;
        },
    
        WHILE$finish: function(n) {
        },
    
        DO$build: function(t) {
            var n = new Node(t, DO);
            n.isLoop = true;
            return n;
        },
    
        DO$setCondition: function(n, e) {
            n.condition = e;
        },
    
        DO$setBody: function(n, s) {
            n.body = s;
        },
    
        DO$finish: function(n) {
        },
    
        BREAK$build: function(t) {
            return new Node(t, BREAK);
        },
    
        BREAK$setLabel: function(n, v) {
            n.label = v;
        },
    
        BREAK$setTarget: function(n, n2) {
            n.target = n2;
        },
    
        BREAK$finish: function(n) {
        },
    
        CONTINUE$build: function(t) {
            return new Node(t, CONTINUE);
        },
    
        CONTINUE$setLabel: function(n, v) {
            n.label = v;
        },
    
        CONTINUE$setTarget: function(n, n2) {
            n.target = n2;
        },
    
        CONTINUE$finish: function(n) {
        },
    
        TRY$build: function(t) {
            var n = new Node(t, TRY);
            n.catchClauses = [];
            return n;
        },
    
        TRY$setTryBlock: function(n, s) {
            n.tryBlock = s;
        },
    
        TRY$addCatch: function(n, n2) {
            n.catchClauses.push(n2);
        },
    
        TRY$finishCatches: function(n) {
        },
    
        TRY$setFinallyBlock: function(n, s) {
            n.finallyBlock = s;
        },
    
        TRY$finish: function(n) {
        },
    
        CATCH$build: function(t) {
            var n = new Node(t, CATCH);
            n.guard = null;
            return n;
        },
    
        CATCH$setVarName: function(n, v) {
            n.varName = v;
        },
    
        CATCH$setGuard: function(n, e) {
            n.guard = e;
        },
    
        CATCH$setBlock: function(n, s) {
            n.block = s;
        },
    
        CATCH$finish: function(n) {
        },
    
        THROW$build: function(t) {
            return new Node(t, THROW);
        },
    
        THROW$setException: function(n, e) {
            n.exception = e;
        },
    
        THROW$finish: function(n) {
        },
    
        RETURN$build: function(t) {
            return new Node(t, RETURN);
        },
    
        RETURN$setValue: function(n, e) {
            n.value = e;
        },
    
        RETURN$finish: function(n) {
        },
    
        YIELD$build: function(t) {
            return new Node(t, YIELD);
        },
    
        YIELD$setValue: function(n, e) {
            n.value = e;
        },
    
        YIELD$finish: function(n) {
        },
    
        GENERATOR$build: function(t) {
            return new Node(t, GENERATOR);
        },
    
        GENERATOR$setExpression: function(n, e) {
            n.expression = e;
        },
    
        GENERATOR$setTail: function(n, n2) {
            n.tail = n2;
        },
    
        GENERATOR$finish: function(n) {
        },
    
        WITH$build: function(t) {
            return new Node(t, WITH);
        },
    
        WITH$setObject: function(n, e) {
            n.object = e;
        },
    
        WITH$setBody: function(n, s) {
            n.body = s;
        },
    
        WITH$finish: function(n) {
        },
    
        DEBUGGER$build: function(t) {
            return new Node(t, DEBUGGER);
        },
    
        SEMICOLON$build: function(t) {
            return new Node(t, SEMICOLON);
        },
    
        SEMICOLON$setExpression: function(n, e) {
            n.expression = e;
        },
    
        SEMICOLON$finish: function(n) {
        },
    
        LABEL$build: function(t) {
            return new Node(t, LABEL);
        },
    
        LABEL$setLabel: function(n, e) {
            n.label = e;
        },
    
        LABEL$setStatement: function(n, s) {
            n.statement = s;
        },
    
        LABEL$finish: function(n) {
        },
    
        FUNCTION$build: function(t) {
            var n = new Node(t);
            if (n.type != FUNCTION)
                n.type = (n.value == "get") ? GETTER : SETTER;
            n.params = [];
            return n;
        },
    
        FUNCTION$setName: function(n, v) {
            n.name = v;
        },
    
        FUNCTION$addParam: function(n, v) {
            n.params.push(v);
        },
    
        FUNCTION$setBody: function(n, s) {
            n.body = s;
        },
    
        FUNCTION$hoistVars: function(x) {
        },
    
        FUNCTION$finish: function(n, x) {
        },
    
        VAR$build: function(t) {
            return new Node(t, VAR);
        },
    
        VAR$addDecl: function(n, n2, x) {
            n.push(n2);
        },
    
        VAR$finish: function(n) {
        },
    
        CONST$build: function(t) {
            return new Node(t, VAR);
        },
    
        CONST$addDecl: function(n, n2, x) {
            n.push(n2);
        },
    
        CONST$finish: function(n) {
        },
    
        LET$build: function(t) {
            return new Node(t, LET);
        },
    
        LET$addDecl: function(n, n2, x) {
            n.push(n2);
        },
    
        LET$finish: function(n) {
        },
    
        DECL$build: function(t) {
            return new Node(t, IDENTIFIER);
        },
    
        DECL$setName: function(n, v) {
            n.name = v;
        },
    
        DECL$setInitializer: function(n, e) {
            n.initializer = e;
        },
    
        DECL$setReadOnly: function(n, b) {
            n.readOnly = b;
        },
    
        DECL$finish: function(n) {
        },
    
        LET_BLOCK$build: function(t) {
            var n = Node(t, LET_BLOCK);
            n.varDecls = [];
            return n;
        },
    
        LET_BLOCK$setVariables: function(n, n2) {
            n.variables = n2;
        },
    
        LET_BLOCK$setExpression: function(n, e) {
            n.expression = e;
        },
    
        LET_BLOCK$setBlock: function(n, s) {
            n.block = s;
        },
    
        LET_BLOCK$finish: function(n) {
        },
    
        BLOCK$build: function(t, id) {
            var n = new Node(t, BLOCK);
            n.varDecls = [];
            n.id = id;
            return n;
        },
    
        BLOCK$hoistLets: function(n) {
        },
    
        BLOCK$addStatement: function(n, n2) {
            n.push(n2);
        },
    
        BLOCK$finish: function(n) {
        },
    
        EXPRESSION$build: function(t, tt) {
            return new Node(t, tt);
        },
    
        EXPRESSION$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        EXPRESSION$finish: function(n) {
        },
    
        ASSIGN$build: function(t) {
            return new Node(t, ASSIGN);
        },
    
        ASSIGN$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        ASSIGN$setAssignOp: function(n, o) {
            n.assignOp = o;
        },
    
        ASSIGN$finish: function(n) {
        },
    
        HOOK$build: function(t) {
            return new Node(t, HOOK);
        },
    
        HOOK$setCondition: function(n, e) {
            n[0] = e;
        },
    
        HOOK$setThenPart: function(n, n2) {
            n[1] = n2;
        },
    
        HOOK$setElsePart: function(n, n2) {
            n[2] = n2;
        },
    
        HOOK$finish: function(n) {
        },
    
        OR$build: function(t) {
            return new Node(t, OR);
        },
    
        OR$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        OR$finish: function(n) {
        },
    
        AND$build: function(t) {
            return new Node(t, AND);
        },
    
        AND$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        AND$finish: function(n) {
        },
    
        BITWISE_OR$build: function(t) {
            return new Node(t, BITWISE_OR);
        },
    
        BITWISE_OR$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        BITWISE_OR$finish: function(n) {
        },
    
        BITWISE_XOR$build: function(t) {
            return new Node(t, BITWISE_XOR);
        },
    
        BITWISE_XOR$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        BITWISE_XOR$finish: function(n) {
        },
    
        BITWISE_AND$build: function(t) {
            return new Node(t, BITWISE_AND);
        },
    
        BITWISE_AND$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        BITWISE_AND$finish: function(n) {
        },
    
        EQUALITY$build: function(t) {
            // NB t.token.type must be EQ, NE, STRICT_EQ, or STRICT_NE.
            return new Node(t);
        },
    
        EQUALITY$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        EQUALITY$finish: function(n) {
        },
    
        RELATIONAL$build: function(t) {
            // NB t.token.type must be LT, LE, GE, or GT.
            return new Node(t);
        },
    
        RELATIONAL$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        RELATIONAL$finish: function(n) {
        },
    
        SHIFT$build: function(t) {
            // NB t.token.type must be LSH, RSH, or URSH.
            return new Node(t);
        },
    
        SHIFT$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        SHIFT$finish: function(n) {
        },
    
        ADD$build: function(t) {
            // NB t.token.type must be PLUS or MINUS.
            return new Node(t);
        },
    
        ADD$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        ADD$finish: function(n) {
        },
    
        MULTIPLY$build: function(t) {
            // NB t.token.type must be MUL, DIV, or MOD.
            return new Node(t);
        },
    
        MULTIPLY$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        MULTIPLY$finish: function(n) {
        },
    
        UNARY$build: function(t) {
            // NB t.token.type must be DELETE, VOID, TYPEOF, NOT, BITWISE_NOT,
            // UNARY_PLUS, UNARY_MINUS, INCREMENT, or DECREMENT.
            if (t.token.type == PLUS)
                t.token.type = UNARY_PLUS;
            else if (t.token.type == MINUS)
                t.token.type = UNARY_MINUS;
            return new Node(t);
        },
    
        UNARY$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        UNARY$setPostfix: function(n) {
            n.postfix = true;
        },
    
        UNARY$finish: function(n) {
        },
    
        MEMBER$build: function(t, tt) {
            // NB t.token.type must be NEW, DOT, or INDEX.
            return new Node(t, tt);
        },
    
        MEMBER$rebuildNewWithArgs: function(n) {
            n.type = NEW_WITH_ARGS;
        },
    
        MEMBER$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        MEMBER$finish: function(n) {
        },
    
        PRIMARY$build: function(t, tt) {
            // NB t.token.type must be NULL, THIS, TRUIE, FALSE, IDENTIFIER,
            // NUMBER, STRING, or REGEXP.
            return new Node(t, tt);
        },
    
        PRIMARY$finish: function(n) {
        },
    
        ARRAY_INIT$build: function(t) {
            return new Node(t, ARRAY_INIT);
        },
    
        ARRAY_INIT$addElement: function(n, n2) {
            n.push(n2);
        },
    
        ARRAY_INIT$finish: function(n) {
        },
    
        ARRAY_COMP: {
            build: function(t) {
                return new Node(t, ARRAY_COMP);
            },
    
            setExpression: function(n, e) {
                n.expression = e
            },
    
            setTail: function(n, n2) {
                n.tail = n2;
            },
    
            finish: function(n) {
            }
        },
    
        COMP_TAIL$build: function(t) {
            return new Node(t, COMP_TAIL);
        },
    
        COMP_TAIL$setGuard: function(n, e) {
            n.guard = e;
        },
    
        COMP_TAIL$addFor: function(n, n2) {
            n.push(n2);
        },
    
        COMP_TAIL$finish: function(n) {
        },
    
        OBJECT_INIT$build: function(t) {
            return new Node(t, OBJECT_INIT);
        },
    
        OBJECT_INIT$addProperty: function(n, n2) {
            n.push(n2);
        },
    
        OBJECT_INIT$finish: function(n) {
        },
    
        PROPERTY_INIT$build: function(t) {
            return new Node(t, PROPERTY_INIT);
        },
    
        PROPERTY_INIT$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        PROPERTY_INIT$finish: function(n) {
        },
    
        COMMA$build: function(t) {
            return new Node(t, COMMA);
        },
    
        COMMA$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        COMMA$finish: function(n) {
        },
    
        LIST$build: function(t) {
            return new Node(t, LIST);
        },
    
        LIST$addOperand: function(n, n2) {
            n.push(n2);
        },
    
        LIST$finish: function(n) {
        },
    
        setHoists: function(id, vds) {
        }
    };
    
    function CompilerContext(inFunction, builder) {
        this.inFunction = inFunction;
        this.hasEmptyReturn = false;
        this.hasReturnWithValue = false;
        this.isGenerator = false;
        this.blockId = 0;
        this.builder = builder;
        this.stmtStack = [];
        this.funDecls = [];
        this.varDecls = [];
    }
    
    CompilerContext.prototype = {
        bracketLevel: 0,
        curlyLevel: 0,
        parenLevel: 0,
        hookLevel: 0,
        ecma3OnlyMode: false,
        inForLoopInit: false,
    };
    
    /*
     * Script :: (tokenizer, compiler context) -> node
     *
     * Parses the toplevel and function bodies.
     */
    function Script(t, x) {
        var n = Statements(t, x);
        n.type = SCRIPT;
        n.funDecls = x.funDecls;
        n.varDecls = x.varDecls;
        return n;
    }
    
    // Node extends Array, which we extend slightly with a top-of-stack method.
    jsdefs.defineProperty(Array.prototype, "top",
                   function() {
                       return this.length && this[this.length-1];
                   }, false, false, true);
    
    /*
     * Node :: (tokenizer, optional type) -> node
     */
    function Node(t, type) {
        var token = t.token;
        if (token) {
            this.type = type || token.type;
            this.value = token.value;
            this.lineno = token.lineno;
            // Start & end are file positions for error handling.
            this.start = token.start;
            this.end = token.end;
        } else {
            this.type = type;
            this.lineno = t.lineno;
        }
        // Nodes use a tokenizer for debugging (getSource, filename getter).
        this.tokenizer = t;
    
        for (var i = 2; i < arguments.length; i++)
            this.push(arguments[i]);
    }
    
    var Np = Node.prototype = new Array;
    Np.constructor = Node;
    Np.toSource = Object.prototype.toSource;
    
    // Always use push to add operands to an expression, to update start and end.
    Np.push = function (kid) {
        // kid can be null e.g. [1, , 2].
        if (kid !== null) {
            if (kid.start < this.start)
                this.start = kid.start;
            if (this.end < kid.end)
                this.end = kid.end;
        }
        return Array.prototype.push.call(this, kid);
    }
    
    Node.indentLevel = 0;
    
    function tokenstr(tt) {
        var t = jsdefs.tokens[tt];
        return /^\W/.test(t) ? jsdefs.opTypeNames[t] : t.toUpperCase();
    }
    
    Np.toString = function () {
        var a = [];
        for (var i in this) {
            if (this.hasOwnProperty(i) && i != 'type' && i != 'target')
                a.push({id: i, value: this[i]});
        }
        a.sort(function (a,b) { return (a.id < b.id) ? -1 : 1; });
        const INDENTATION = "    ";
        var n = ++Node.indentLevel;
        var s = "{\n" + INDENTATION.repeat(n) + "type: " + tokenstr(this.type);
        for (i = 0; i < a.length; i++)
            s += ",\n" + INDENTATION.repeat(n) + a[i].id + ": " + a[i].value;
        n = --Node.indentLevel;
        s += "\n" + INDENTATION.repeat(n) + "}";
        return s;
    }
    
    Np.getSource = function () {
        return this.tokenizer.source.slice(this.start, this.end);
    };
    
    jsdefs.defineGetter(Np, "filename",
                 function() {
                     return this.tokenizer.filename;
                 });
    
    jsdefs.defineProperty(String.prototype, "repeat",
                   function(n) {
                       var s = "", t = this + s;
                       while (--n >= 0)
                           s += t;
                       return s;
                   }, false, false, true);
    
    // Statement stack and nested statement handler.
    function nest(t, x, node, func, end) {
        x.stmtStack.push(node);
        var n = func(t, x);
        x.stmtStack.pop();
        end && t.mustMatch(end);
        return n;
    }
    
    /*
     * Statements :: (tokenizer, compiler context) -> node
     *
     * Parses a list of Statements.
     */
    function Statements(t, x) {
        var b = x.builder;
        var n = b.BLOCK$build(t, x.blockId++);
        b.BLOCK$hoistLets(n);
        x.stmtStack.push(n);
        while (!t.done && t.peek(true) != RIGHT_CURLY)
            b.BLOCK$addStatement(n, Statement(t, x));
        x.stmtStack.pop();
        b.BLOCK$finish(n);
        if (n.needsHoisting) {
            b.setHoists(n.id, n.varDecls);
            // Propagate up to the function.
            x.needsHoisting = true;
        }
        return n;
    }
    
    function Block(t, x) {
        t.mustMatch(LEFT_CURLY);
        var n = Statements(t, x);
        t.mustMatch(RIGHT_CURLY);
        return n;
    }
    
    const DECLARED_FORM = 0, EXPRESSED_FORM = 1, STATEMENT_FORM = 2;
    
    /*
     * Statement :: (tokenizer, compiler context) -> node
     *
     * Parses a Statement.
     */
    function Statement(t, x) {
        var i, label, n, n2, ss, tt = t.get(true);
        var b = x.builder;
    
        // Cases for statements ending in a right curly return early, avoiding the
        // common semicolon insertion magic after this switch.
        switch (tt) {
          case FUNCTION:
            // DECLARED_FORM extends funDecls of x, STATEMENT_FORM doesn't.
            return FunctionDefinition(t, x, true,
                                      (x.stmtStack.length > 1)
                                      ? STATEMENT_FORM
                                      : DECLARED_FORM);
    
          case LEFT_CURLY:
            n = Statements(t, x);
            t.mustMatch(RIGHT_CURLY);
            return n;
    
          case IF:
            n = b.IF$build(t);
            b.IF$setCondition(n, ParenExpression(t, x));
            x.stmtStack.push(n);
            b.IF$setThenPart(n, Statement(t, x));
            if (t.match(ELSE))
                b.IF$setElsePart(n, Statement(t, x));
            x.stmtStack.pop();
            b.IF$finish(n);
            return n;
    
          case SWITCH:
            // This allows CASEs after a DEFAULT, which is in the standard.
            n = b.SWITCH$build(t);
            b.SWITCH$setDiscriminant(n, ParenExpression(t, x));
            x.stmtStack.push(n);
            t.mustMatch(LEFT_CURLY);
            while ((tt = t.get()) != RIGHT_CURLY) {
                switch (tt) {
                  case DEFAULT:
                    if (n.defaultIndex >= 0)
                        throw t.newSyntaxError("More than one switch default");
                    n2 = b.DEFAULT$build(t);
                    b.SWITCH$setDefaultIndex(n, n.cases.length);
                    t.mustMatch(COLON);
                    b.DEFAULT$initializeStatements(n2, t);
                    while ((tt=t.peek(true)) != CASE && tt != DEFAULT &&
                           tt != RIGHT_CURLY)
                        b.DEFAULT$addStatement(n2, Statement(t, x));
                    b.DEFAULT$finish(n2);
                    break;
    
                  case CASE:
                    n2 = b.CASE$build(t);
                    b.CASE$setLabel(n2, Expression(t, x, COLON));
                    t.mustMatch(COLON);
                    b.CASE$initializeStatements(n2, t);
                    while ((tt=t.peek(true)) != CASE && tt != DEFAULT &&
                           tt != RIGHT_CURLY)
                        b.CASE$addStatement(n2, Statement(t, x));
                    b.CASE$finish(n2);
                    break;
    
                  default:
                    throw t.newSyntaxError("Invalid switch case");
                }
                b.SWITCH$addCase(n, n2);
            }
            x.stmtStack.pop();
            b.SWITCH$finish(n);
            return n;
    
          case FOR:
            n = b.FOR$build(t);
            if (t.match(IDENTIFIER) && t.token.value == "each")
                b.FOR$rebuildForEach(n);
            t.mustMatch(LEFT_PAREN);
            if ((tt = t.peek()) != SEMICOLON) {
                x.inForLoopInit = true;
                if (tt == VAR || tt == CONST) {
                    t.get();
                    n2 = Variables(t, x);
                } else if (tt == LET) {
                    t.get();
                    if (t.peek() == LEFT_PAREN) {
                        n2 = LetBlock(t, x, false);
                    } else {
                        /*
                         * Let in for head, we need to add an implicit block
                         * around the rest of the for.
                         */
                        var forBlock = b.BLOCK$build(t, x.blockId++);
                        x.stmtStack.push(forBlock);
                        n2 = Variables(t, x, forBlock);
                    }
                } else {
                    n2 = Expression(t, x);
                }
                x.inForLoopInit = false;
            }
            if (n2 && t.match(IN)) {
                b.FOR$rebuildForIn(n);
                b.FOR$setObject(n, Expression(t, x), forBlock);
                if (n2.type == VAR || n2.type == LET) {
                    if (n2.length != 1) {
                        throw new SyntaxError("Invalid for..in left-hand side",
                                              t.filename, n2.lineno);
                    }
                    b.FOR$setIterator(n, n2[0], n2, forBlock);
                } else {
                    b.FOR$setIterator(n, n2, null, forBlock);
                }
            } else {
                b.FOR$setSetup(n, n2);
                t.mustMatch(SEMICOLON);
                if (n.isEach)
                    throw t.newSyntaxError("Invalid for each..in loop");
                b.FOR$setCondition(n, (t.peek() == SEMICOLON)
                                  ? null
                                  : Expression(t, x));
                t.mustMatch(SEMICOLON);
                b.FOR$setUpdate(n, (t.peek() == RIGHT_PAREN)
                                   ? null
                                   : Expression(t, x));
            }
            t.mustMatch(RIGHT_PAREN);
            b.FOR$setBody(n, nest(t, x, n, Statement));
            if (forBlock) {
                b.BLOCK$finish(forBlock);
                x.stmtStack.pop();
            }
            b.FOR$finish(n);
            return n;
    
          case WHILE:
            n = b.WHILE$build(t);
            b.WHILE$setCondition(n, ParenExpression(t, x));
            b.WHILE$setBody(n, nest(t, x, n, Statement));
            b.WHILE$finish(n);
            return n;
    
          case DO:
            n = b.DO$build(t);
            b.DO$setBody(n, nest(t, x, n, Statement, WHILE));
            b.DO$setCondition(n, ParenExpression(t, x));
            b.DO$finish(n);
            if (!x.ecmaStrictMode) {
                // <script language="JavaScript"> (without version hints) may need
                // automatic semicolon insertion without a newline after do-while.
                // See http://bugzilla.mozilla.org/show_bug.cgi?id=238945.
                t.match(SEMICOLON);
                return n;
            }
            break;
    
          case BREAK:
          case CONTINUE:
            n = tt == BREAK ? b.BREAK$build(t) : b.CONTINUE$build(t);
    
            if (t.peekOnSameLine() == IDENTIFIER) {
                t.get();
                if (tt == BREAK)
                    b.BREAK$setLabel(n, t.token.value);
                else
                    b.CONTINUE$setLabel(n, t.token.value);
            }
    
            ss = x.stmtStack;
            i = ss.length;
            label = n.label;
    
            if (label) {
                do {
                    if (--i < 0)
                        throw t.newSyntaxError("Label not found");
                } while (ss[i].label != label);
    
                /*
                 * Both break and continue to label need to be handled specially
                 * within a labeled loop, so that they target that loop. If not in
                 * a loop, then break targets its labeled statement. Labels can be
                 * nested so we skip all labels immediately enclosing the nearest
                 * non-label statement.
                 */
                while (i < ss.length - 1 && ss[i+1].type == LABEL)
                    i++;
                if (i < ss.length - 1 && ss[i+1].isLoop)
                    i++;
                else if (tt == CONTINUE)
                    throw t.newSyntaxError("Invalid continue");
            } else {
                do {
                    if (--i < 0) {
                        throw t.newSyntaxError("Invalid " + ((tt == BREAK)
                                                             ? "break"
                                                             : "continue"));
                    }
                } while (!ss[i].isLoop && !(tt == BREAK && ss[i].type == SWITCH));
            }
            if (tt == BREAK) {
                b.BREAK$setTarget(n, ss[i]);
                b.BREAK$finish(n);
            } else {
                b.CONTINUE$setTarget(n, ss[i]);
                b.CONTINUE$finish(n);
            }
            break;
    
          case TRY:
            n = b.TRY$build(t);
            b.TRY$setTryBlock(n, Block(t, x));
            while (t.match(CATCH)) {
                n2 = b.CATCH$build(t);
                t.mustMatch(LEFT_PAREN);
                switch (t.get()) {
                  case LEFT_BRACKET:
                  case LEFT_CURLY:
                    // Destructured catch identifiers.
                    t.unget();
                    b.CATCH$setVarName(n2, DestructuringExpression(t, x, true));
                  case IDENTIFIER:
                    b.CATCH$setVarName(n2, t.token.value);
                    break;
                  default:
                    throw t.newSyntaxError("Missing identifier in catch");
                    break;
                }
                if (t.match(IF)) {
                    if (x.ecma3OnlyMode)
                        throw t.newSyntaxError("Illegal catch guard");
                    if (n.catchClauses.length && !n.catchClauses.top().guard)
                        throw t.newSyntaxError("Guarded catch after unguarded");
                    b.CATCH$setGuard(n2, Expression(t, x));
                } else {
                    b.CATCH$setGuard(n2, null);
                }
                t.mustMatch(RIGHT_PAREN);
                b.CATCH$setBlock(n2, Block(t, x));
                b.CATCH$finish(n2);
                b.TRY$addCatch(n, n2);
            }
            b.TRY$finishCatches(n);
            if (t.match(FINALLY))
                b.TRY$setFinallyBlock(n, Block(t, x));
            if (!n.catchClauses.length && !n.finallyBlock)
                throw t.newSyntaxError("Invalid try statement");
            b.TRY$finish(n);
            return n;
    
          case CATCH:
          case FINALLY:
            throw t.newSyntaxError(jsdefs.tokens[tt] + " without preceding try");
    
          case THROW:
            n = b.THROW$build(t);
            b.THROW$setException(n, Expression(t, x));
            b.THROW$finish(n);
            break;
    
          case RETURN:
            n = returnOrYield(t, x);
            break;
    
          case WITH:
            n = b.WITH$build(t);
            b.WITH$setObject(n, ParenExpression(t, x));
            b.WITH$setBody(n, nest(t, x, n, Statement));
            b.WITH$finish(n);
            return n;
    
          case VAR:
          case CONST:
            n = Variables(t, x);
            break;
    
          case LET:
            if (t.peek() == LEFT_PAREN)
                n = LetBlock(t, x, true);
            else
                n = Variables(t, x);
            break;
    
          case DEBUGGER:
            n = b.DEBUGGER$build(t);
            break;
    
          case NEWLINE:
          case SEMICOLON:
            n = b.SEMICOLON$build(t);
            b.SEMICOLON$setExpression(n, null);
            b.SEMICOLON$finish(t);
            return n;
    
          default:
            if (tt == IDENTIFIER) {
                tt = t.peek();
                // Labeled statement.
                if (tt == COLON) {
                    label = t.token.value;
                    ss = x.stmtStack;
                    for (i = ss.length-1; i >= 0; --i) {
                        if (ss[i].label == label)
                            throw t.newSyntaxError("Duplicate label");
                    }
                    t.get();
                    n = b.LABEL$build(t);
                    b.LABEL$setLabel(n, label)
                    b.LABEL$setStatement(n, nest(t, x, n, Statement));
                    b.LABEL$finish(n);
                    return n;
                }
            }
    
            // Expression statement.
            // We unget the current token to parse the expression as a whole.
            n = b.SEMICOLON$build(t);
            t.unget();
            b.SEMICOLON$setExpression(n, Expression(t, x));
            n.end = n.expression.end;
            b.SEMICOLON$finish(n);
            break;
        }
    
        MagicalSemicolon(t);
        return n;
    }
    
    function MagicalSemicolon(t) {
        var tt;
        if (t.lineno == t.token.lineno) {
            tt = t.peekOnSameLine();
            if (tt != END && tt != NEWLINE && tt != SEMICOLON && tt != RIGHT_CURLY)
                throw t.newSyntaxError("Missing ; before statement");
        }
        t.match(SEMICOLON);
    }
    
    function returnOrYield(t, x) {
        var n, b = x.builder, tt = t.token.type, tt2;
    
        if (tt == RETURN) {
            if (!x.inFunction)
                throw t.newSyntaxError("Return not in function");
            n = b.RETURN$build(t);
        } else /* (tt == YIELD) */ {
            if (!x.inFunction)
                throw t.newSyntaxError("Yield not in function");
            x.isGenerator = true;
            n = b.YIELD$build(t);
        }
    
        tt2 = t.peek(true);
        if (tt2 != END && tt2 != NEWLINE && tt2 != SEMICOLON && tt2 != RIGHT_CURLY
            && (tt != YIELD ||
                (tt2 != tt && tt2 != RIGHT_BRACKET && tt2 != RIGHT_PAREN &&
                 tt2 != COLON && tt2 != COMMA))) {
            if (tt == RETURN) {
                b.RETURN$setValue(n, Expression(t, x));
                x.hasReturnWithValue = true;
            } else {
                b.YIELD$setValue(n, AssignExpression(t, x));
            }
        } else if (tt == RETURN) {
            x.hasEmptyReturn = true;
        }
    
        // Disallow return v; in generator.
        if (x.hasReturnWithValue && x.isGenerator)
            throw t.newSyntaxError("Generator returns a value");
    
        if (tt == RETURN)
            b.RETURN$finish(n);
        else
            b.YIELD$finish(n);
    
        return n;
    }
    
    /*
     * FunctionDefinition :: (tokenizer, compiler context, boolean,
     *                        DECLARED_FORM or EXPRESSED_FORM or STATEMENT_FORM)
     *                    -> node
     */
    function FunctionDefinition(t, x, requireName, functionForm) {
        var b = x.builder;
        var f = b.FUNCTION$build(t);
        if (t.match(IDENTIFIER))
            b.FUNCTION$setName(f, t.token.value);
        else if (requireName)
            throw t.newSyntaxError("Missing function identifier");
    
        t.mustMatch(LEFT_PAREN);
        if (!t.match(RIGHT_PAREN)) {
            do {
                switch (t.get()) {
                  case LEFT_BRACKET:
                  case LEFT_CURLY:
                    // Destructured formal parameters.
                    t.unget();
                    b.FUNCTION$addParam(f, DestructuringExpression(t, x));
                    break;
                  case IDENTIFIER:
                    b.FUNCTION$addParam(f, t.token.value);
                    break;
                  default:
                    throw t.newSyntaxError("Missing formal parameter");
                    break;
                }
            } while (t.match(COMMA));
            t.mustMatch(RIGHT_PAREN);
        }
    
        // Do we have an expression closure or a normal body?
        var tt = t.get();
        if (tt != LEFT_CURLY)
            t.unget();
    
        var x2 = new CompilerContext(true, b);
        var rp = t.save();
        if (x.inFunction) {
            /*
             * Inner functions don't reset block numbering. They also need to
             * remember which block they were parsed in for hoisting (see comment
             * below).
             */
            x2.blockId = x.blockId;
        }
    
        if (tt != LEFT_CURLY) {
            b.FUNCTION$setBody(f, AssignExpression(t, x));
            if (x.isGenerator)
                throw t.newSyntaxError("Generator returns a value");
        } else {
            b.FUNCTION$hoistVars(x2.blockId);
            b.FUNCTION$setBody(f, Script(t, x2));
        }
    
        /*
         * To linearize hoisting with nested blocks needing hoists, if a toplevel
         * function has any hoists we reparse the entire thing. Each toplevel
         * function is parsed at most twice.
         *
         * Pass 1: If there needs to be hoisting at any child block or inner
         * function, the entire function gets reparsed.
         *
         * Pass 2: It's possible that hoisting has changed the upvars of
         * functions. That is, consider:
         *
         * function f() {
         *   x = 0;
         *   g();
         *   x; // x's forward pointer should be invalidated!
         *   function g() {
         *     x = 'g';
         *   }
         *   var x;
         * }
         *
         * So, a function needs to remember in which block it is parsed under
         * (since the function body is _not_ hoisted, only the declaration) and
         * upon hoisting, needs to recalculate all its upvars up front.
         */
        if (x2.needsHoisting) {
            // Order is important here! funDecls must come _after_ varDecls!
            b.setHoists(f.body.id, x2.varDecls.concat(x2.funDecls));
    
            if (x.inFunction) {
                // Propagate up to the parent function if we're an inner function.
                x.needsHoisting = true;
            } else {
                // Only re-parse toplevel functions.
                var x3 = x2;
                x2 = new CompilerContext(true, b);
                t.rewind(rp);
                // Set a flag in case the builder wants to have different behavior
                // on the second pass.
                b.secondPass = true;
                b.FUNCTION$hoistVars(f.body.id, true);
                b.FUNCTION$setBody(f, Script(t, x2));
                b.secondPass = false;
            }
        }
    
        if (tt == LEFT_CURLY)
            t.mustMatch(RIGHT_CURLY);
    
        f.end = t.token.end;
        f.functionForm = functionForm;
        if (functionForm == DECLARED_FORM)
            x.funDecls.push(f);
        b.FUNCTION$finish(f, x);
        return f;
    }
    
    /*
     * Variables :: (tokenizer, compiler context) -> node
     *
     * Parses a comma-separated list of var declarations (and maybe
     * initializations).
     */
    function Variables(t, x, letBlock) {
        var b = x.builder;
        var n, ss, i, s;
        var build, addDecl, finish;
        switch (t.token.type) {
          case VAR:
            build = b.VAR$build;
            addDecl = b.VAR$addDecl;
            finish = b.VAR$finish;
            s = x;
            break;
          case CONST:
            build = b.CONST$build;
            addDecl = b.CONST$addDecl;
            finish = b.CONST$finish;
            s = x;
            break;
          case LET:
          case LEFT_PAREN:
            build = b.LET$build;
            addDecl = b.LET$addDecl;
            finish = b.LET$finish;
            if (!letBlock) {
                ss = x.stmtStack;
                i = ss.length;
                while (ss[--i].type !== BLOCK) ; // a BLOCK *must* be found.
                /*
                 * Lets at the function toplevel are just vars, at least in
                 * SpiderMonkey.
                 */
                if (i == 0) {
                    build = b.VAR$build;
                    addDecl = b.VAR$addDecl;
                    finish = b.VAR$finish;
                    s = x;
                } else {
                    s = ss[i];
                }
            } else {
                s = letBlock;
            }
            break;
        }
        n = build.call(b, t);
        initializers = [];
        do {
            var tt = t.get();
            /*
             * FIXME Should have a special DECLARATION node instead of overloading
             * IDENTIFIER to mean both identifier declarations and destructured
             * declarations.
             */
            var n2 = b.DECL$build(t);
            if (tt == LEFT_BRACKET || tt == LEFT_CURLY) {
                // Pass in s if we need to add each pattern matched into
                // its varDecls, else pass in x.
                var data = null;
                // Need to unget to parse the full destructured expression.
                t.unget();
                b.DECL$setName(n2, DestructuringExpression(t, x, true, s));
                if (x.inForLoopInit && t.peek() == IN) {
                    addDecl.call(b, n, n2, s);
                    continue;
                }
    
                t.mustMatch(ASSIGN);
                if (t.token.assignOp)
                    throw t.newSyntaxError("Invalid variable initialization");
    
                // Parse the init as a normal assignment.
                var n3 = b.ASSIGN$build(t);
                b.ASSIGN$addOperand(n3, n2.name);
                b.ASSIGN$addOperand(n3, AssignExpression(t, x));
                b.ASSIGN$finish(n3);
    
                // But only add the rhs as the initializer.
                b.DECL$setInitializer(n2, n3[1]);
                b.DECL$finish(n2);
                addDecl.call(b, n, n2, s);
                continue;
            }
    
            if (tt != IDENTIFIER)
                throw t.newSyntaxError("Missing variable name");
    
            b.DECL$setName(n2, t.token.value);
            b.DECL$setReadOnly(n2, n.type == CONST);
            addDecl.call(b, n, n2, s);
    
            if (t.match(ASSIGN)) {
                if (t.token.assignOp)
                    throw t.newSyntaxError("Invalid variable initialization");
    
                // Parse the init as a normal assignment with a fake lhs.
                var id = new Node(n2.tokenizer, IDENTIFIER);
                var n3 = b.ASSIGN$build(t);
                id.name = id.value = n2.name;
                b.ASSIGN$addOperand(n3, id);
                b.ASSIGN$addOperand(n3, AssignExpression(t, x));
                b.ASSIGN$finish(n3);
                initializers.push(n3);
    
                // But only add the rhs as the initializer.
                b.DECL$setInitializer(n2, n3[1]);
            }
    
            b.DECL$finish(n2);
            s.varDecls.push(n2);
        } while (t.match(COMMA));
        finish.call(b, n);
        return n;
    }
    
    /*
     * LetBlock :: (tokenizer, compiler context, boolean) -> node
     *
     * Does not handle let inside of for loop init.
     */
    function LetBlock(t, x, isStatement) {
        var n, n2, binds;
        var b = x.builder;
    
        // t.token.type must be LET
        n = b.LET_BLOCK$build(t);
        t.mustMatch(LEFT_PAREN);
        b.LET_BLOCK$setVariables(n, Variables(t, x, n));
        t.mustMatch(RIGHT_PAREN);
    
        if (isStatement && t.peek() != LEFT_CURLY) {
            /*
             * If this is really an expression in let statement guise, then we
             * need to wrap the LET_BLOCK node in a SEMICOLON node so that we pop
             * the return value of the expression.
             */
            n2 = b.SEMICOLON$build(t);
            b.SEMICOLON$setExpression(n2, n);
            b.SEMICOLON$finish(n2);
            isStatement = false;
        }
    
        if (isStatement) {
            n2 = Block(t, x);
            b.LET_BLOCK$setBlock(n, n2);
        } else {
            n2 = AssignExpression(t, x);
            b.LET_BLOCK$setExpression(n, n2);
        }
    
        b.LET_BLOCK$finish(n);
    
        return n;
    }
    
    function checkDestructuring(t, x, n, simpleNamesOnly, data) {
        if (n.type == ARRAY_COMP)
            throw t.newSyntaxError("Invalid array comprehension left-hand side");
        if (n.type != ARRAY_INIT && n.type != OBJECT_INIT)
            return;
    
        var b = x.builder;
    
        for (var i = 0, j = n.length; i < j; i++) {
            var nn = n[i], lhs, rhs;
            if (!nn)
                continue;
            if (nn.type == PROPERTY_INIT)
                lhs = nn[0], rhs = nn[1];
            else
                lhs = null, rhs = null;
            if (rhs && (rhs.type == ARRAY_INIT || rhs.type == OBJECT_INIT))
                checkDestructuring(t, x, rhs, simpleNamesOnly, data);
            if (lhs && simpleNamesOnly) {
                // In declarations, lhs must be simple names
                if (lhs.type != IDENTIFIER) {
                    throw t.newSyntaxError("Missing name in pattern");
                } else if (data) {
                    var n2 = b.DECL$build(t);
                    b.DECL$setName(n2, lhs.value);
                    // Don't need to set initializer because it's just for
                    // hoisting anyways.
                    b.DECL$finish(n2);
                    // Each pattern needs to be added to varDecls.
                    data.varDecls.push(n2);
                }
            }
        }
    }
    
    function DestructuringExpression(t, x, simpleNamesOnly, data) {
        var n = PrimaryExpression(t, x);
        checkDestructuring(t, x, n, simpleNamesOnly, data);
        return n;
    }
    
    function GeneratorExpression(t, x, e) {
        var n;
    
        n = b.GENERATOR$build(t);
        b.GENERATOR$setExpression(n, e);
        b.GENERATOR$setTail(n, comprehensionTail(t, x));
        b.GENERATOR$finish(n);
    
        return n;
    }
    
    function comprehensionTail(t, x) {
        var body, n;
        var b = x.builder;
        // t.token.type must be FOR
        body = b.COMP_TAIL$build(t);
    
        do {
            n = b.FOR$build(t);
            // Comprehension tails are always for..in loops.
            b.FOR$rebuildForIn(n);
            if (t.match(IDENTIFIER)) {
                // But sometimes they're for each..in.
                if (t.token.value == "each")
                    b.FOR$rebuildForEach(n);
                else
                    t.unget();
            }
            t.mustMatch(LEFT_PAREN);
            switch(t.get()) {
              case LEFT_BRACKET:
              case LEFT_CURLY:
                t.unget();
                // Destructured left side of for in comprehension tails.
                b.FOR$setIterator(n, DestructuringExpression(t, x), null);
                break;
    
              case IDENTIFIER:
                var n3 = b.DECL$build(t);
                b.DECL$setName(n3, n3.value);
                b.DECL$finish(n3);
                var n2 = b.VAR$build(t);
                b.VAR$addDecl(n2, n3);
                b.VAR$finish(n2);
                b.FOR$setIterator(n, n3, n2);
                /*
                 * Don't add to varDecls since the semantics of comprehensions is
                 * such that the variables are in their own function when
                 * desugared.
                 */
                break;
    
              default:
                throw t.newSyntaxError("Missing identifier");
            }
            t.mustMatch(IN);
            b.FOR$setObject(n, Expression(t, x));
            t.mustMatch(RIGHT_PAREN);
            b.COMP_TAIL$addFor(body, n);
        } while (t.match(FOR));
    
        // Optional guard.
        if (t.match(IF))
            b.COMP_TAIL$setGuard(body, ParenExpression(t, x));
    
        b.COMP_TAIL$finish(body);
        return body;
    }
    
    function ParenExpression(t, x) {
        t.mustMatch(LEFT_PAREN);
    
        /*
         * Always accept the 'in' operator in a parenthesized expression,
         * where it's unambiguous, even if we might be parsing the init of a
         * for statement.
         */
        var oldLoopInit = x.inForLoopInit;
        x.inForLoopInit = false;
        var n = Expression(t, x);
        x.inForLoopInit = oldLoopInit;
    
        var err = "expression must be parenthesized";
        if (t.match(FOR)) {
            if (n.type == YIELD && !n.parenthesized)
                throw t.newSyntaxError("Yield " + err);
            if (n.type == COMMA && !n.parenthesized)
                throw t.newSyntaxError("Generator " + err);
            n = GeneratorExpression(t, x, n);
        }
    
        t.mustMatch(RIGHT_PAREN);
    
        return n;
    }
    
    /*
     * Expression: (tokenizer, compiler context) -> node
     *
     * Top-down expression parser matched against SpiderMonkey.
     */
    function Expression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = AssignExpression(t, x);
        if (t.match(COMMA)) {
            n2 = b.COMMA$build(t);
            b.COMMA$addOperand(n2, n);
            n = n2;
            do {
                n2 = n[n.length-1];
                if (n2.type == YIELD && !n2.parenthesized)
                    throw t.newSyntaxError("Yield expression must be parenthesized");
                b.COMMA$addOperand(n, AssignExpression(t, x));
            } while (t.match(COMMA));
            b.COMMA$finish(n);
        }
    
        return n;
    }
    
    function AssignExpression(t, x) {
        var n, lhs;
        var b = x.builder;
    
        // Have to treat yield like an operand because it could be the leftmost
        // operand of the expression.
        if (t.match(YIELD, true))
            return returnOrYield(t, x);
    
        n = b.ASSIGN$build(t);
        lhs = ConditionalExpression(t, x);
    
        if (!t.match(ASSIGN)) {
            b.ASSIGN$finish(n);
            return lhs;
        }
    
        switch (lhs.type) {
          case OBJECT_INIT:
          case ARRAY_INIT:
            checkDestructuring(t, x, lhs);
            // FALL THROUGH
          case IDENTIFIER: case DOT: case INDEX: case CALL:
            break;
          default:
            throw t.newSyntaxError("Bad left-hand side of assignment");
            break;
        }
    
        b.ASSIGN$setAssignOp(n, t.token.assignOp);
        b.ASSIGN$addOperand(n, lhs);
        b.ASSIGN$addOperand(n, AssignExpression(t, x));
        b.ASSIGN$finish(n);
    
        return n;
    }
    
    function ConditionalExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = OrExpression(t, x);
        if (t.match(HOOK)) {
            n2 = n;
            n = b.HOOK$build(t);
            b.HOOK$setCondition(n, n2);
            /*
             * Always accept the 'in' operator in the middle clause of a ternary,
             * where it's unambiguous, even if we might be parsing the init of a
             * for statement.
             */
            var oldLoopInit = x.inForLoopInit;
            x.inForLoopInit = false;
            b.HOOK$setThenPart(n, AssignExpression(t, x));
            x.inForLoopInit = oldLoopInit;
            if (!t.match(COLON))
                throw t.newSyntaxError("Missing : after ?");
            b.HOOK$setElsePart(n, AssignExpression(t, x));
            b.HOOK$finish(n);
        }
    
        return n;
    }
    
    function OrExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = AndExpression(t, x);
        while (t.match(OR)) {
            n2 = b.OR$build(t);
            b.OR$addOperand(n2, n);
            b.OR$addOperand(n2, AndExpression(t, x));
            b.OR$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function AndExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = BitwiseOrExpression(t, x);
        while (t.match(AND)) {
            n2 = b.AND$build(t);
            b.AND$addOperand(n2, n);
            b.AND$addOperand(n2, BitwiseOrExpression(t, x));
            b.AND$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function BitwiseOrExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = BitwiseXorExpression(t, x);
        while (t.match(BITWISE_OR)) {
            n2 = b.BITWISE_OR$build(t);
            b.BITWISE_OR$addOperand(n2, n);
            b.BITWISE_OR$addOperand(n2, BitwiseXorExpression(t, x));
            b.BITWISE_OR$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function BitwiseXorExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = BitwiseAndExpression(t, x);
        while (t.match(BITWISE_XOR)) {
            n2 = b.BITWISE_XOR$build(t);
            b.BITWISE_XOR$addOperand(n2, n);
            b.BITWISE_XOR$addOperand(n2, BitwiseAndExpression(t, x));
            b.BITWISE_XOR$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function BitwiseAndExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = EqualityExpression(t, x);
        while (t.match(BITWISE_AND)) {
            n2 = b.BITWISE_AND$build(t);
            b.BITWISE_AND$addOperand(n2, n);
            b.BITWISE_AND$addOperand(n2, EqualityExpression(t, x));
            b.BITWISE_AND$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function EqualityExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = RelationalExpression(t, x);
        while (t.match(EQ) || t.match(NE) ||
               t.match(STRICT_EQ) || t.match(STRICT_NE)) {
            n2 = b.EQUALITY$build(t);
            b.EQUALITY$addOperand(n2, n);
            b.EQUALITY$addOperand(n2, RelationalExpression(t, x));
            b.EQUALITY$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function RelationalExpression(t, x) {
        var n, n2;
        var b = x.builder;
        var oldLoopInit = x.inForLoopInit;
    
        /*
         * Uses of the in operator in shiftExprs are always unambiguous,
         * so unset the flag that prohibits recognizing it.
         */
        x.inForLoopInit = false;
        n = ShiftExpression(t, x);
        while ((t.match(LT) || t.match(LE) || t.match(GE) || t.match(GT) ||
               (oldLoopInit == false && t.match(IN)) ||
               t.match(INSTANCEOF))) {
            n2 = b.RELATIONAL$build(t);
            b.RELATIONAL$addOperand(n2, n);
            b.RELATIONAL$addOperand(n2, ShiftExpression(t, x));
            b.RELATIONAL$finish(n2);
            n = n2;
        }
        x.inForLoopInit = oldLoopInit;
    
        return n;
    }
    
    function ShiftExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = AddExpression(t, x);
        while (t.match(LSH) || t.match(RSH) || t.match(URSH)) {
            n2 = b.SHIFT$build(t);
            b.SHIFT$addOperand(n2, n);
            b.SHIFT$addOperand(n2, AddExpression(t, x));
            b.SHIFT$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function AddExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = MultiplyExpression(t, x);
        while (t.match(PLUS) || t.match(MINUS)) {
            n2 = b.ADD$build(t);
            b.ADD$addOperand(n2, n);
            b.ADD$addOperand(n2, MultiplyExpression(t, x));
            b.ADD$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function MultiplyExpression(t, x) {
        var n, n2;
        var b = x.builder;
    
        n = UnaryExpression(t, x);
        while (t.match(MUL) || t.match(DIV) || t.match(MOD)) {
            n2 = b.MULTIPLY$build(t);
            b.MULTIPLY$addOperand(n2, n);
            b.MULTIPLY$addOperand(n2, UnaryExpression(t, x));
            b.MULTIPLY$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function UnaryExpression(t, x) {
        var n, n2, tt;
        var b = x.builder;
    
        switch (tt = t.get(true)) {
          case DELETE: case VOID: case TYPEOF:
          case NOT: case BITWISE_NOT: case PLUS: case MINUS:
            n = b.UNARY$build(t);
            b.UNARY$addOperand(n, UnaryExpression(t, x));
            break;
    
          case INCREMENT:
          case DECREMENT:
            // Prefix increment/decrement.
            n = b.UNARY$build(t)
            b.UNARY$addOperand(n, MemberExpression(t, x, true));
            break;
    
          default:
            t.unget();
            n = MemberExpression(t, x, true);
    
            // Don't look across a newline boundary for a postfix {in,de}crement.
            if (t.tokens[(t.tokenIndex + t.lookahead - 1) & 3].lineno ==
                t.lineno) {
                if (t.match(INCREMENT) || t.match(DECREMENT)) {
                    n2 = b.UNARY$build(t);
                    b.UNARY$setPostfix(n2);
                    b.UNARY$finish(n);
                    b.UNARY$addOperand(n2, n);
                    n = n2;
                }
            }
            break;
        }
    
        b.UNARY$finish(n);
        return n;
    }
    
    function MemberExpression(t, x, allowCallSyntax) {
        var n, n2, tt;
        var b = x.builder;
    
        if (t.match(NEW)) {
            n = b.MEMBER$build(t);
            b.MEMBER$addOperand(n, MemberExpression(t, x, false));
            if (t.match(LEFT_PAREN)) {
                b.MEMBER$rebuildNewWithArgs(n);
                b.MEMBER$addOperand(n, ArgumentList(t, x));
            }
            b.MEMBER$finish(n);
        } else {
            n = PrimaryExpression(t, x);
        }
    
        while ((tt = t.get()) != END) {
            switch (tt) {
              case DOT:
                n2 = b.MEMBER$build(t);
                b.MEMBER$addOperand(n2, n);
                t.mustMatch(IDENTIFIER);
                b.MEMBER$addOperand(n2, b.MEMBER$build(t));
                break;
    
              case LEFT_BRACKET:
                n2 = b.MEMBER$build(t, INDEX);
                b.MEMBER$addOperand(n2, n);
                b.MEMBER$addOperand(n2, Expression(t, x));
                t.mustMatch(RIGHT_BRACKET);
                break;
    
              case LEFT_PAREN:
                if (allowCallSyntax) {
                    n2 = b.MEMBER$build(t, CALL);
                    b.MEMBER$addOperand(n2, n);
                    b.MEMBER$addOperand(n2, ArgumentList(t, x));
                    break;
                }
    
                // FALL THROUGH
              default:
                t.unget();
                return n;
            }
    
            b.MEMBER$finish(n2);
            n = n2;
        }
    
        return n;
    }
    
    function ArgumentList(t, x) {
        var n, n2;
        var b = x.builder;
        var err = "expression must be parenthesized";
    
        n = b.LIST$build(t);
        if (t.match(RIGHT_PAREN, true))
            return n;
        do {
            n2 = AssignExpression(t, x);
            if (n2.type == YIELD && !n2.parenthesized && t.peek() == COMMA)
                throw t.newSyntaxError("Yield " + err);
            if (t.match(FOR)) {
                n2 = GeneratorExpression(t, x, n2);
                if (n.length > 1 || t.peek(true) == COMMA)
                    throw t.newSyntaxError("Generator " + err);
            }
            b.LIST$addOperand(n, n2);
        } while (t.match(COMMA));
        t.mustMatch(RIGHT_PAREN);
        b.LIST$finish(n);
    
        return n;
    }
    
    function PrimaryExpression(t, x) {
        var n, n2, n3, tt = t.get(true);
        var b = x.builder;
    
        switch (tt) {
          case FUNCTION:
            n = FunctionDefinition(t, x, false, EXPRESSED_FORM);
            break;
    
          case LEFT_BRACKET:
            n = b.ARRAY_INIT$build(t);
            while ((tt = t.peek()) != RIGHT_BRACKET) {
                if (tt == COMMA) {
                    t.get();
                    b.ARRAY_INIT$addElement(n, null);
                    continue;
                }
                b.ARRAY_INIT$addElement(n, AssignExpression(t, x));
                if (tt != COMMA && !t.match(COMMA))
                    break;
            }
    
            // If we matched exactly one element and got a FOR, we have an
            // array comprehension.
            if (n.length == 1 && t.match(FOR)) {
                n2 = b.ARRAY_COMP$build(t);
                b.ARRAY_COMP$setExpression(n2, n[0]);
                b.ARRAY_COMP$setTail(n2, comprehensionTail(t, x));
                n = n2;
            }
            t.mustMatch(RIGHT_BRACKET);
            b.PRIMARY$finish(n);
            break;
    
          case LEFT_CURLY:
            var id;
            n = b.OBJECT_INIT$build(t);
    
          object_init:
            if (!t.match(RIGHT_CURLY)) {
                do {
                    tt = t.get();
                    if ((t.token.value == "get" || t.token.value == "set") &&
                        t.peek() == IDENTIFIER) {
                        if (x.ecma3OnlyMode)
                            throw t.newSyntaxError("Illegal property accessor");
                        var fd = FunctionDefinition(t, x, true, EXPRESSED_FORM);
                        b.OBJECT_INIT$addProperty(n, fd);
                    } else {
                        switch (tt) {
                          case IDENTIFIER: case NUMBER: case STRING:
                            id = b.PRIMARY$build(t, IDENTIFIER);
                            b.PRIMARY$finish(id);
                            break;
                          case RIGHT_CURLY:
                            if (x.ecma3OnlyMode)
                                throw t.newSyntaxError("Illegal trailing ,");
                            break object_init;
                          default:
                            if (t.token.value in jsdefs.keywords) {
                                id = b.PRIMARY$build(t, IDENTIFIER);
                                b.PRIMARY$finish(id);
                                break;
                            }
                            throw t.newSyntaxError("Invalid property name");
                        }
                        if (t.match(COLON)) {
                            n2 = b.PROPERTY_INIT$build(t);
                            b.PROPERTY_INIT$addOperand(n2, id);
                            b.PROPERTY_INIT$addOperand(n2, AssignExpression(t, x));
                            b.PROPERTY_INIT$finish(n2);
                            b.OBJECT_INIT$addProperty(n, n2);
                        } else {
                            // Support, e.g., |var {x, y} = o| as destructuring shorthand
                            // for |var {x: x, y: y} = o|, per proposed JS2/ES4 for JS1.8.
                            if (t.peek() != COMMA && t.peek() != RIGHT_CURLY)
                                throw t.newSyntaxError("Missing : after property");
                            b.OBJECT_INIT$addProperty(n, id);
                        }
                    }
                } while (t.match(COMMA));
                t.mustMatch(RIGHT_CURLY);
            }
            b.OBJECT_INIT$finish(n);
            break;
    
          case LEFT_PAREN:
            // ParenExpression does its own matching on parentheses, so we need to
            // unget.
            t.unget();
            n = ParenExpression(t, x);
            n.parenthesized = true;
            break;
    
          case LET:
            n = LetBlock(t, x, false);
            break;
    
          case NULL: case THIS: case TRUE: case FALSE:
          case IDENTIFIER: case NUMBER: case STRING: case REGEXP:
            n = b.PRIMARY$build(t);
            b.PRIMARY$finish(n);
            break;
    
          default:
            throw t.newSyntaxError("Missing operand");
            break;
        }
    
        return n;
    }
    
    /*
     * parse :: (builder, file ptr, path, line number) -> node
     */
    function parse(b, s, f, l) {
        var t = new jslex.Tokenizer(s, f, l);
        var x = new CompilerContext(false, b);
        var n = Script(t, x);
        if (!t.done)
            throw t.newSyntaxError("Syntax error");
    
        return n;
    }
    
    return {
        "parse": parse,
        "VanillaBuilder": VanillaBuilder,
        "DECLARED_FORM": DECLARED_FORM,
        "EXPRESSED_FORM": EXPRESSED_FORM,
        "STATEMENT_FORM": STATEMENT_FORM,
        "Tokenizer": jslex.Tokenizer,
        "FunctionDefinition": FunctionDefinition
    };

}());
