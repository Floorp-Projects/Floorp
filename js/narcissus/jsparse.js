/* -*- Mode: JS; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set sw=4 ts=8 et tw=78:
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

// boolean -> undefined
// inFunction is used to check if a return stm appears in a valid context.
function CompilerContext(inFunction) {
    this.inFunction = inFunction;
    //The elms of stmtStack are used to find the target label of CONTINUEs and
    // BREAKs. Its length is used in function definitions.
    this.stmtStack = [];
    this.funDecls = [];
    //varDecls accumulate when we process decls w/ the var keyword.
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

// tokenizer, compiler context -> node
// parses the toplevel and function bodies
function Script(t, x) {
    var n = Statements(t, x);
    n.type = SCRIPT;
    n.funDecls = x.funDecls;
    // LETs may add varDecls to blocks.
    n.varDecls = n.varDecls || [];
    Array.prototype.push.apply(n.varDecls, x.varDecls);
    return n;
}

// Node extends Array, which we extend slightly with a top-of-stack method.
defineProperty(Array.prototype, "top",
               function() {
                   return this.length && this[this.length-1];
               }, false, false, true);

// tokenizer, optional type -> node
function Node(t, type) {
    var token = t.token;
    if (token) {
        this.type = type || token.type;
        this.value = token.value;
        this.lineno = token.lineno;
        // start & end are file positions for error handling
        this.start = token.start;
        this.end = token.end;
    } else {
        this.type = type;
        this.lineno = t.lineno;
    }
    // nodes use a tokenizer for debugging (getSource, filename getter)
    this.tokenizer = t;

    for (var i = 2; i < arguments.length; i++)
        this.push(arguments[i]);
}

var Np = Node.prototype = new Array;
Np.constructor = Node;
Np.toSource = Object.prototype.toSource;

// Always use push to add operands to an expression, to update start and end.
Np.push = function (kid) {
    if (kid !== null) { // kids can be null e.g. [1, , 2]
        if (kid.start < this.start)
            this.start = kid.start;
        if (this.end < kid.end)
            this.end = kid.end;
    }
    return Array.prototype.push.call(this, kid);
}

Node.indentLevel = 0;

function tokenstr(tt) {
    var t = tokens[tt];
    return /^\W/.test(t) ? opTypeNames[t] : t.toUpperCase();
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

defineGetter(Np, "filename",
             function() {
                 return this.tokenizer.filename;
             });

defineProperty(String.prototype, "repeat",
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

// tokenizer, compiler context -> node
// parses a list of Statements
function Statements(t, x) {
    var n = new Node(t, BLOCK);
    x.stmtStack.push(n);
    while (!t.done && t.peek() != RIGHT_CURLY)
        n.push(Statement(t, x));
    x.stmtStack.pop();
    return n;
}

function Block(t, x) {
    t.mustMatch(LEFT_CURLY);
    var n = Statements(t, x);
    t.mustMatch(RIGHT_CURLY);
    return n;
}

const DECLARED_FORM = 0, EXPRESSED_FORM = 1, STATEMENT_FORM = 2;

// tokenizer, compiler context -> node
// parses a Statement
function Statement(t, x) {
    var i, label, n, n2, ss, tt = t.get();

    // Cases for statements ending in a right curly return early, avoiding the
    // common semicolon insertion magic after this switch.
    switch (tt) {
      case LET:
        n = LetForm(t, x, STATEMENT_FORM);
        if (n.type === LET_STM)
            return n;
        if (n.type === LET_EXP) {// exps in stm context are semi nodes
            n2 = new Node(t, SEMICOLON);
            n2.expression = n;
            n = n2;
            n.end = n.expression.end;
        }
        break;

      case FUNCTION:
        // DECLD_FORM extends fundefs of x, STM_FORM doesn't.
        return FunctionDefinition(t, x, true,
                                  (x.stmtStack.length > 1)
                                  ? STATEMENT_FORM
                                  : DECLARED_FORM);

      case LEFT_CURLY:
        n = Statements(t, x);
        t.mustMatch(RIGHT_CURLY);
        return n;

      case IF:
        n = new Node(t);
        n.condition = ParenExpression(t, x);
        x.stmtStack.push(n);
        n.thenPart = Statement(t, x);
        n.elsePart = t.match(ELSE) ? Statement(t, x) : null;
        x.stmtStack.pop();
        return n;

      case SWITCH:
        // This allows CASEs after a DEFAULT, which is in the standard.
        n = new Node(t);

        n.discriminant = ParenExpression(t, x);
        n.cases = [];
        n.defaultIndex = -1;
        x.stmtStack.push(n);
        t.mustMatch(LEFT_CURLY);
        while ((tt = t.get()) != RIGHT_CURLY) {
            switch (tt) {
              case DEFAULT:
                if (n.defaultIndex >= 0)
                    throw t.newSyntaxError("More than one switch default");
                // FALL THROUGH
              case CASE:
                n2 = new Node(t);
                if (tt == DEFAULT)
                    n.defaultIndex = n.cases.length;
                else
                    n2.caseLabel = Expression(t, x, COLON);
                break;
              default:
                throw t.newSyntaxError("Invalid switch case");
            }
            t.mustMatch(COLON);
            n2.statements = new Node(t, BLOCK);
            while ((tt=t.peek()) != CASE && tt != DEFAULT && tt != RIGHT_CURLY)
                n2.statements.push(Statement(t, x));
            n.cases.push(n2);
        }
        x.stmtStack.pop();
        return n;

      case FOR:
        n = new Node(t);
        n.isLoop = true;
        if (t.match(IDENTIFIER)) {
            if (t.token.value !== "each")
                throw t.newSyntaxError("Illegal identifier after for");
            else
                n.foreach = true;
        }
        t.mustMatch(LEFT_PAREN);
        if ((tt = t.peek()) != SEMICOLON) {
            x.inForLoopInit = true;
            switch (tt) {
              case VAR: case CONST:
                t.get();
                n2 = Variables(t, x);
                break;
              case LET:
                t.get();
                n2 = Variables(t, x, "local decls");
                // don't confuse w/ n.varDecl used by for/in.
                n.varDecls = [];
                for (var i = 0, len = n2.length, vdecls = n.varDecls; i < len; i++)
                    vdecls.push(n2[i]);
                break;
              default:
                n2 = Expression(t, x);
                break;
            }
            x.inForLoopInit = false;
        }
        if (n2 && t.match(IN)) { // for...in
            var n2t = n2.type,
                se = t.newSyntaxError("Invalid for..in left-hand side");
            n.type = FOR_IN;
            if (n2t === VAR || n2t === LET) {
                if (n2.length != 1) throw se;
                n.iterator = n2[0];
                n.varDecl = n2;
            } else {
                function badLhs(tt) {
                     return (tt !== IDENTIFIER && tt !== CALL &&
                             tt !== DOT && tt !== INDEX);
                }
                if (n2t !== GROUP && badLhs(n2t)) throw se;
                if (n2t === GROUP && badLhs(n2[0].type)) throw se;
                n.iterator = n2;
                n.varDecl = null;
            }
            n.object = Expression(t, x);
        } else { // classic for
            if (n.foreach) throw t.newSyntaxError("Illegal for-each syntax");
            n.setup = n2 || null;
            t.mustMatch(SEMICOLON);
            n.condition = (t.peek() == SEMICOLON) ? null : Expression(t, x);
            t.mustMatch(SEMICOLON);
            n.update = (t.peek() == RIGHT_PAREN) ? null : Expression(t, x);
        }
        t.mustMatch(RIGHT_PAREN);
        n.body = nest(t, x, n, Statement);
        return n;

      case WHILE:
        n = new Node(t);
        n.isLoop = true;
        n.condition = ParenExpression(t, x);
        n.body = nest(t, x, n, Statement);
        return n;

      case DO:
        n = new Node(t);
        n.isLoop = true;
        n.body = nest(t, x, n, Statement, WHILE);
        n.condition = ParenExpression(t, x);
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
        n = new Node(t);
        if (t.peekOnSameLine() == IDENTIFIER) {
            t.get();
            n.label = t.token.value;
        }
        ss = x.stmtStack;
        i = ss.length;
        label = n.label;
        if (label) {
            do {
                if (--i < 0)
                    throw t.newSyntaxError("Label not found");
            } while (ss[i].label != label);

            /* Both break and continue to label need to be handled specially
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
        n.target = ss[i]; // cycle in the AST
        break;

      case TRY:
        n = new Node(t);
        n.tryBlock = Block(t, x);
        n.catchClauses = [];
        while (t.match(CATCH)) {
            n2 = new Node(t);
            t.mustMatch(LEFT_PAREN);
            n2.varName = t.mustMatch(IDENTIFIER).value;
            if (t.match(IF)) {
                if (x.ecma3OnlyMode)
                    throw t.newSyntaxError("Illegal catch guard");
                if (n.catchClauses.length && !n.catchClauses.top().guard)
                    throw t.newSyntaxError("Guarded catch after unguarded");
                n2.guard = Expression(t, x);
            } else {
                n2.guard = null;
            }
            t.mustMatch(RIGHT_PAREN);
            n2.block = Block(t, x);
            n.catchClauses.push(n2);
        }
        if (t.match(FINALLY))
            n.finallyBlock = Block(t, x);
        if (!n.catchClauses.length && !n.finallyBlock)
            throw t.newSyntaxError("Invalid try statement");
        return n;

      case CATCH:
      case FINALLY:
        throw t.newSyntaxError(tokens[tt] + " without preceding try");

      case THROW:
        n = new Node(t);
        n.exception = Expression(t, x);
        break;

      case RETURN:
        if (!x.inFunction)
            throw t.newSyntaxError("Invalid return");
        n = new Node(t);
        tt = t.peekOnSameLine();
        if (tt != END && tt != NEWLINE && tt != SEMICOLON && tt != RIGHT_CURLY)
            n.value = Expression(t, x);
        break;

      case WITH:
        n = new Node(t);
        n.object = ParenExpression(t, x);
        n.body = nest(t, x, n, Statement);
        return n;


      case VAR: // for variable declarations using the VAR and CONST keywords.
      case CONST:
        n = Variables(t, x);
        break;

      case DEBUGGER:
        n = new Node(t);
        break;

      case NEWLINE:
      case SEMICOLON:
        n = new Node(t, SEMICOLON);
        n.expression = null;
        return n;

      default:
        if (tt == IDENTIFIER) {
            t.scanOperand = false;
            tt = t.peek();
            t.scanOperand = true;
            // labeled statement
            if (tt == COLON) {
                label = t.token.value;
                ss = x.stmtStack;
                for (i = ss.length - 1; i >= 0; --i) {
                    if (ss[i].label == label)
                        throw t.newSyntaxError("Duplicate label");
                }
                t.get();
                n = new Node(t, LABEL);
                n.label = label;
                n.statement = nest(t, x, n, Statement);
                return n;
            }
        }
        // expression statement.
        // We unget the current token to parse the expr as a whole.
        n = new Node(t, SEMICOLON);
        t.unget();
        n.expression = Expression(t, x);
        n.end = n.expression.end;
        break;
    }

    // semicolon-insertion magic
    if (t.lineno == t.token.lineno) {
        tt = t.peekOnSameLine();
        if (tt != END && tt != NEWLINE && tt != SEMICOLON && tt != RIGHT_CURLY)
            throw t.newSyntaxError("Missing ; before statement");
    }
    t.match(SEMICOLON);
    return n;
}

// tokenizer, compiler context, boolean,
// DECLARED_FORM or EXPRESSED_FORM or STATEMENT_FORM -> node
function FunctionDefinition(t, x, requireName, functionForm) {
    var f = new Node(t);
    if (f.type != FUNCTION)
        f.type = (f.value == "get") ? GETTER : SETTER;
    if (t.match(IDENTIFIER))
        f.name = t.token.value;
    else if (requireName)
        throw t.newSyntaxError("Missing function identifier");

    t.mustMatch(LEFT_PAREN);
    f.params = [];
    var tt;
    while ((tt = t.get()) != RIGHT_PAREN) {
        if (tt != IDENTIFIER)
            throw t.newSyntaxError("Missing formal parameter");
        f.params.push(t.token.value);
        if (t.peek() != RIGHT_PAREN)
            t.mustMatch(COMMA);
    }

    if (t.match(LEFT_CURLY)) {
        var x2 = new CompilerContext(true);
        f.body = Script(t, x2);
        t.mustMatch(RIGHT_CURLY);
    } else { /* Expression closures (1.8) */
        f.body = Expression(t, x, COMMA);
    }
    f.end = t.token.end;

    f.functionForm = functionForm;
    if (functionForm == DECLARED_FORM)
        x.funDecls.push(f);
    return f;
}

