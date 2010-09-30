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

Narcissus.parser = (function() {

    var lexer = Narcissus.lexer;
    var definitions = Narcissus.definitions;

    // Set constants in the local scope.
    eval(definitions.consts);

    /*
     * Function.prototype.bind is not yet implemented in many browsers.
     * The following definition will be removed when it is.
     *
     * Similar to Prototype's implementation.
     */

    function bindMethod(method, context) {
        if (arguments.length < 3 && arguments[0] === undefined)
            return method;
        var slice = Array.prototype.slice;
        var args = slice.call(arguments, 2);
        // Optimization for when there's no currying.
        if (args.length === 0) {
            return function() {
                return method.apply(context, arguments);
            }
        }

        return function() {
            var a = slice.call(args, 0);
            for (var i = 0, j = arguments.length; i < j; i++) {
                a.push(arguments[i]);
            }
            return method.apply(context, a);
        }
    }

    function bindSubBuilders(builder, proto) {
        for (var ns in proto) {
            var unbound = proto[ns];
            // We do not want to bind functions like setHoists.
            if (typeof unbound !== "object")
                continue;

            // We store the bound sub-builder as builder's own property
            // so that we can have multiple builders at the same time.
            var bound = builder[ns] = {};
            for (var m in unbound) {
                bound[m] = bindMethod(unbound[m], builder);
            }
        }
    }

    /*
     * The vanilla AST builder.
     */

    function DefaultBuilder() {
        bindSubBuilders(this, DefaultBuilder.prototype);
    }

    function pushDestructuringVarDecls(n, x) {
        for (var i in n) {
            var sub = n[i];
            if (sub.type === IDENTIFIER) {
                x.varDecls.push(sub);
            } else {
                pushDestructuringVarDecls(sub, x);
            }
        }
    }

    function mkBinopBuilder(type) {
        return {
            build: !type ? function(t) { return new Node(t); }
                          : function(t) { return new Node(t, type); },
            addOperand: function(n, n2) { n.push(n2); },
            finish: function(n) { }
        };
    }

    DefaultBuilder.prototype = {
        IF: {
            build: function(t) {
                return new Node(t, IF);
            },

            setCondition: function(n, e) {
                n.condition = e;
            },

            setThenPart: function(n, s) {
                n.thenPart = s;
            },

            setElsePart: function(n, s) {
                n.elsePart = s;
            },

            finish: function(n) {
            }
        },

        SWITCH: {
            build: function(t) {
                var n = new Node(t, SWITCH);
                n.cases = [];
                n.defaultIndex = -1;
                return n;
            },

            setDiscriminant: function(n, e) {
                n.discriminant = e;
            },

            setDefaultIndex: function(n, i) {
                n.defaultIndex = i;
            },

            addCase: function(n, n2) {
                n.cases.push(n2);
            },

            finish: function(n) {
            }
        },

        CASE: {
            build: function(t) {
                return new Node(t, CASE);
            },

            setLabel: function(n, e) {
                n.caseLabel = e;
            },

            initializeStatements: function(n, t) {
                n.statements = new Node(t, BLOCK);
            },

            addStatement: function(n, s) {
                n.statements.push(s);
            },

            finish: function(n) {
            }
        },

        DEFAULT: {
            build: function(t, p) {
                return new Node(t, DEFAULT);
            },

            initializeStatements: function(n, t) {
                n.statements = new Node(t, BLOCK);
            },

            addStatement: function(n, s) {
                n.statements.push(s);
            },

            finish: function(n) {
            }
        },

        FOR: {
            build: function(t) {
                var n = new Node(t, FOR);
                n.isLoop = true;
                n.isEach = false;
                return n;
            },

            rebuildForEach: function(n) {
                n.isEach = true;
            },

            // NB. This function is called after rebuildForEach, if that's called
            // at all.
            rebuildForIn: function(n) {
                n.type = FOR_IN;
            },

            setCondition: function(n, e) {
                n.condition = e;
            },

            setSetup: function(n, e) {
                n.setup = e || null;
            },

            setUpdate: function(n, e) {
                n.update = e;
            },

            setObject: function(n, e) {
                n.object = e;
            },

            setIterator: function(n, e, e2) {
                n.iterator = e;
                n.varDecl = e2;
            },

            setBody: function(n, s) {
                n.body = s;
            },

            finish: function(n) {
            }
        },

        WHILE: {
            build: function(t) {
                var n = new Node(t, WHILE);
                n.isLoop = true;
                return n;
            },

            setCondition: function(n, e) {
                n.condition = e;
            },

            setBody: function(n, s) {
                n.body = s;
            },

            finish: function(n) {
            }
        },

        DO: {
            build: function(t) {
                var n = new Node(t, DO);
                n.isLoop = true;
                return n;
            },

            setCondition: function(n, e) {
                n.condition = e;
            },

            setBody: function(n, s) {
                n.body = s;
            },

            finish: function(n) {
            }
        },

        BREAK: {
            build: function(t) {
                return new Node(t, BREAK);
            },

            setLabel: function(n, v) {
                n.label = v;
            },

            setTarget: function(n, n2) {
                n.target = n2;
            },

            finish: function(n) {
            }
        },

        CONTINUE: {
            build: function(t) {
                return new Node(t, CONTINUE);
            },

            setLabel: function(n, v) {
                n.label = v;
            },

            setTarget: function(n, n2) {
                n.target = n2;
            },

            finish: function(n) {
            }
        },

        TRY: {
            build: function(t) {
                var n = new Node(t, TRY);
                n.catchClauses = [];
                return n;
            },

            setTryBlock: function(n, s) {
                n.tryBlock = s;
            },

            addCatch: function(n, n2) {
                n.catchClauses.push(n2);
            },

            finishCatches: function(n) {
            },

            setFinallyBlock: function(n, s) {
                n.finallyBlock = s;
            },

            finish: function(n) {
            }
        },

        CATCH: {
            build: function(t) {
                var n = new Node(t, CATCH);
                n.guard = null;
                return n;
            },

            setVarName: function(n, v) {
                n.varName = v;
            },

            setGuard: function(n, e) {
                n.guard = e;
            },

            setBlock: function(n, s) {
                n.block = s;
            },

            finish: function(n) {
            }
        },

        THROW: {
            build: function(t) {
                return new Node(t, THROW);
            },

            setException: function(n, e) {
                n.exception = e;
            },

            finish: function(n) {
            }
        },

        RETURN: {
            build: function(t) {
                var n = new Node(t, RETURN);
                n.value = undefined;
                return n;
            },

            setValue: function(n, e) {
                n.value = e;
            },

            finish: function(n) {
            }
        },

        YIELD: {
            build: function(t) {
                return new Node(t, YIELD);
            },

            setValue: function(n, e) {
                n.value = e;
            },

            finish: function(n) {
            }
        },

        GENERATOR: {
            build: function(t) {
                return new Node(t, GENERATOR);
            },

            setExpression: function(n, e) {
                n.expression = e;
            },

            setTail: function(n, n2) {
                n.tail = n2;
            },

            finish: function(n) {
            }
        },

        WITH: {
            build: function(t) {
                return new Node(t, WITH);
            },

            setObject: function(n, e) {
                n.object = e;
            },

            setBody: function(n, s) {
                n.body = s;
            },

            finish: function(n) {
            }
        },

        DEBUGGER: {
            build: function(t) {
                return new Node(t, DEBUGGER);
            }
        },

        SEMICOLON: {
            build: function(t) {
                return new Node(t, SEMICOLON);
            },

            setExpression: function(n, e) {
                n.expression = e;
            },

            finish: function(n) {
            }
        },

        LABEL: {
            build: function(t) {
                return new Node(t, LABEL);
            },

            setLabel: function(n, e) {
                n.label = e;
            },

            setStatement: function(n, s) {
                n.statement = s;
            },

            finish: function(n) {
            }
        },

        FUNCTION: {
            build: function(t) {
                var n = new Node(t);
                if (n.type !== FUNCTION)
                    n.type = (n.value === "get") ? GETTER : SETTER;
                n.params = [];
                return n;
            },

            setName: function(n, v) {
                n.name = v;
            },

            addParam: function(n, v) {
                n.params.push(v);
            },

            setBody: function(n, s) {
                n.body = s;
            },

            hoistVars: function(x) {
            },

            finish: function(n, x) {
            }
        },

        VAR: {
            build: function(t) {
                return new Node(t, VAR);
            },

            addDestructuringDecl: function(n, n2, x) {
                n.push(n2);
                pushDestructuringVarDecls(n2.name.destructuredNames, x);
            },

            addDecl: function(n, n2, x) {
                n.push(n2);
                x.varDecls.push(n2);
            },

            finish: function(n) {
            }
        },

        CONST: {
            build: function(t) {
                return new Node(t, CONST);
            },

            addDestructuringDecl: function(n, n2, x) {
                n.push(n2);
                pushDestructuringVarDecls(n2.name.destructuredNames, x);
            },

            addDecl: function(n, n2, x) {
                n.push(n2);
                x.varDecls.push(n2);
            },

            finish: function(n) {
            }
        },

        LET: {
            build: function(t) {
                return new Node(t, LET);
            },

            addDestructuringDecl: function(n, n2, x) {
                n.push(n2);
                pushDestructuringVarDecls(n2.name.destructuredNames, x);
            },

            addDecl: function(n, n2, x) {
                n.push(n2);
                x.varDecls.push(n2);
            },

            finish: function(n) {
            }
        },

        DECL: {
            build: function(t) {
                return new Node(t, IDENTIFIER);
            },

            setName: function(n, v) {
                n.name = v;
            },

            setInitializer: function(n, e) {
                n.initializer = e;
            },

            setReadOnly: function(n, b) {
                n.readOnly = b;
            },

            finish: function(n) {
            }
        },

        LET_BLOCK: {
            build: function(t) {
                var n = new Node(t, LET_BLOCK);
                n.varDecls = [];
                return n;
            },

            setVariables: function(n, n2) {
                n.variables = n2;
            },

            setExpression: function(n, e) {
                n.expression = e;
            },

            setBlock: function(n, s) {
                n.block = s;
            },

            finish: function(n) {
            }
        },

        BLOCK: {
            build: function(t, id) {
                var n = new Node(t, BLOCK);
                n.varDecls = [];
                n.id = id;
                return n;
            },

            hoistLets: function(n) {
            },

            addStatement: function(n, n2) {
                n.push(n2);
            },

            finish: function(n) {
            }
        },

        ASSIGN: {
            build: function(t) {
                return new Node(t, ASSIGN);
            },

            addOperand: function(n, n2) {
                n.push(n2);
            },

            setAssignOp: function(n, o) {
                n.assignOp = o;
            },

            finish: function(n) {
            }
        },

        HOOK: {
            build: function(t) {
                return new Node(t, HOOK);
            },

            setCondition: function(n, e) {
                n[0] = e;
            },

            setThenPart: function(n, n2) {
                n[1] = n2;
            },

            setElsePart: function(n, n2) {
                n[2] = n2;
            },

            finish: function(n) {
            }
        },

        OR: mkBinopBuilder(OR),
        AND: mkBinopBuilder(AND),
        BITWISE_OR: mkBinopBuilder(BITWISE_OR),
        BITWISE_XOR: mkBinopBuilder(BITWISE_XOR),
        BITWISE_AND: mkBinopBuilder(BITWISE_AND),
        EQUALITY: mkBinopBuilder(), // EQ | NE | STRICT_EQ | STRICT_NE
        RELATIONAL: mkBinopBuilder(), // LT | LE | GE | GT
        SHIFT: mkBinopBuilder(), // LSH | RSH | URSH
        ADD: mkBinopBuilder(), // PLUS | MINUS
        MULTIPLY: mkBinopBuilder(), // MUL | DIV | MOD

        UNARY: {
            // DELETE | VOID | TYPEOF | NOT | BITWISE_NOT
            // UNARY_PLUS | UNARY_MINUS | INCREMENT | DECREMENT
            build: function(t) {
                if (t.token.type === PLUS)
                    t.token.type = UNARY_PLUS;
                else if (t.token.type === MINUS)
                    t.token.type = UNARY_MINUS;
                return new Node(t);
            },

            addOperand: function(n, n2) {
                n.push(n2);
            },

            setPostfix: function(n) {
                n.postfix = true;
            },

            finish: function(n) {
            }
        },

        MEMBER: {
            // NEW | DOT | INDEX
            build: function(t, tt) {
                return new Node(t, tt);
            },

            rebuildNewWithArgs: function(n) {
                n.type = NEW_WITH_ARGS;
            },

            addOperand: function(n, n2) {
                n.push(n2);
            },

            finish: function(n) {
            }
        },

        PRIMARY: {
            build: function(t, tt) {
                // NULL | THIS | TRUE | FALSE | IDENTIFIER | NUMBER
                // STRING | REGEXP.
                return new Node(t, tt);
            },

            finish: function(n) {
            }
        },

        ARRAY_INIT: {
            build: function(t) {
                return new Node(t, ARRAY_INIT);
            },

            addElement: function(n, n2) {
                n.push(n2);
            },

            finish: function(n) {
            }
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

        COMP_TAIL: {
            build: function(t) {
                return new Node(t, COMP_TAIL);
            },

            setGuard: function(n, e) {
                n.guard = e;
            },

            addFor: function(n, n2) {
                n.push(n2);
            },

            finish: function(n) {
            }
        },

        OBJECT_INIT: {
            build: function(t) {
                return new Node(t, OBJECT_INIT);
            },

            addProperty: function(n, n2) {
                n.push(n2);
            },

            finish: function(n) {
            }
        },

        PROPERTY_NAME: {
            build: function(t) {
                return new Node(t, IDENTIFIER);
            },

            finish: function(n) {
            }
        },

        PROPERTY_INIT: {
            build: function(t) {
                return new Node(t, PROPERTY_INIT);
            },

            addOperand: function(n, n2) {
                n.push(n2);
            },

            finish: function(n) {
            }
        },

        COMMA: {
            build: function(t) {
                return new Node(t, COMMA);
            },

            addOperand: function(n, n2) {
                n.push(n2);
            },

            finish: function(n) {
            }
        },

        LIST: {
            build: function(t) {
                return new Node(t, LIST);
            },

            addOperand: function(n, n2) {
                n.push(n2);
            },

            finish: function(n) {
            }
        },

        setHoists: function(id, vds) {
        }
    };

    function StaticContext(inFunction, builder) {
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

    StaticContext.prototype = {
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
    definitions.defineProperty(Array.prototype, "top",
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
        var t = definitions.tokens[tt];
        return /^\W/.test(t) ? definitions.opTypeNames[t] : t.toUpperCase();
    }

    Np.toString = function () {
        var a = [];
        for (var i in this) {
            if (this.hasOwnProperty(i) && i !== 'type' && i !== 'target')
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

    definitions.defineGetter(Np, "filename",
                 function() {
                     return this.tokenizer.filename;
                 });

    definitions.defineProperty(String.prototype, "repeat",
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
        /*
         * Blocks are uniquely numbered by a blockId within a function that is
         * at the top level of the program. blockId starts from 0.
         *
         * This is done to aid hoisting for parse-time analyses done in custom
         * builders.
         *
         * For more details in its interaction with hoisting, see comments in
         * FunctionDefinition.
         */
        var builder = x.builder;
        var b = builder.BLOCK;
        var n = b.build(t, x.blockId++);
        b.hoistLets(n);
        x.stmtStack.push(n);
        while (!t.done && t.peek(true) !== RIGHT_CURLY)
            b.addStatement(n, Statement(t, x));
        x.stmtStack.pop();
        b.finish(n);
        if (n.needsHoisting) {
            builder.setHoists(n.id, n.varDecls);
            /*
             * If a block needs hoisting, we need to propagate this flag up to
             * the CompilerContext.
             */
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
        var builder = x.builder, b, b2, b3;

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
            b = builder.IF;
            n = b.build(t);
            b.setCondition(n, ParenExpression(t, x));
            x.stmtStack.push(n);
            b.setThenPart(n, Statement(t, x));
            if (t.match(ELSE))
                b.setElsePart(n, Statement(t, x));
            x.stmtStack.pop();
            b.finish(n);
            return n;

          case SWITCH:
            // This allows CASEs after a DEFAULT, which is in the standard.
            b = builder.SWITCH;
            b2 = builder.DEFAULT;
            b3 = builder.CASE;
            n = b.build(t);
            b.setDiscriminant(n, ParenExpression(t, x));
            x.stmtStack.push(n);
            t.mustMatch(LEFT_CURLY);
            while ((tt = t.get()) !== RIGHT_CURLY) {
                switch (tt) {
                  case DEFAULT:
                    if (n.defaultIndex >= 0)
                        throw t.newSyntaxError("More than one switch default");
                    n2 = b2.build(t);
                    b.setDefaultIndex(n, n.cases.length);
                    t.mustMatch(COLON);
                    b2.initializeStatements(n2, t);
                    while ((tt=t.peek(true)) !== CASE && tt !== DEFAULT &&
                           tt !== RIGHT_CURLY)
                        b2.addStatement(n2, Statement(t, x));
                    b2.finish(n2);
                    break;

                  case CASE:
                    n2 = b3.build(t);
                    b3.setLabel(n2, Expression(t, x, COLON));
                    t.mustMatch(COLON);
                    b3.initializeStatements(n2, t);
                    while ((tt=t.peek(true)) !== CASE && tt !== DEFAULT &&
                           tt !== RIGHT_CURLY)
                        b3.addStatement(n2, Statement(t, x));
                    b3.finish(n2);
                    break;

                  default:
                    throw t.newSyntaxError("Invalid switch case");
                }
                b.addCase(n, n2);
            }
            x.stmtStack.pop();
            b.finish(n);
            return n;

          case FOR:
            b = builder.FOR;
            n = b.build(t);
            if (t.match(IDENTIFIER) && t.token.value === "each")
                b.rebuildForEach(n);
            t.mustMatch(LEFT_PAREN);
            if ((tt = t.peek()) !== SEMICOLON) {
                x.inForLoopInit = true;
                if (tt === VAR || tt === CONST) {
                    t.get();
                    n2 = Variables(t, x);
                } else if (tt === LET) {
                    t.get();
                    if (t.peek() === LEFT_PAREN) {
                        n2 = LetBlock(t, x, false);
                    } else {
                        /*
                         * Let in for head, we need to add an implicit block
                         * around the rest of the for.
                         */
                        var forBlock = builder.BLOCK.build(t, x.blockId++);
                        x.stmtStack.push(forBlock);
                        n2 = Variables(t, x, forBlock);
                    }
                } else {
                    n2 = Expression(t, x);
                }
                x.inForLoopInit = false;
            }
            if (n2 && t.match(IN)) {
                // for-ins always get a for block to help desugaring.
                if (!forBlock) {
                    var forBlock = builder.BLOCK.build(t, x.blockId++);
                    forBlock.isInternalForInBlock = true;
                    x.stmtStack.push(forBlock);
                }

                b.rebuildForIn(n);
                b.setObject(n, Expression(t, x));
                if (n2.type === VAR || n2.type === LET) {
                    // Destructuring turns one decl into multiples, so either
                    // there must be only one destructuring or only one
                    // decl.
                    if (n2.length !== 1 && n2.destructurings.length !== 1) {
                        throw new SyntaxError("Invalid for..in left-hand side",
                                              t.filename, n2.lineno);
                    }
                    if (n2.destructurings.length > 0) {
                        b.setIterator(n, n2.destructurings[0], n2, forBlock);
                    } else {
                        b.setIterator(n, n2[0], n2, forBlock);
                    }
                } else {
                    if (n2.type === ARRAY_INIT || n2.type === OBJECT_INIT) {
                        n2.destructuredNames = checkDestructuring(t, x, n2);
                    }
                    b.setIterator(n, n2, null, forBlock);
                }
            } else {
                b.setSetup(n, n2);
                t.mustMatch(SEMICOLON);
                if (n.isEach)
                    throw t.newSyntaxError("Invalid for each..in loop");
                b.setCondition(n, (t.peek() === SEMICOLON)
                                  ? null
                                  : Expression(t, x));
                t.mustMatch(SEMICOLON);
                b.setUpdate(n, (t.peek() === RIGHT_PAREN)
                               ? null
                               : Expression(t, x));
            }
            t.mustMatch(RIGHT_PAREN);
            b.setBody(n, nest(t, x, n, Statement));
            b.finish(n);

            // In case desugaring statements were added to the imaginary
            // block.
            if (forBlock) {
                builder.BLOCK.finish(forBlock);
                x.stmtStack.pop();
                for (var i = 0, j = forBlock.length; i < j; i++) {
                    n.body.unshift(forBlock[i]);
                }
            }
            return n;

          case WHILE:
            b = builder.WHILE;
            n = b.build(t);
            b.setCondition(n, ParenExpression(t, x));
            b.setBody(n, nest(t, x, n, Statement));
            b.finish(n);
            return n;

          case DO:
            b = builder.DO;
            n = b.build(t);
            b.setBody(n, nest(t, x, n, Statement, WHILE));
            b.setCondition(n, ParenExpression(t, x));
            b.finish(n);
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
            b = (tt === BREAK) ? builder.BREAK : builder.CONTINUE;
            n = b.build(t);

            if (t.peekOnSameLine() === IDENTIFIER) {
                t.get();
                b.setLabel(n, t.token.value);
            }

            ss = x.stmtStack;
            i = ss.length;
            label = n.label;

            if (label) {
                do {
                    if (--i < 0)
                        throw t.newSyntaxError("Label not found");
                } while (ss[i].label !== label);

                /*
                 * Both break and continue to label need to be handled specially
                 * within a labeled loop, so that they target that loop. If not in
                 * a loop, then break targets its labeled statement. Labels can be
                 * nested so we skip all labels immediately enclosing the nearest
                 * non-label statement.
                 */
                while (i < ss.length - 1 && ss[i+1].type === LABEL)
                    i++;
                if (i < ss.length - 1 && ss[i+1].isLoop)
                    i++;
                else if (i < ss.length - 1 && ss[i+1].isInternalForInBlock
                                           && ss[i+2].isLoop)
                    i++;
                else if (tt === CONTINUE)
                    throw t.newSyntaxError("Invalid continue");
            } else {
                do {
                    if (--i < 0) {
                        throw t.newSyntaxError("Invalid " + ((tt === BREAK)
                                                             ? "break"
                                                             : "continue"));
                    }
                } while (!ss[i].isLoop && !(tt === BREAK && ss[i].type === SWITCH));
            }
            b.setTarget(n, ss[i]);
            b.finish(n);
            break;

          case TRY:
            b = builder.TRY;
            b2 = builder.CATCH;
            n = b.build(t);
            b.setTryBlock(n, Block(t, x));
            while (t.match(CATCH)) {
                n2 = b2.build(t);
                t.mustMatch(LEFT_PAREN);
                switch (t.get()) {
                  case LEFT_BRACKET:
                  case LEFT_CURLY:
                    // Destructured catch identifiers.
                    t.unget();
                    b2.setVarName(n2, DestructuringExpressionNoHoist(t, x, true));
                    break;
                  case IDENTIFIER:
                    b2.setVarName(n2, t.token.value);
                    break;
                  default:
                    throw t.newSyntaxError("missing identifier in catch");
                    break;
                }
                if (t.match(IF)) {
                    if (x.ecma3OnlyMode)
                        throw t.newSyntaxError("Illegal catch guard");
                    if (n.catchClauses.length && !n.catchClauses.top().guard)
                        throw t.newSyntaxError("Guarded catch after unguarded");
                    b2.setGuard(n2, Expression(t, x));
                } else {
                    b2.setGuard(n2, null);
                }
                t.mustMatch(RIGHT_PAREN);
                b2.setBlock(n2, Block(t, x));
                b2.finish(n2);
                b.addCatch(n, n2);
            }
            b.finishCatches(n);
            if (t.match(FINALLY))
                b.setFinallyBlock(n, Block(t, x));
            if (!n.catchClauses.length && !n.finallyBlock)
                throw t.newSyntaxError("Invalid try statement");
            b.finish(n);
            return n;

          case CATCH:
          case FINALLY:
            throw t.newSyntaxError(definitions.tokens[tt] + " without preceding try");

          case THROW:
            b = builder.THROW;
            n = b.build(t);
            b.setException(n, Expression(t, x));
            b.finish(n);
            break;

          case RETURN:
            n = returnOrYield(t, x);
            break;

          case WITH:
            b = builder.WITH;
            n = b.build(t);
            b.setObject(n, ParenExpression(t, x));
            b.setBody(n, nest(t, x, n, Statement));
            b.finish(n);
            return n;

          case VAR:
          case CONST:
            n = Variables(t, x);
            break;

          case LET:
            if (t.peek() === LEFT_PAREN)
                n = LetBlock(t, x, true);
            else
                n = Variables(t, x);
            break;

          case DEBUGGER:
            n = builder.DEBUGGER.build(t);
            break;

          case NEWLINE:
          case SEMICOLON:
            b = builder.SEMICOLON;
            n = b.build(t);
            b.setExpression(n, null);
            b.finish(t);
            return n;

          default:
            if (tt === IDENTIFIER) {
                tt = t.peek();
                // Labeled statement.
                if (tt === COLON) {
                    label = t.token.value;
                    ss = x.stmtStack;
                    for (i = ss.length-1; i >= 0; --i) {
                        if (ss[i].label === label)
                            throw t.newSyntaxError("Duplicate label");
                    }
                    t.get();
                    b = builder.LABEL;
                    n = b.build(t);
                    b.setLabel(n, label)
                    b.setStatement(n, nest(t, x, n, Statement));
                    b.finish(n);
                    return n;
                }
            }

            // Expression statement.
            // We unget the current token to parse the expression as a whole.
            b = builder.SEMICOLON;
            n = b.build(t);
            t.unget();
            b.setExpression(n, Expression(t, x));
            n.end = n.expression.end;
            b.finish(n);
            break;
        }

        MagicalSemicolon(t);
        return n;
    }

    function MagicalSemicolon(t) {
        var tt;
        if (t.lineno === t.token.lineno) {
            tt = t.peekOnSameLine();
            if (tt !== END && tt !== NEWLINE && tt !== SEMICOLON && tt !== RIGHT_CURLY)
                throw t.newSyntaxError("missing ; before statement");
        }
        t.match(SEMICOLON);
    }

    function returnOrYield(t, x) {
        var n, b, tt = t.token.type, tt2;

        if (tt === RETURN) {
            if (!x.inFunction)
                throw t.newSyntaxError("Return not in function");
            b = x.builder.RETURN;
        } else /* if (tt === YIELD) */ {
            if (!x.inFunction)
                throw t.newSyntaxError("Yield not in function");
            x.isGenerator = true;
            b = x.builder.YIELD;
        }
        n = b.build(t);

        tt2 = t.peek(true);
        if (tt2 !== END && tt2 !== NEWLINE &&
            tt2 !== SEMICOLON && tt2 !== RIGHT_CURLY
            && (tt !== YIELD ||
                (tt2 !== tt && tt2 !== RIGHT_BRACKET && tt2 !== RIGHT_PAREN &&
                 tt2 !== COLON && tt2 !== COMMA))) {
            if (tt === RETURN) {
                b.setValue(n, Expression(t, x));
                x.hasReturnWithValue = true;
            } else {
                b.setValue(n, AssignExpression(t, x));
            }
        } else if (tt === RETURN) {
            x.hasEmptyReturn = true;
        }

        // Disallow return v; in generator.
        if (x.hasReturnWithValue && x.isGenerator)
            throw t.newSyntaxError("Generator returns a value");

        b.finish(n);
        return n;
    }

    /*
     * FunctionDefinition :: (tokenizer, compiler context, boolean,
     *                        DECLARED_FORM or EXPRESSED_FORM or STATEMENT_FORM)
     *                    -> node
     */
    function FunctionDefinition(t, x, requireName, functionForm) {
        var tt, x2, rp;
        var builder = x.builder;
        var b = builder.FUNCTION;
        var f = b.build(t);
        if (t.match(IDENTIFIER))
            b.setName(f, t.token.value);
        else if (requireName)
            throw t.newSyntaxError("missing function identifier");

        t.mustMatch(LEFT_PAREN);
        if (!t.match(RIGHT_PAREN)) {
            do {
                switch (t.get()) {
                  case LEFT_BRACKET:
                  case LEFT_CURLY:
                    // Destructured formal parameters.
                    t.unget();
                    b.addParam(f, DestructuringExpression(t, x));
                    break;
                  case IDENTIFIER:
                    b.addParam(f, t.token.value);
                    break;
                  default:
                    throw t.newSyntaxError("missing formal parameter");
                    break;
                }
            } while (t.match(COMMA));
            t.mustMatch(RIGHT_PAREN);
        }

        // Do we have an expression closure or a normal body?
        tt = t.get();
        if (tt !== LEFT_CURLY)
            t.unget();

        x2 = new StaticContext(true, builder);
        rp = t.save();
        if (x.inFunction) {
            /*
             * Inner functions don't reset block numbering, only functions at
             * the top level of the program do.
             */
            x2.blockId = x.blockId;
        }

        if (tt !== LEFT_CURLY) {
            b.setBody(f, AssignExpression(t, x));
            if (x.isGenerator)
                throw t.newSyntaxError("Generator returns a value");
        } else {
            b.hoistVars(x2.blockId);
            b.setBody(f, Script(t, x2));
        }

        /*
         * Hoisting makes parse-time binding analysis tricky. A taxonomy of hoists:
         *
         * 1. vars hoist to the top of their function:
         *
         *    var x = 'global';
         *    function f() {
         *      x = 'f';
         *      if (false)
         *        var x;
         *    }
         *    f();
         *    print(x); // "global"
         *
         * 2. lets hoist to the top of their block:
         *
         *    function f() { // id: 0
         *      var x = 'f';
         *      {
         *        {
         *          print(x); // "undefined"
         *        }
         *        let x;
         *      }
         *    }
         *    f();
         *
         * 3. inner functions at function top-level hoist to the beginning
         *    of the function.
         *
         * If the builder used is doing parse-time analyses, hoisting may
         * invalidate earlier conclusions it makes about variable scope.
         *
         * The builder can opt to set the needsHoisting flag in a
         * CompilerContext (in the case of var and function hoisting) or in a
         * node of type BLOCK (in the case of let hoisting). This signals for
         * the parser to reparse sections of code.
         *
         * To avoid exponential blowup, if a function at the program top-level
         * has any hoists in its child blocks or inner functions, we reparse
         * the entire toplevel function. Each toplevel function is parsed at
         * most twice.
         *
         * The list of declarations can be tied to block ids to aid talking
         * about declarations of blocks that have not yet been fully parsed.
         *
         * Blocks are already uniquely numbered; see the comment in
         * Statements.
         */
        if (x2.needsHoisting) {
            // Order is important here! Builders expect funDecls to come
            // after varDecls!
            builder.setHoists(f.body.id, x2.varDecls.concat(x2.funDecls));

            if (x.inFunction) {
                /*
                 * If an inner function needs hoisting, we need to propagate
                 * this flag up to the parent function.
                 */
                x.needsHoisting = true;
            } else {
                // Only re-parse functions at the top level of the program.
                x2 = new StaticContext(true, builder);
                t.rewind(rp);
                /*
                 * Set a flag in case the builder wants to have different behavior
                 * on the second pass.
                 */
                builder.secondPass = true;
                b.hoistVars(f.body.id, true);
                b.setBody(f, Script(t, x2));
                builder.secondPass = false;
            }
        }

        if (tt === LEFT_CURLY)
            t.mustMatch(RIGHT_CURLY);

        f.end = t.token.end;
        f.functionForm = functionForm;
        if (functionForm === DECLARED_FORM)
            x.funDecls.push(f);
        b.finish(f, x);
        return f;
    }

    /*
     * Variables :: (tokenizer, compiler context) -> node
     *
     * Parses a comma-separated list of var declarations (and maybe
     * initializations).
     */
    function Variables(t, x, letBlock) {
        var b, n, n2, ss, i, s, tt;
        var builder = x.builder;
        var bDecl = builder.DECL;

        switch (t.token.type) {
          case VAR:
            b = builder.VAR;
            s = x;
            break;
          case CONST:
            b = builder.CONST;
            s = x;
            break;
          case LET:
          case LEFT_PAREN:
            b = builder.LET;
            if (!letBlock) {
                ss = x.stmtStack;
                i = ss.length;
                while (ss[--i].type !== BLOCK) ; // a BLOCK *must* be found.
                /*
                 * Lets at the function toplevel are just vars, at least in
                 * SpiderMonkey.
                 */
                if (i === 0) {
                    b = builder.VAR;
                    s = x;
                } else {
                    s = ss[i];
                }
            } else {
                s = letBlock;
            }
            break;
        }

        n = b.build(t);
        n.destructurings = [];

        do {
            tt = t.get();
            if (tt === LEFT_BRACKET || tt === LEFT_CURLY) {
                // Need to unget to parse the full destructured expression.
                t.unget();

                var dexp = DestructuringExpressionNoHoist(t, x, true, s);

                n2 = bDecl.build(t);
                bDecl.setName(n2, dexp);
                bDecl.setReadOnly(n2, n.type === CONST);
                b.addDestructuringDecl(n, n2, s);

                n.destructurings.push({ exp: dexp, decl: n2 });

                if (x.inForLoopInit && t.peek() === IN) {
                    continue;
                }

                t.mustMatch(ASSIGN);
                if (t.token.assignOp)
                    throw t.newSyntaxError("Invalid variable initialization");

                bDecl.setInitializer(n2, AssignExpression(t, x));
                bDecl.finish(n2);

                continue;
            }

            if (tt !== IDENTIFIER)
                throw t.newSyntaxError("missing variable name");

            n2 = bDecl.build(t);
            bDecl.setName(n2, t.token.value);
            bDecl.setReadOnly(n2, n.type === CONST);
            b.addDecl(n, n2, s);

            if (t.match(ASSIGN)) {
                if (t.token.assignOp)
                    throw t.newSyntaxError("Invalid variable initialization");

                bDecl.setInitializer(n2, AssignExpression(t, x));
            }

            bDecl.finish(n2);
        } while (t.match(COMMA));
        b.finish(n);
        return n;
    }

    /*
     * LetBlock :: (tokenizer, compiler context, boolean) -> node
     *
     * Does not handle let inside of for loop init.
     */
    function LetBlock(t, x, isStatement) {
        var n, n2, binds;
        var builder = x.builder;
        var b = builder.LET_BLOCK, b2;

        // t.token.type must be LET
        n = b.build(t);
        t.mustMatch(LEFT_PAREN);
        b.setVariables(n, Variables(t, x, n));
        t.mustMatch(RIGHT_PAREN);

        if (isStatement && t.peek() !== LEFT_CURLY) {
            /*
             * If this is really an expression in let statement guise, then we
             * need to wrap the LET_BLOCK node in a SEMICOLON node so that we pop
             * the return value of the expression.
             */
            b2 = builder.SEMICOLON;
            n2 = b2.build(t);
            b2.setExpression(n2, n);
            b2.finish(n2);
            isStatement = false;
        }

        if (isStatement) {
            n2 = Block(t, x);
            b.setBlock(n, n2);
        } else {
            n2 = AssignExpression(t, x);
            b.setExpression(n, n2);
        }

        b.finish(n);

        return n;
    }

    function checkDestructuring(t, x, n, simpleNamesOnly, data) {
        if (n.type === ARRAY_COMP)
            throw t.newSyntaxError("Invalid array comprehension left-hand side");
        if (n.type !== ARRAY_INIT && n.type !== OBJECT_INIT)
            return;

        var lhss = {};
        var nn, n2, idx, sub;
        for (var i = 0, j = n.length; i < j; i++) {
            if (!(nn = n[i]))
                continue;
            if (nn.type === PROPERTY_INIT) {
                sub = nn[1];
                idx = nn[0].value;
            } else if (n.type === OBJECT_INIT) {
                // Do we have destructuring shorthand {foo, bar}?
                sub = nn;
                idx = nn.value;
            } else {
                sub = nn;
                idx = i;
            }

            if (sub.type === ARRAY_INIT || sub.type === OBJECT_INIT) {
                lhss[idx] = checkDestructuring(t, x, sub,
                                               simpleNamesOnly, data);
            } else {
                if (simpleNamesOnly && sub.type !== IDENTIFIER) {
                    // In declarations, lhs must be simple names
                    throw t.newSyntaxError("missing name in pattern");
                }

                lhss[idx] = sub;
            }
        }

        return lhss;
    }

    function DestructuringExpression(t, x, simpleNamesOnly, data) {
        var n = PrimaryExpression(t, x);
        // Keep the list of lefthand sides in case the builder wants to
        // desugar.
        n.destructuredNames = checkDestructuring(t, x, n,
                                                 simpleNamesOnly, data);
        return n;
    }

    function DestructuringExpressionNoHoist(t, x, simpleNamesOnly, data) {
        // Sometimes we don't want to flag the pattern as possible hoists, so
        // pretend it's the second pass.
        var builder = x.builder;
        var oldSP = builder.secondPass;
        builder.secondPass = true;
        var dexp = DestructuringExpression(t, x, simpleNamesOnly, data);
        builder.secondPass = oldSP;
        return dexp;
    }

    function GeneratorExpression(t, x, e) {
        var n, b = x.builder.GENERATOR;

        n = b.build(t);
        b.setExpression(n, e);
        b.setTail(n, comprehensionTail(t, x));
        b.finish(n);

        return n;
    }

    function comprehensionTail(t, x) {
        var body, n;
        var builder = x.builder;
        var b = builder.COMP_TAIL;
        var bFor = builder.FOR;
        var bDecl = builder.DECL;
        var bVar = builder.VAR;

        // t.token.type must be FOR
        body = b.build(t);

        do {
            n = bFor.build(t);
            // Comprehension tails are always for..in loops.
            bFor.rebuildForIn(n);
            if (t.match(IDENTIFIER)) {
                // But sometimes they're for each..in.
                if (t.token.value === "each")
                    bFor.rebuildForEach(n);
                else
                    t.unget();
            }
            t.mustMatch(LEFT_PAREN);
            switch(t.get()) {
              case LEFT_BRACKET:
              case LEFT_CURLY:
                t.unget();
                // Destructured left side of for in comprehension tails.
                b2.setIterator(n, DestructuringExpressionNoHoist(t, x), null);
                break;

              case IDENTIFIER:
                var n3 = bDecl.build(t);
                bDecl.setName(n3, n3.value);
                bDecl.finish(n3);
                var n2 = bVar.build(t);
                bVar.addDecl(n2, n3, x);
                bVar.finish(n2);
                bFor.setIterator(n, n3, n2);
                // Don't add to varDecls since the semantics of comprehensions is
                // such that the variables are in their own function when
                // desugared.
                break;

              default:
                throw t.newSyntaxError("missing identifier");
            }
            t.mustMatch(IN);
            bFor.setObject(n, Expression(t, x));
            t.mustMatch(RIGHT_PAREN);
            b.addFor(body, n);
        } while (t.match(FOR));

        // Optional guard.
        if (t.match(IF))
            b.setGuard(body, ParenExpression(t, x));

        b.finish(body);
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
            if (n.type === YIELD && !n.parenthesized)
                throw t.newSyntaxError("Yield " + err);
            if (n.type === COMMA && !n.parenthesized)
                throw t.newSyntaxError("Generator " + err);
            n = GeneratorExpression(t, x, n);
        }

        t.mustMatch(RIGHT_PAREN);

        return n;
    }

    /*
     * Expression :: (tokenizer, compiler context) -> node
     *
     * Top-down expression parser matched against SpiderMonkey.
     */
    function Expression(t, x) {
        var n, n2, b = x.builder.COMMA;

        n = AssignExpression(t, x);
        if (t.match(COMMA)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            n = n2;
            do {
                n2 = n[n.length-1];
                if (n2.type === YIELD && !n2.parenthesized)
                    throw t.newSyntaxError("Yield expression must be parenthesized");
                b.addOperand(n, AssignExpression(t, x));
            } while (t.match(COMMA));
            b.finish(n);
        }

        return n;
    }

    function AssignExpression(t, x) {
        var n, lhs, b = x.builder.ASSIGN;

        // Have to treat yield like an operand because it could be the leftmost
        // operand of the expression.
        if (t.match(YIELD, true))
            return returnOrYield(t, x);

        n = b.build(t);
        lhs = ConditionalExpression(t, x);

        if (!t.match(ASSIGN)) {
            b.finish(n);
            return lhs;
        }

        switch (lhs.type) {
          case OBJECT_INIT:
          case ARRAY_INIT:
            lhs.destructuredNames = checkDestructuring(t, x, lhs);
            // FALL THROUGH
          case IDENTIFIER: case DOT: case INDEX: case CALL:
            break;
          default:
            throw t.newSyntaxError("Bad left-hand side of assignment");
            break;
        }

        b.setAssignOp(n, t.token.assignOp);
        b.addOperand(n, lhs);
        b.addOperand(n, AssignExpression(t, x));
        b.finish(n);

        return n;
    }

    function ConditionalExpression(t, x) {
        var n, n2, b = x.builder.HOOK;

        n = OrExpression(t, x);
        if (t.match(HOOK)) {
            n2 = n;
            n = b.build(t);
            b.setCondition(n, n2);
            /*
             * Always accept the 'in' operator in the middle clause of a ternary,
             * where it's unambiguous, even if we might be parsing the init of a
             * for statement.
             */
            var oldLoopInit = x.inForLoopInit;
            x.inForLoopInit = false;
            b.setThenPart(n, AssignExpression(t, x));
            x.inForLoopInit = oldLoopInit;
            if (!t.match(COLON))
                throw t.newSyntaxError("missing : after ?");
            b.setElsePart(n, AssignExpression(t, x));
            b.finish(n);
        }

        return n;
    }

    function OrExpression(t, x) {
        var n, n2, b = x.builder.OR;

        n = AndExpression(t, x);
        while (t.match(OR)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, AndExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function AndExpression(t, x) {
        var n, n2, b = x.builder.AND;

        n = BitwiseOrExpression(t, x);
        while (t.match(AND)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, BitwiseOrExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function BitwiseOrExpression(t, x) {
        var n, n2, b = x.builder.BITWISE_OR;

        n = BitwiseXorExpression(t, x);
        while (t.match(BITWISE_OR)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, BitwiseXorExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function BitwiseXorExpression(t, x) {
        var n, n2, b = x.builder.BITWISE_XOR;

        n = BitwiseAndExpression(t, x);
        while (t.match(BITWISE_XOR)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, BitwiseAndExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function BitwiseAndExpression(t, x) {
        var n, n2, b = x.builder.BITWISE_AND;

        n = EqualityExpression(t, x);
        while (t.match(BITWISE_AND)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, EqualityExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function EqualityExpression(t, x) {
        var n, n2, b = x.builder.EQUALITY;

        n = RelationalExpression(t, x);
        while (t.match(EQ) || t.match(NE) ||
               t.match(STRICT_EQ) || t.match(STRICT_NE)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, RelationalExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function RelationalExpression(t, x) {
        var n, n2, b = x.builder.RELATIONAL;
        var oldLoopInit = x.inForLoopInit;

        /*
         * Uses of the in operator in shiftExprs are always unambiguous,
         * so unset the flag that prohibits recognizing it.
         */
        x.inForLoopInit = false;
        n = ShiftExpression(t, x);
        while ((t.match(LT) || t.match(LE) || t.match(GE) || t.match(GT) ||
               (oldLoopInit === false && t.match(IN)) ||
               t.match(INSTANCEOF))) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, ShiftExpression(t, x));
            b.finish(n2);
            n = n2;
        }
        x.inForLoopInit = oldLoopInit;

        return n;
    }

    function ShiftExpression(t, x) {
        var n, n2, b = x.builder.SHIFT;

        n = AddExpression(t, x);
        while (t.match(LSH) || t.match(RSH) || t.match(URSH)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, AddExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function AddExpression(t, x) {
        var n, n2, b = x.builder.ADD;

        n = MultiplyExpression(t, x);
        while (t.match(PLUS) || t.match(MINUS)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, MultiplyExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function MultiplyExpression(t, x) {
        var n, n2, b = x.builder.MULTIPLY;

        n = UnaryExpression(t, x);
        while (t.match(MUL) || t.match(DIV) || t.match(MOD)) {
            n2 = b.build(t);
            b.addOperand(n2, n);
            b.addOperand(n2, UnaryExpression(t, x));
            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function UnaryExpression(t, x) {
        var n, n2, tt, b = x.builder.UNARY;

        switch (tt = t.get(true)) {
          case DELETE: case VOID: case TYPEOF:
          case NOT: case BITWISE_NOT: case PLUS: case MINUS:
            n = b.build(t);
            b.addOperand(n, UnaryExpression(t, x));
            break;

          case INCREMENT:
          case DECREMENT:
            // Prefix increment/decrement.
            n = b.build(t)
            b.addOperand(n, MemberExpression(t, x, true));
            break;

          default:
            t.unget();
            n = MemberExpression(t, x, true);

            // Don't look across a newline boundary for a postfix {in,de}crement.
            if (t.tokens[(t.tokenIndex + t.lookahead - 1) & 3].lineno ===
                t.lineno) {
                if (t.match(INCREMENT) || t.match(DECREMENT)) {
                    n2 = b.build(t);
                    b.setPostfix(n2);
                    b.finish(n);
                    b.addOperand(n2, n);
                    n = n2;
                }
            }
            break;
        }

        b.finish(n);
        return n;
    }

    function MemberExpression(t, x, allowCallSyntax) {
        var n, n2, name, tt;
        var builder = x.builder;
        var b = builder.MEMBER
        var b2 = builder.PROPERTY_NAME;

        if (t.match(NEW)) {
            n = b.build(t);
            b.addOperand(n, MemberExpression(t, x, false));
            if (t.match(LEFT_PAREN)) {
                b.rebuildNewWithArgs(n);
                b.addOperand(n, ArgumentList(t, x));
            }
            b.finish(n);
        } else {
            n = PrimaryExpression(t, x);
        }

        while ((tt = t.get()) !== END) {
            switch (tt) {
              case DOT:
                n2 = b.build(t);
                b.addOperand(n2, n);
                t.mustMatch(IDENTIFIER);
                name = b2.build(t);
                b2.finish(name);
                b.addOperand(n2, name);
                break;

              case LEFT_BRACKET:
                n2 = b.build(t, INDEX);
                b.addOperand(n2, n);
                b.addOperand(n2, Expression(t, x));
                t.mustMatch(RIGHT_BRACKET);
                break;

              case LEFT_PAREN:
                if (allowCallSyntax) {
                    n2 = b.build(t, CALL);
                    b.addOperand(n2, n);
                    b.addOperand(n2, ArgumentList(t, x));
                    break;
                }

                // FALL THROUGH
              default:
                t.unget();
                return n;
            }

            b.finish(n2);
            n = n2;
        }

        return n;
    }

    function ArgumentList(t, x) {
        var n, n2, b = x.builder.LIST;
        var err = "expression must be parenthesized";

        n = b.build(t);
        if (t.match(RIGHT_PAREN, true))
            return n;
        do {
            n2 = AssignExpression(t, x);
            if (n2.type === YIELD && !n2.parenthesized && t.peek() === COMMA)
                throw t.newSyntaxError("Yield " + err);
            if (t.match(FOR)) {
                n2 = GeneratorExpression(t, x, n2);
                if (n.length > 1 || t.peek(true) === COMMA)
                    throw t.newSyntaxError("Generator " + err);
            }
            b.addOperand(n, n2);
        } while (t.match(COMMA));
        t.mustMatch(RIGHT_PAREN);
        b.finish(n);

        return n;
    }

    function PrimaryExpression(t, x) {
        var n, n2, n3, tt = t.get(true);
        var builder = x.builder;
        var bArrayInit = builder.ARRAY_INIT;
        var bArrayComp = builder.ARRAY_COMP;
        var bPrimary = builder.PRIMARY;
        var bPropName = builder.PROPERTY_NAME;
        var bObjInit = builder.OBJECT_INIT;
        var bPropInit = builder.PROPERTY_INIT;

        switch (tt) {
          case FUNCTION:
            n = FunctionDefinition(t, x, false, EXPRESSED_FORM);
            break;

          case LEFT_BRACKET:
            n = bArrayInit.build(t);
            while ((tt = t.peek(true)) !== RIGHT_BRACKET) {
                if (tt === COMMA) {
                    t.get();
                    bArrayInit.addElement(n, null);
                    continue;
                }
                bArrayInit.addElement(n, AssignExpression(t, x));
                if (tt !== COMMA && !t.match(COMMA))
                    break;
            }

            // If we matched exactly one element and got a FOR, we have an
            // array comprehension.
            if (n.length === 1 && t.match(FOR)) {
                n2 = bArrayComp.build(t);
                bArrayComp.setExpression(n2, n[0]);
                bArrayComp.setTail(n2, comprehensionTail(t, x));
                n = n2;
            }
            t.mustMatch(RIGHT_BRACKET);
            bArrayInit.finish(n);
            break;

          case LEFT_CURLY:
            var id, fd;
            n = bObjInit.build(t);

          object_init:
            if (!t.match(RIGHT_CURLY)) {
                do {
                    tt = t.get();
                    if ((t.token.value === "get" || t.token.value === "set") &&
                        t.peek() === IDENTIFIER) {
                        if (x.ecma3OnlyMode)
                            throw t.newSyntaxError("Illegal property accessor");
                        fd = FunctionDefinition(t, x, true, EXPRESSED_FORM);
                        bObjInit.addProperty(n, fd);
                    } else {
                        switch (tt) {
                          case IDENTIFIER: case NUMBER: case STRING:
                            id = bPropName.build(t);
                            bPropName.finish(id);
                            break;
                          case RIGHT_CURLY:
                            if (x.ecma3OnlyMode)
                                throw t.newSyntaxError("Illegal trailing ,");
                            break object_init;
                          default:
                            if (t.token.value in definitions.keywords) {
                                id = bPropName.build(t);
                                bPropName.finish(id);
                                break;
                            }
                            throw t.newSyntaxError("Invalid property name");
                        }
                        if (t.match(COLON)) {
                            n2 = bPropInit.build(t);
                            bPropInit.addOperand(n2, id);
                            bPropInit.addOperand(n2, AssignExpression(t, x));
                            bPropInit.finish(n2);
                            bObjInit.addProperty(n, n2);
                        } else {
                            // Support, e.g., |var {x, y} = o| as destructuring shorthand
                            // for |var {x: x, y: y} = o|, per proposed JS2/ES4 for JS1.8.
                            if (t.peek() !== COMMA && t.peek() !== RIGHT_CURLY)
                                throw t.newSyntaxError("missing : after property");
                            bObjInit.addProperty(n, id);
                        }
                    }
                } while (t.match(COMMA));
                t.mustMatch(RIGHT_CURLY);
            }
            bObjInit.finish(n);
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
            n = bPrimary.build(t);
            bPrimary.finish(n);
            break;

          default:
            throw t.newSyntaxError("missing operand");
            break;
        }

        return n;
    }

    /*
     * parse :: (builder, file ptr, path, line number) -> node
     */
    function parse(b, s, f, l) {
        var t = new lexer.Tokenizer(s, f, l);
        var x = new StaticContext(false, b);
        var n = Script(t, x);
        if (!t.done)
            throw t.newSyntaxError("Syntax error");

        return n;
    }

    return {
        parse: parse,
        Node: Node,
        DefaultBuilder: DefaultBuilder,
        get SSABuilder() {
            throw new Error("SSA builder not yet supported");
        },
        bindSubBuilders: bindSubBuilders,
        DECLARED_FORM: DECLARED_FORM,
        EXPRESSED_FORM: EXPRESSED_FORM,
        STATEMENT_FORM: STATEMENT_FORM,
        Tokenizer: lexer.Tokenizer,
        FunctionDefinition: FunctionDefinition
    };

}());