// tokenizer, compiler context -> node
// parses a comma-separated list of var decls (and maybe initializations)
function Variables(t, x) {
    var n = new Node(t), tt, n2;
    do {
        tt = t.peek();
        if (tt === LEFT_CURLY || tt === LEFT_BRACKET) {
            n2 = Expression(t, x); // for destructuring
        } else {
            t.mustMatch(IDENTIFIER);
            n2 = new Node(t);
            n2.name = n2.value;
        }
        if (t.match(ASSIGN)) {
            if (t.token.assignOp)
                throw t.newSyntaxError("Invalid variable initialization");
            n2.initializer = Expression(t, x, COMMA);
        }
        n2.readOnly = (n.type == CONST);
        n.push(n2);
        // LETs use "local decls"
        if (arguments[2] !== "local decls") x.varDecls.push(n2);
    } while (t.match(COMMA));
    return n;
}

// tokenizer, comp. context, EXPRESSED_FORM or STATEMENT_FORM -> node
// doesn't handle lets in the toplevel of forloop heads
function LetForm(t, x, form) {
    var i, n, n2, s, ss, hasLeftParen;

    n = new Node(t);
    hasLeftParen = t.match(LEFT_PAREN);
    n2 = Variables(t, x, "local decls");
    if (hasLeftParen) {//let statement and let expression
        t.mustMatch(RIGHT_PAREN);
        n.varDecls = [];
        for (i = 0; i < n2.length; i++)
            n.varDecls.push(n2[i]);
        if (form === STATEMENT_FORM && t.peek() === RIGHT_CURLY) {
            n.type = LET_STM;
            n.body = nest(t, x, n, Block);
        } else {
            n.type = LET_EXP;
            n.body = Expression(t, x, COMMA);
        }
    } else if (form === EXPRESSED_FORM) {
        throw t.newSyntaxError("Let-definition used as expression.");
    } else {//let definition
        n.type = LET_DEF;
        //search context to find enclosing BLOCK
        ss = x.stmtStack;
        i = ss.length;
        while (ss[--i].type !== BLOCK) ; // a BLOCK *must* be found.
        s = ss[i];
        s.varDecls = s.varDecls || [];
        n.varDecls = [];
        for (i = 0; i < n2.length; i++) {
            s.varDecls.push(n2[i]); // the vars must go in the correct scope
            n.varDecls.push(n2[i]); // but the assignments must stay here
        }
    }
    return n;
}

// tokenizer, compiler context -> node
function ParenExpression(t, x) {
    t.mustMatch(LEFT_PAREN);
    var n = Expression(t, x);
    t.mustMatch(RIGHT_PAREN);
    return n;
}

var opPrecedence = {
    SEMICOLON: 0,
    COMMA: 1,
    ASSIGN: 2, HOOK: 2, COLON: 2,
    // The above all have to have the same precedence, see bug 330975.
    OR: 4,
    AND: 5,
    BITWISE_OR: 6,
    BITWISE_XOR: 7,
    BITWISE_AND: 8,
    EQ: 9, NE: 9, STRICT_EQ: 9, STRICT_NE: 9,
    LT: 10, LE: 10, GE: 10, GT: 10, IN: 10, INSTANCEOF: 10,
    LSH: 11, RSH: 11, URSH: 11,
    PLUS: 12, MINUS: 12,
    MUL: 13, DIV: 13, MOD: 13,
    DELETE: 14, VOID: 14, TYPEOF: 14, // PRE_INCREMENT: 14, PRE_DECREMENT: 14,
    NOT: 14, BITWISE_NOT: 14, UNARY_PLUS: 14, UNARY_MINUS: 14,
    INCREMENT: 15, DECREMENT: 15,     // postfix
    NEW: 16,
    DOT: 17
};

// Map operator type code to precedence.
for (i in opPrecedence)
    opPrecedence[tokenIds[i]] = opPrecedence[i];

var opArity = {
    COMMA: -2,
    ASSIGN: 2,
    HOOK: 3,
    OR: 2,
    AND: 2,
    BITWISE_OR: 2,
    BITWISE_XOR: 2,
    BITWISE_AND: 2,
    EQ: 2, NE: 2, STRICT_EQ: 2, STRICT_NE: 2,
    LT: 2, LE: 2, GE: 2, GT: 2, IN: 2, INSTANCEOF: 2,
    LSH: 2, RSH: 2, URSH: 2,
    PLUS: 2, MINUS: 2,
    MUL: 2, DIV: 2, MOD: 2,
    DELETE: 1, VOID: 1, TYPEOF: 1,  // PRE_INCREMENT: 1, PRE_DECREMENT: 1,
    NOT: 1, BITWISE_NOT: 1, UNARY_PLUS: 1, UNARY_MINUS: 1,
    INCREMENT: 1, DECREMENT: 1,     // postfix
    NEW: 1, NEW_WITH_ARGS: 2, DOT: 2, INDEX: 2, CALL: 2,
    ARRAY_INIT: 1, OBJECT_INIT: 1, GROUP: 1
};

// Map operator type code to arity.
for (i in opArity)
    opArity[tokenIds[i]] = opArity[i];

// tokenizer, compiler context, optional COMMA or COLON -> node
// When scanOperand is true the parser wants an operand (the "default" mode).
// When it's false, the parser is expecting an operator.
function Expression(t, x, stop) {
    var n, id, tt, operators = [], operands = [];
    var bl = x.bracketLevel, cl = x.curlyLevel, pl = x.parenLevel,
        hl = x.hookLevel;

    // void -> node
    // Uses an operator and its operands to construct a whole expression.
    // The result of reduce isn't used by its callers. It's left on the operands
    // stack and it's retrieved from there.
    function reduce() {
        var n = operators.pop();
        var op = n.type;
        var arity = opArity[op];
        if (arity == -2) {
            // Flatten left-associative trees.
            var left = operands.length >= 2 && operands[operands.length-2];
            if (left.type == op) {
                var right = operands.pop();
                left.push(right);
                return left;
            }
            arity = 2;
        }

        // Always use push to add operands to n, to update start and end.
        var a = operands.splice(operands.length - arity);
        for (var i = 0; i < arity; i++)
            n.push(a[i]);

        // Include closing bracket or postfix operator in [start,end).
        if (n.end < t.token.end)
            n.end = t.token.end;

        operands.push(n);
        return n;
    }

    // If we are expecting an operator and find sth else it may not be an error,
    // because of semicolon insertion. So Expression doesn't throw for this.
    // If it turns out to be an error it is detected by various other parts of
    // the code and the msg may be obscure.

    loop: // tt stands for token type
    while ((tt = t.get()) != END) {
        if (tt == stop &&
            x.bracketLevel == bl && x.curlyLevel == cl && x.parenLevel == pl &&
            x.hookLevel == hl) {
            // Stop only if tt matches the optional stop parameter, and that
            // token is not quoted by some kind of bracket.
            break;
        }
        switch (tt) {
          case SEMICOLON:
            // NB: cannot be empty, Statement handled that.
            break loop;

          case LET: //parse let expressions
            //LET is not an operator, no need to assign precedence to it.
            if (!t.scanOperand) break loop;
            operands.push(LetForm(t, x, EXPRESSED_FORM));
            t.scanOperand = false;
            break;

          case ASSIGN:
          //the parser doesn't check that the lhs of an assignment is legal,
          //so it unintentionally allows destructuring here.
          //FIXME: report illegal lhs`s in assignments.
          case HOOK:
          case COLON:
            if (t.scanOperand)
                break loop;

            // Use >, not >=, for right-associative ASSIGN and HOOK/COLON.
            // if operators is empty, operators.top().type is undefined.
            while (opPrecedence[operators.top().type] > opPrecedence[tt] ||
                   (tt == COLON && operators.top().type == ASSIGN)) {
                reduce();
            }
            if (tt == COLON) {
                n = operators.top();
                if (n.type != HOOK)
                    throw t.newSyntaxError("Invalid label");
                --x.hookLevel;
            } else {
                operators.push(new Node(t));
                if (tt == ASSIGN)
                    operands.top().assignOp = t.token.assignOp;
                else
                    ++x.hookLevel;      // tt == HOOK
            }
            t.scanOperand = true;
            break;

          case IN:
            // An in operator should not be parsed if we're parsing the head of
            // a for (...) loop, unless it is in the then part of a conditional
            // expression, or parenthesized somehow.
            if (x.inForLoopInit && !x.hookLevel &&
                !x.bracketLevel && !x.curlyLevel && !x.parenLevel) {
                break loop;
            }
            // FALL THROUGH
          case COMMA:
            // Treat comma as left-associative so reduce can fold left-heavy
            // COMMA trees into a single array.
            // FALL THROUGH
          case OR:
          case AND:
          case BITWISE_OR:
          case BITWISE_XOR:
          case BITWISE_AND:
          case EQ: case NE: case STRICT_EQ: case STRICT_NE:
          case LT: case LE: case GE: case GT:
          case INSTANCEOF:
          case LSH: case RSH: case URSH:
          case PLUS: case MINUS:
          case MUL: case DIV: case MOD:
          case DOT:
            if (t.scanOperand)
                break loop;
            while (opPrecedence[operators.top().type] >= opPrecedence[tt])
                reduce();
            if (tt == DOT) {
                t.mustMatch(IDENTIFIER);
                operands.push(new Node(t, DOT, operands.pop(), new Node(t)));
            } else {
                operators.push(new Node(t));
                t.scanOperand = true;
            }
            break;

          case YIELD:
            if (!x.inFunction) throw t.newSyntaxError("yield not in function");
            // yield is followed by 0 or 1 expr, so we don't know if we should
            // go to operator or operand mode, we must handle the expr here.
            n = new Node(t);
            if ((tt = t.peek()) !== SEMICOLON && tt !== RIGHT_CURLY &&
                tt !== RIGHT_PAREN && tt !== RIGHT_BRACKET && tt !== COMMA &&
                tt !== COLON)
              n.value = Expression(t, x);
            operands.push(n);
            t.scanOperand = false;
            break;
            

          case DELETE: case VOID: case TYPEOF:
          case NOT: case BITWISE_NOT: case UNARY_PLUS: case UNARY_MINUS:
          case NEW:
            if (!t.scanOperand) break loop;
            operators.push(new Node(t));
            break;

          case INCREMENT: case DECREMENT:
            if (t.scanOperand) {
                operators.push(new Node(t));  // prefix increment or decrement
            } else {
                // Don't cross a line boundary for postfix {in,de}crement.
                if (t.tokens[(t.tokenIndex + t.lookahead - 1) & 3].lineno !=
                    t.lineno) {
                    break loop;
                }

                // Use >, not >=, so postfix has higher precedence than prefix.
                while (opPrecedence[operators.top().type] > opPrecedence[tt])
                    reduce();
                n = new Node(t, tt, operands.pop());
                n.postfix = true;
                operands.push(n);
            }
            break;

          case FUNCTION:
            if (!t.scanOperand) break loop;
            operands.push(FunctionDefinition(t, x, false, EXPRESSED_FORM));
            t.scanOperand = false;
            break;

          case NULL:
          case THIS:
          case TRUE:
          case FALSE:
          case IDENTIFIER:
          case NUMBER:
          case STRING:
          case REGEXP:
            if (!t.scanOperand) break loop;
            operands.push(new Node(t));
            t.scanOperand = false;
            break;

          case LEFT_BRACKET:
            if (t.scanOperand) {
                // Array initialiser.  Parse using recursive descent, as the
                // sub-grammar here is not an operator grammar.
                var fi, iter, elms, x2;
                n = new Node(t, ARRAY_INIT);
                elms = 0
                while ((tt = t.peek()) != RIGHT_BRACKET) {
                    elms++;
                    if (tt == COMMA) {
                        t.get();
                        n.push(null);
                        continue;
                    }
                    n.push(Expression(t, x, COMMA));
                    if (t.match(FOR)) { // array comprehensions
                        if (elms !== 1)
                            throw t.newSyntaxError("Invalid comprehension");
                        fi = new Node(t, FOR_IN);
                        if (t.match(IDENTIFIER)) {
                            if (t.token.value !== "each")
                                throw t.newSyntaxError("Invalid comprehension");
                            else
                                n.foreach = true;
                        }
                        t.mustMatch(LEFT_PAREN);
                        // x.inForLoopInit = true;  won't work because this FOR
                        // may be inside another expression => parenLevel !== 0
                        x2 = new CompilerContext(x.inFunction);
                        x2.inForLoopInit = true;
                        iter = Expression(t, x2);
                        if (iter.type !== IDENTIFIER)
                            throw t.newSyntaxError("Invalid comprehension");
                        fi.iterator = iter;
                        t.mustMatch(IN);
                        fi.object = Expression(t, x);
                        t.mustMatch(RIGHT_PAREN);
                        if (t.match(IF)) fi.condition = Expression(t, x);
                        break;
                    }
                    if (!t.match(COMMA)) break;
                }
                t.mustMatch(RIGHT_BRACKET);
                operands.push(n);
                t.scanOperand = false;
            } else {
                // Property indexing operator.
                operators.push(new Node(t, INDEX));
                t.scanOperand = true;
                ++x.bracketLevel;
            }
            break;

          case RIGHT_BRACKET:
            if (t.scanOperand || x.bracketLevel == bl)
                break loop;
            while (reduce().type != INDEX)
                continue;
            --x.bracketLevel;
            break;

          case LEFT_CURLY:
            if (!t.scanOperand) break loop;
            // Object initialiser.  As for array initialisers (see above),
            // parse using recursive descent.
            ++x.curlyLevel;
            n = new Node(t, OBJECT_INIT);
          object_init:
            if (!t.match(RIGHT_CURLY)) {
                do {
                    tt = t.get();
                    if ((t.token.value == "get" || t.token.value == "set") &&
                        t.peek() == IDENTIFIER) {
                        if (x.ecma3OnlyMode)
                            throw t.newSyntaxError("Illegal property accessor");
                        n.push(FunctionDefinition(t, x, true, EXPRESSED_FORM));
                    } else {
                        switch (tt) {
                          case IDENTIFIER:
                          case NUMBER:
                          case STRING:
                            id = new Node(t);
                            break;
                          case RIGHT_CURLY:
                            if (x.ecma3OnlyMode)
                                throw t.newSyntaxError("Illegal trailing ,");
                            break object_init;
                          default:
                            throw t.newSyntaxError("Invalid property name");
                        }
                        t.mustMatch(COLON);
                        n.push(new Node(t, PROPERTY_INIT, id,
                                        Expression(t, x, COMMA)));
                    }
                } while (t.match(COMMA));
                t.mustMatch(RIGHT_CURLY);
            }
            operands.push(n);
            t.scanOperand = false;
            --x.curlyLevel;
            break;

          case RIGHT_CURLY:
            if (!t.scanOperand && x.curlyLevel != cl)
                throw "PANIC: right curly botch";
            break loop;

          case LEFT_PAREN:
            if (t.scanOperand) {
                operators.push(new Node(t, GROUP));
            } else {
                while (opPrecedence[operators.top().type] > opPrecedence[NEW])
                    reduce();

                // Handle () now, to regularize the n-ary case for n > 0.
                // We must set scanOperand in case there are arguments and
                // the first one is a regexp or unary+/-.
                n = operators.top();
                t.scanOperand = true;
                if (t.match(RIGHT_PAREN)) {
                    if (n.type == NEW) {
                        --operators.length;
                        n.push(operands.pop());
                    } else {
                        n = new Node(t, CALL, operands.pop(),
                                     new Node(t, LIST));
                    }
                    operands.push(n);
                    t.scanOperand = false;
                    break;
                }
                if (n.type == NEW)
                    n.type = NEW_WITH_ARGS;
                else
                    operators.push(new Node(t, CALL));
            }
            ++x.parenLevel;
            break;

          case RIGHT_PAREN:
            if (t.scanOperand || x.parenLevel == pl)
                break loop;
            while ((tt = reduce().type) != GROUP && tt != CALL &&
                   tt != NEW_WITH_ARGS) {
                continue;
            }
            if (tt != GROUP) {
                n = operands.top();
                if (n[1].type != COMMA)
                    n[1] = new Node(t, LIST, n[1]);
                else
                    n[1].type = LIST;
            }
            --x.parenLevel;
            break;

            // Automatic semicolon insertion means we may scan across a newline
            // and into the beginning of another statement.  If so, break out of
            // the while loop and let the t.scanOperand logic handle errors.
          default:
            break loop;
        }
    }

    if (x.hookLevel != hl)
        throw t.newSyntaxError("Missing : after ?");
    if (x.parenLevel != pl)
        throw t.newSyntaxError("Missing ) in parenthetical");
    if (x.bracketLevel != bl)
        throw t.newSyntaxError("Missing ] in index expression");
    if (t.scanOperand)
        throw t.newSyntaxError("Missing operand");

    // Resume default mode, scanning for operands, not operators.
    t.scanOperand = true;
    t.unget();
    while (operators.length)
        reduce();
    return operands.pop();
}

// file ptr, path to file, line number -> node
function parse(s, f, l) {
    var t = new Tokenizer(s, f, l);
    var x = new CompilerContext(false);
    var n = Script(t, x);
    if (!t.done)
        throw t.newSyntaxError("Syntax error");
    return n;
}

