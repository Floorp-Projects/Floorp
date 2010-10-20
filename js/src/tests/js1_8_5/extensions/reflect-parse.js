/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var { Pattern, MatchError } = Match;

var _ = Pattern.ANY;

function program(elts) Pattern({ type: "Program", body: elts })
function exprStmt(expr) Pattern({ type: "ExpressionStatement", expression: expr })
function throwStmt(expr) Pattern({ type: "ThrowStatement", argument: expr })
function returnStmt(expr) Pattern({ type: "ReturnStatement", argument: expr })
function yieldExpr(expr) Pattern({ type: "YieldExpression", argument: expr })
function lit(val) Pattern({ type: "Literal", value: val })
var thisExpr = Pattern({ type: "ThisExpression" });
function funDecl(id, params, body) Pattern({ type: "FunctionDeclaration",
                                             id: id,
                                             params: params,
                                             body: body,
                                             generator: false })
function genFunDecl(id, params, body) Pattern({ type: "FunctionDeclaration",
                                                id: id,
                                                params: params,
                                                body: body,
                                                generator: true })
function varDecl(decls) Pattern({ type: "VariableDeclaration", declarations: decls, kind: "var" })
function letDecl(decls) Pattern({ type: "VariableDeclaration", declarations: decls, kind: "let" })
function constDecl(decls) Pattern({ type: "VariableDeclaration", declarations: decls, kind: "const" })
function ident(name) Pattern({ type: "Identifier", name: name })
function dotExpr(obj, id) Pattern({ type: "MemberExpression", computed: false, object: obj, property: id })
function memExpr(obj, id) Pattern({ type: "MemberExpression", computed: true, object: obj, property: id })
function forStmt(init, test, update, body) Pattern({ type: "ForStatement", init: init, test: test, update: update, body: body })
function forInStmt(lhs, rhs, body) Pattern({ type: "ForInStatement", left: lhs, right: rhs, body: body, each: false })
function forEachInStmt(lhs, rhs, body) Pattern({ type: "ForInStatement", left: lhs, right: rhs, body: body, each: true })
function breakStmt(lab) Pattern({ type: "BreakStatement", label: lab })
function continueStmt(lab) Pattern({ type: "ContinueStatement", label: lab })
function blockStmt(body) Pattern({ type: "BlockStatement", body: body })
var emptyStmt = Pattern({ type: "EmptyStatement" })
function ifStmt(test, cons, alt) Pattern({ type: "IfStatement", test: test, alternate: alt, consequent: cons })
function labStmt(lab, stmt) Pattern({ type: "LabeledStatement", label: lab, body: stmt })
function withStmt(obj, stmt) Pattern({ type: "WithStatement", object: obj, body: stmt })
function whileStmt(test, stmt) Pattern({ type: "WhileStatement", test: test, body: stmt })
function doStmt(stmt, test) Pattern({ type: "DoWhileStatement", test: test, body: stmt })
function switchStmt(disc, cases) Pattern({ type: "SwitchStatement", discriminant: disc, cases: cases })
function caseClause(test, stmts) Pattern({ type: "SwitchCase", test: test, consequent: stmts })
function defaultClause(stmts) Pattern({ type: "SwitchCase", test: null, consequent: stmts })
function catchClause(id, guard, body) Pattern({ type: "CatchClause", param: id, guard: guard, body: body })
function tryStmt(body, catches, fin) Pattern({ type: "TryStatement", block: body, handler: catches, finalizer: fin })
function letStmt(head, body) Pattern({ type: "LetStatement", head: head, body: body })
function funExpr(id, args, body, gen) Pattern({ type: "FunctionExpression",
                                                id: id,
                                                params: args,
                                                body: body,
                                                generator: false })
function genFunExpr(id, args, body) Pattern({ type: "FunctionExpression",
                                              id: id,
                                              params: args,
                                              body: body,
                                              generator: true })

function unExpr(op, arg) Pattern({ type: "UnaryExpression", operator: op, argument: arg })
function binExpr(op, left, right) Pattern({ type: "BinaryExpression", operator: op, left: left, right: right })
function aExpr(op, left, right) Pattern({ type: "AssignmentExpression", operator: op, left: left, right: right })
function updExpr(op, arg, prefix) Pattern({ type: "UpdateExpression", operator: op, argument: arg, prefix: prefix })
function logExpr(op, left, right) Pattern({ type: "LogicalExpression", operator: op, left: left, right: right })

function condExpr(test, cons, alt) Pattern({ type: "ConditionalExpression", test: test, consequent: cons, alternate: alt })
function seqExpr(exprs) Pattern({ type: "SequenceExpression", expressions: exprs })
function newExpr(callee, args) Pattern({ type: "NewExpression", callee: callee, arguments: args })
function callExpr(callee, args) Pattern({ type: "CallExpression", callee: callee, arguments: args })
function arrExpr(elts) Pattern({ type: "ArrayExpression", elements: elts })
function objExpr(elts) Pattern({ type: "ObjectExpression", properties: elts })
function compExpr(body, blocks, filter) Pattern({ type: "ComprehensionExpression", body: body, blocks: blocks, filter: filter })
function genExpr(body, blocks, filter) Pattern({ type: "GeneratorExpression", body: body, blocks: blocks, filter: filter })
function graphExpr(idx, body) Pattern({ type: "GraphExpression", index: idx, expression: body })
function letExpr(head, body) Pattern({ type: "LetExpression", head: head, body: body })
function idxExpr(idx) Pattern({ type: "GraphIndexExpression", index: idx })

function compBlock(left, right) Pattern({ type: "ComprehensionBlock", left: left, right: right, each: false })
function compEachBlock(left, right) Pattern({ type: "ComprehensionBlock", left: left, right: right, each: true })

function arrPatt(elts) Pattern({ type: "ArrayPattern", elements: elts })
function objPatt(elts) Pattern({ type: "ObjectPattern", properties: elts })

function localSrc(src) "(function(){ " + src + " })"
function localPatt(patt) program([exprStmt(funExpr(null, [], blockStmt([patt])))])
function blockSrc(src) "(function(){ { " + src + " } })"
function blockPatt(patt) program([exprStmt(funExpr(null, [], blockStmt([blockStmt([patt])])))])

var xmlAnyName = Pattern({ type: "XMLAnyName" });

function xmlQualId(left, right, computed) Pattern({ type: "XMLQualifiedIdentifier", left: left, right: right, computed: computed })
function xmlFuncQualId(right, computed) Pattern({ type: "XMLFunctionQualifiedIdentifier", right: right, computed: computed })
function xmlAttrSel(id) Pattern({ type: "XMLAttributeSelector", attribute: id })
function xmlFilter(left, right) Pattern({ type: "XMLFilterExpression", left: left, right: right })
function xmlPointTag(contents) Pattern({ type: "XMLPointTag", contents: contents })
function xmlStartTag(contents) Pattern({ type: "XMLStartTag", contents: contents })
function xmlEndTag(contents) Pattern({ type: "XMLEndTag", contents: contents })
function xmlEscape(expr) Pattern({ type: "XMLEscape", expression: expr })
function xmlElt(contents) Pattern({ type: "XMLElement", contents: contents })
function xmlAttr(value) Pattern({ type: "XMLAttribute", value: value })
function xmlText(text) Pattern({ type: "XMLText", text: text })
function xmlPI(target, contents) Pattern({ type: "XMLProcessingInstruction", target: target, contents: contents })
function xmlDefNS(ns) Pattern({ type: "XMLDefaultDeclaration", namespace: ns })
function xmlName(name) Pattern({ type: "XMLName", contents: name })
function xmlComment(contents) Pattern({ type: "XMLComment", contents: contents })
function xmlCdata(cdata) Pattern({ type: "XMLCdata", contents: cdata })

function assertBlockStmt(src, patt) {
    blockPatt(patt).assert(Reflect.parse(blockSrc(src)));
}

function assertBlockExpr(src, patt) {
    assertBlockStmt(src, exprStmt(patt));
}

function assertBlockDecl(src, patt) {
    blockPatt(patt).assert(Reflect.parse(blockSrc(src)));
}

function assertLocalStmt(src, patt) {
    localPatt(patt).assert(Reflect.parse(localSrc(src)));
}

function assertLocalExpr(src, patt) {
    assertLocalStmt(src, exprStmt(patt));
}

function assertLocalDecl(src, patt) {
    localPatt(patt).assert(Reflect.parse(localSrc(src)));
}

function assertGlobalStmt(src, patt) {
    program([patt]).assert(Reflect.parse(src));
}

function assertGlobalExpr(src, patt) {
    assertStmt(src, exprStmt(patt));
}

function assertGlobalDecl(src, patt) {
    program([patt]).assert(Reflect.parse(src));
}

function assertStmt(src, patt) {
    assertLocalStmt(src, patt);
    assertGlobalStmt(src, patt);
    assertBlockStmt(src, patt);
}

function assertExpr(src, patt) {
    assertLocalExpr(src, patt);
    assertGlobalExpr(src, patt);
    assertBlockExpr(src, patt);
}

function assertDecl(src, patt) {
    assertLocalDecl(src, patt);
    assertGlobalDecl(src, patt);
    assertBlockDecl(src, patt);
}

function assertError(src, errorType) {
    try {
        Reflect.parse(src);
    } catch (expected if expected instanceof errorType) {
        return;
    }
    throw new Error("expected " + errorType.name + " for " + uneval(src));
}

// general tests

// NB: These are useful but for now trace-test doesn't do I/O reliably.

//program(_).assert(Reflect.parse(snarf('data/flapjax.txt')));
//program(_).assert(Reflect.parse(snarf('data/jquery-1.4.2.txt')));
//program(_).assert(Reflect.parse(snarf('data/prototype.js')));
//program(_).assert(Reflect.parse(snarf('data/dojo.js.uncompressed.js')));
//program(_).assert(Reflect.parse(snarf('data/mootools-1.2.4-core-nc.js')));


// declarations

assertDecl("var x = 1, y = 2, z = 3",
           varDecl([{ id: ident("x"), init: lit(1) },
                    { id: ident("y"), init: lit(2) },
                    { id: ident("z"), init: lit(3) }]));
assertDecl("var x, y, z",
           varDecl([{ id: ident("x"), init: null },
                    { id: ident("y"), init: null },
                    { id: ident("z"), init: null }]));
assertDecl("function foo() { }",
           funDecl(ident("foo"), [], blockStmt([])));
assertDecl("function foo() { return 42 }",
           funDecl(ident("foo"), [], blockStmt([returnStmt(lit(42))])));


// Bug 591437: rebound args have their defs turned into uses
assertDecl("function f(a) { function a() { } }",
           funDecl(ident("f"), [ident("a")], blockStmt([funDecl(ident("a"), [], blockStmt([]))])));
assertDecl("function f(a,b,c) { function b() { } }",
           funDecl(ident("f"), [ident("a"),ident("b"),ident("c")], blockStmt([funDecl(ident("b"), [], blockStmt([]))])));
assertDecl("function f(a,[x,y]) { function a() { } }",
           funDecl(ident("f"),
                   [ident("a"), arrPatt([ident("x"), ident("y")])],
                   blockStmt([funDecl(ident("a"), [], blockStmt([]))])));

// Bug 591450: this test currently crashes because of a bug in jsparse
// assertDecl("function f(a,[x,y],b,[w,z],c) { function b() { } }",
//            funDecl(ident("f"),
//                    [ident("a"), arrPatt([ident("x"), ident("y")]), ident("b"), arrPatt([ident("w"), ident("z")]), ident("c")],
//                    blockStmt([funDecl(ident("b"), [], blockStmt([]))])));


// expressions

assertExpr("true", lit(true));
assertExpr("false", lit(false));
assertExpr("42", lit(42));
assertExpr("(/asdf/)", lit(/asdf/));
assertExpr("this", thisExpr);
assertExpr("foo", ident("foo"));
assertExpr("foo.bar", dotExpr(ident("foo"), ident("bar")));
assertExpr("foo[bar]", memExpr(ident("foo"), ident("bar")));
assertExpr("(function(){})", funExpr(null, [], blockStmt([])));
assertExpr("(function f() {})", funExpr(ident("f"), [], blockStmt([])));
assertExpr("(function f(x,y,z) {})", funExpr(ident("f"), [ident("x"),ident("y"),ident("z")], blockStmt([])));
assertExpr("(++x)", updExpr("++", ident("x"), true));
assertExpr("(x++)", updExpr("++", ident("x"), false));
assertExpr("(+x)", unExpr("+", ident("x")));
assertExpr("(-x)", unExpr("-", ident("x")));
assertExpr("(!x)", unExpr("!", ident("x")));
assertExpr("(~x)", unExpr("~", ident("x")));
assertExpr("(delete x)", unExpr("delete", ident("x")));
assertExpr("(typeof x)", unExpr("typeof", ident("x")));
assertExpr("(void x)", unExpr("void", ident("x")));
assertExpr("(x == y)", binExpr("==", ident("x"), ident("y")));
assertExpr("(x != y)", binExpr("!=", ident("x"), ident("y")));
assertExpr("(x === y)", binExpr("===", ident("x"), ident("y")));
assertExpr("(x !== y)", binExpr("!==", ident("x"), ident("y")));
assertExpr("(x < y)", binExpr("<", ident("x"), ident("y")));
assertExpr("(x <= y)", binExpr("<=", ident("x"), ident("y")));
assertExpr("(x > y)", binExpr(">", ident("x"), ident("y")));
assertExpr("(x >= y)", binExpr(">=", ident("x"), ident("y")));
assertExpr("(x << y)", binExpr("<<", ident("x"), ident("y")));
assertExpr("(x >> y)", binExpr(">>", ident("x"), ident("y")));
assertExpr("(x >>> y)", binExpr(">>>", ident("x"), ident("y")));
assertExpr("(x + y)", binExpr("+", ident("x"), ident("y")));
assertExpr("(w + x + y + z)", binExpr("+", ident("w"), binExpr("+", ident("x", binExpr("+", ident("y"), ident("z"))))))
assertExpr("(x - y)", binExpr("-", ident("x"), ident("y")));
assertExpr("(x * y)", binExpr("*", ident("x"), ident("y")));
assertExpr("(x / y)", binExpr("/", ident("x"), ident("y")));
assertExpr("(x % y)", binExpr("%", ident("x"), ident("y")));
assertExpr("(x | y)", binExpr("|", ident("x"), ident("y")));
assertExpr("(x ^ y)", binExpr("^", ident("x"), ident("y")));
assertExpr("(x & y)", binExpr("&", ident("x"), ident("y")));
assertExpr("(x in y)", binExpr("in", ident("x"), ident("y")));
assertExpr("(x instanceof y)", binExpr("instanceof", ident("x"), ident("y")));
assertExpr("(x = y)", aExpr("=", ident("x"), ident("y")));
assertExpr("(x += y)", aExpr("+=", ident("x"), ident("y")));
assertExpr("(x -= y)", aExpr("-=", ident("x"), ident("y")));
assertExpr("(x *= y)", aExpr("*=", ident("x"), ident("y")));
assertExpr("(x /= y)", aExpr("/=", ident("x"), ident("y")));
assertExpr("(x %= y)", aExpr("%=", ident("x"), ident("y")));
assertExpr("(x <<= y)", aExpr("<<=", ident("x"), ident("y")));
assertExpr("(x >>= y)", aExpr(">>=", ident("x"), ident("y")));
assertExpr("(x >>>= y)", aExpr(">>>=", ident("x"), ident("y")));
assertExpr("(x |= y)", aExpr("|=", ident("x"), ident("y")));
assertExpr("(x ^= y)", aExpr("^=", ident("x"), ident("y")));
assertExpr("(x &= y)", aExpr("&=", ident("x"), ident("y")));
assertExpr("(x || y)", logExpr("||", ident("x"), ident("y")));
assertExpr("(x && y)", logExpr("&&", ident("x"), ident("y")));
assertExpr("(w || x || y || z)", logExpr("||", ident("w"), logExpr("||", ident("x", logExpr("||", ident("y"), ident("z"))))))
assertExpr("(x ? y : z)", condExpr(ident("x"), ident("y"), ident("z")));
assertExpr("(x,y)", seqExpr([ident("x"),ident("y")]))
assertExpr("(x,y,z)", seqExpr([ident("x"),ident("y"),ident("z")]))
assertExpr("(a,b,c,d,e,f,g)", seqExpr([ident("a"),ident("b"),ident("c"),ident("d"),ident("e"),ident("f"),ident("g")]));
assertExpr("(new Object)", newExpr(ident("Object"), []));
assertExpr("(new Object())", newExpr(ident("Object"), []));
assertExpr("(new Object(42))", newExpr(ident("Object"), [lit(42)]));
assertExpr("(new Object(1,2,3))", newExpr(ident("Object"), [lit(1),lit(2),lit(3)]));
assertExpr("(String())", callExpr(ident("String"), []));
assertExpr("(String(42))", callExpr(ident("String"), [lit(42)]));
assertExpr("(String(1,2,3))", callExpr(ident("String"), [lit(1),lit(2),lit(3)]));
assertExpr("[]", arrExpr([]));
assertExpr("[1]", arrExpr([lit(1)]));
assertExpr("[1,2]", arrExpr([lit(1),lit(2)]));
assertExpr("[1,2,3]", arrExpr([lit(1),lit(2),lit(3)]));
assertExpr("[1,,2,3]", arrExpr([lit(1),null,lit(2),lit(3)]));
assertExpr("[1,,,2,3]", arrExpr([lit(1),null,null,lit(2),lit(3)]));
assertExpr("[1,,,2,,3]", arrExpr([lit(1),null,null,lit(2),null,lit(3)]));
assertExpr("[1,,,2,,,3]", arrExpr([lit(1),null,null,lit(2),null,null,lit(3)]));
assertExpr("[,1,2,3]", arrExpr([null,lit(1),lit(2),lit(3)]));
assertExpr("[,,1,2,3]", arrExpr([null,null,lit(1),lit(2),lit(3)]));
assertExpr("[,,,1,2,3]", arrExpr([null,null,null,lit(1),lit(2),lit(3)]));
assertExpr("[,,,1,2,3,]", arrExpr([null,null,null,lit(1),lit(2),lit(3)]));
assertExpr("[,,,1,2,3,,]", arrExpr([null,null,null,lit(1),lit(2),lit(3),null]));
assertExpr("[,,,1,2,3,,,]", arrExpr([null,null,null,lit(1),lit(2),lit(3),null,null]));
assertExpr("[,,,,,]", arrExpr([null,null,null,null,null]));
assertExpr("({})", objExpr([]));
assertExpr("({x:1})", objExpr([{ key: ident("x"), value: lit(1) }]));
assertExpr("({x:1, y:2})", objExpr([{ key: ident("x"), value: lit(1) },
                                    { key: ident("y"), value: lit(2) } ]));
assertExpr("({x:1, y:2, z:3})", objExpr([{ key: ident("x"), value: lit(1) },
                                         { key: ident("y"), value: lit(2) },
                                         { key: ident("z"), value: lit(3) } ]));
assertExpr("({x:1, 'y':2, z:3})", objExpr([{ key: ident("x"), value: lit(1) },
                                           { key: lit("y"), value: lit(2) },
                                           { key: ident("z"), value: lit(3) } ]));
assertExpr("({'x':1, 'y':2, z:3})", objExpr([{ key: lit("x"), value: lit(1) },
                                             { key: lit("y"), value: lit(2) },
                                             { key: ident("z"), value: lit(3) } ]));
assertExpr("({'x':1, 'y':2, 3:3})", objExpr([{ key: lit("x"), value: lit(1) },
                                             { key: lit("y"), value: lit(2) },
                                             { key: lit(3), value: lit(3) } ]));

// statements

assertStmt("throw 42", throwStmt(lit(42)));
assertStmt("for (;;) break", forStmt(null, null, null, breakStmt(null)));
assertStmt("for (x; y; z) break", forStmt(ident("x"), ident("y"), ident("z"), breakStmt(null)));
assertStmt("for (var x; y; z) break", forStmt(varDecl([{ id: ident("x"), init: null }]), ident("y"), ident("z")));
assertStmt("for (var x = 42; y; z) break", forStmt(varDecl([{ id: ident("x"), init: lit(42) }]), ident("y"), ident("z")));
assertStmt("for (x; ; z) break", forStmt(ident("x"), null, ident("z"), breakStmt(null)));
assertStmt("for (var x; ; z) break", forStmt(varDecl([{ id: ident("x"), init: null }]), null, ident("z")));
assertStmt("for (var x = 42; ; z) break", forStmt(varDecl([{ id: ident("x"), init: lit(42) }]), null, ident("z")));
assertStmt("for (x; y; ) break", forStmt(ident("x"), ident("y"), null, breakStmt(null)));
assertStmt("for (var x; y; ) break", forStmt(varDecl([{ id: ident("x"), init: null }]), ident("y"), null, breakStmt(null)));
assertStmt("for (var x = 42; y; ) break", forStmt(varDecl([{ id: ident("x"), init: lit(42) }]), ident("y"), null, breakStmt(null)));
assertStmt("for (var x in y) break", forInStmt(varDecl([{ id: ident("x"), init: null }]), ident("y"), breakStmt(null)));
assertStmt("for (x in y) break", forInStmt(ident("x"), ident("y"), breakStmt(null)));
assertStmt("{ }", blockStmt([]));
assertStmt("{ throw 1; throw 2; throw 3; }", blockStmt([ throwStmt(lit(1)), throwStmt(lit(2)), throwStmt(lit(3))]));
assertStmt(";", emptyStmt);
assertStmt("if (foo) throw 42;", ifStmt(ident("foo"), throwStmt(lit(42)), null));
assertStmt("if (foo) throw 42; else true;", ifStmt(ident("foo"), throwStmt(lit(42)), exprStmt(lit(true))));
assertStmt("if (foo) { throw 1; throw 2; throw 3; }",
           ifStmt(ident("foo"),
                  blockStmt([throwStmt(lit(1)), throwStmt(lit(2)), throwStmt(lit(3))]),
                  null));
assertStmt("if (foo) { throw 1; throw 2; throw 3; } else true;",
           ifStmt(ident("foo"),
                  blockStmt([throwStmt(lit(1)), throwStmt(lit(2)), throwStmt(lit(3))]),
                  exprStmt(lit(true))));
assertStmt("foo: for(;;) break foo;", labStmt(ident("foo"), forStmt(null, null, null, breakStmt(ident("foo")))));
assertStmt("foo: for(;;) continue foo;", labStmt(ident("foo"), forStmt(null, null, null, continueStmt(ident("foo")))));
assertStmt("with (obj) { }", withStmt(ident("obj"), blockStmt([])));
assertStmt("with (obj) { obj; }", withStmt(ident("obj"), blockStmt([exprStmt(ident("obj"))])));
assertStmt("while (foo) { }", whileStmt(ident("foo"), blockStmt([])));
assertStmt("while (foo) { foo; }", whileStmt(ident("foo"), blockStmt([exprStmt(ident("foo"))])));
assertStmt("do { } while (foo);", doStmt(blockStmt([]), ident("foo")));
assertStmt("do { foo; } while (foo)", doStmt(blockStmt([exprStmt(ident("foo"))]), ident("foo")));
assertStmt("switch (foo) { case 1: 1; break; case 2: 2; break; default: 3; }",
           switchStmt(ident("foo"),
                      [ caseClause(lit(1), [ exprStmt(lit(1)), breakStmt(null) ]),
                        caseClause(lit(2), [ exprStmt(lit(2)), breakStmt(null) ]),
                        defaultClause([ exprStmt(lit(3)) ]) ]));
assertStmt("switch (foo) { case 1: 1; break; case 2: 2; break; default: 3; case 42: 42; }",
           switchStmt(ident("foo"),
                      [ caseClause(lit(1), [ exprStmt(lit(1)), breakStmt(null) ]),
                        caseClause(lit(2), [ exprStmt(lit(2)), breakStmt(null) ]),
                        defaultClause([ exprStmt(lit(3)) ]),
                        caseClause(lit(42), [ exprStmt(lit(42)) ]) ]));
assertStmt("try { } catch (e) { }",
           tryStmt(blockStmt([]),
                   catchClause(ident("e"), null, blockStmt([])),
                   null));
assertStmt("try { } catch (e) { } finally { }",
           tryStmt(blockStmt([]),
                   catchClause(ident("e"), null, blockStmt([])),
                   blockStmt([])));
assertStmt("try { } finally { }",
           tryStmt(blockStmt([]),
                   null,
                   blockStmt([])));
assertStmt("try { } catch (e if foo) { } catch (e if bar) { } finally { }",
           tryStmt(blockStmt([]),
                   [ catchClause(ident("e"), ident("foo"), blockStmt([])),
                     catchClause(ident("e"), ident("bar"), blockStmt([])) ],
                   blockStmt([])));
assertStmt("try { } catch (e if foo) { } catch (e if bar) { } catch (e) { } finally { }",
           tryStmt(blockStmt([]),
                   [ catchClause(ident("e"), ident("foo"), blockStmt([])),
                     catchClause(ident("e"), ident("bar"), blockStmt([])),
                     catchClause(ident("e"), null, blockStmt([])) ],
                   blockStmt([])));

// redeclarations (TOK_NAME nodes with lexdef)

assertStmt("function f() { function g() { } function g() { } }",
           funDecl(ident("f"), [], blockStmt([funDecl(ident("g"), [], blockStmt([])),
                                              funDecl(ident("g"), [], blockStmt([]))])));

assertStmt("function f() { function g() { } function g() { return 42 } }",
           funDecl(ident("f"), [], blockStmt([funDecl(ident("g"), [], blockStmt([])),
                                              funDecl(ident("g"), [], blockStmt([returnStmt(lit(42))]))])));

assertStmt("function f() { var x = 42; var x = 43; }",
           funDecl(ident("f"), [], blockStmt([varDecl([{ id: ident("x"), init: lit(42) }]),
                                              varDecl([{ id: ident("x"), init: lit(43) }])])));


assertDecl("var {x:y} = foo;", varDecl([{ id: objPatt([{ key: ident("x"), value: ident("y") }]),
                                          init: ident("foo") }]));

// global let is var
assertGlobalDecl("let {x:y} = foo;", varDecl([{ id: objPatt([{ key: ident("x"), value: ident("y") }]),
                                                init: ident("foo") }]));
// function-global let is var
assertLocalDecl("let {x:y} = foo;", varDecl([{ id: objPatt([{ key: ident("x"), value: ident("y") }]),
                                               init: ident("foo") }]));
// block-local let is let
assertBlockDecl("let {x:y} = foo;", letDecl([{ id: objPatt([{ key: ident("x"), value: ident("y") }]),
                                               init: ident("foo") }]));

assertDecl("const {x:y} = foo;", constDecl([{ id: objPatt([{ key: ident("x"), value: ident("y") }]),
                                              init: ident("foo") }]));


// various combinations of identifiers and destructuring patterns:
function makePatternCombinations(id, destr)
    [
      [ id(1)                                            ],
      [ id(1),    id(2)                                  ],
      [ id(1),    id(2),    id(3)                        ],
      [ id(1),    id(2),    id(3),    id(4)              ],
      [ id(1),    id(2),    id(3),    id(4),    id(5)    ],

      [ destr(1)                                         ],
      [ destr(1), destr(2)                               ],
      [ destr(1), destr(2), destr(3)                     ],
      [ destr(1), destr(2), destr(3), destr(4)           ],
      [ destr(1), destr(2), destr(3), destr(4), destr(5) ],

      [ destr(1), id(2)                                  ],

      [ destr(1), id(2),    id(3)                        ],
      [ destr(1), id(2),    id(3),    id(4)              ],
      [ destr(1), id(2),    id(3),    id(4),    id(5)    ],
      [ destr(1), id(2),    id(3),    id(4),    destr(5) ],
      [ destr(1), id(2),    id(3),    destr(4)           ],
      [ destr(1), id(2),    id(3),    destr(4), id(5)    ],
      [ destr(1), id(2),    id(3),    destr(4), destr(5) ],

      [ destr(1), id(2),    destr(3)                     ],
      [ destr(1), id(2),    destr(3), id(4)              ],
      [ destr(1), id(2),    destr(3), id(4),    id(5)    ],
      [ destr(1), id(2),    destr(3), id(4),    destr(5) ],
      [ destr(1), id(2),    destr(3), destr(4)           ],
      [ destr(1), id(2),    destr(3), destr(4), id(5)    ],
      [ destr(1), id(2),    destr(3), destr(4), destr(5) ],

      [ id(1),    destr(2)                               ],

      [ id(1),    destr(2), id(3)                        ],
      [ id(1),    destr(2), id(3),    id(4)              ],
      [ id(1),    destr(2), id(3),    id(4),    id(5)    ],
      [ id(1),    destr(2), id(3),    id(4),    destr(5) ],
      [ id(1),    destr(2), id(3),    destr(4)           ],
      [ id(1),    destr(2), id(3),    destr(4), id(5)    ],
      [ id(1),    destr(2), id(3),    destr(4), destr(5) ],

      [ id(1),    destr(2), destr(3)                     ],
      [ id(1),    destr(2), destr(3), id(4)              ],
      [ id(1),    destr(2), destr(3), id(4),    id(5)    ],
      [ id(1),    destr(2), destr(3), id(4),    destr(5) ],
      [ id(1),    destr(2), destr(3), destr(4)           ],
      [ id(1),    destr(2), destr(3), destr(4), id(5)    ],
      [ id(1),    destr(2), destr(3), destr(4), destr(5) ]
    ]

// destructuring function parameters

function testParamPatternCombinations(makePattSrc, makePattPatt) {
    var pattSrcs = makePatternCombinations(function(n) ("x" + n), makePattSrc);
    var pattPatts = makePatternCombinations(function(n) (ident("x" + n)), makePattPatt);

    for (var i = 0; i < pattSrcs.length; i++) {
        function makeSrc(body) ("(function(" + pattSrcs[i].join(",") + ") " + body + ")")
        function makePatt(body) (funExpr(null, pattPatts[i], body))

        // no upvars, block body
        assertExpr(makeSrc("{ }", makePatt(blockStmt([]))));
        // upvars, block body
        assertExpr(makeSrc("{ return [x1,x2,x3,x4,x5]; }"),
                   makePatt(blockStmt([returnStmt(arrExpr([ident("x1"), ident("x2"), ident("x3"), ident("x4"), ident("x5")]))])));
        // no upvars, expression body
        assertExpr(makeSrc("(0)"), makePatt(lit(0)));
        // upvars, expression body
        assertExpr(makeSrc("[x1,x2,x3,x4,x5]"),
                   makePatt(arrExpr([ident("x1"), ident("x2"), ident("x3"), ident("x4"), ident("x5")])));
    }
}

testParamPatternCombinations(function(n) ("{a" + n + ":x" + n + "," + "b" + n + ":y" + n + "," + "c" + n + ":z" + n + "}"),
                             function(n) (objPatt([{ key: ident("a" + n), value: ident("x" + n) },
                                                   { key: ident("b" + n), value: ident("y" + n) },
                                                   { key: ident("c" + n), value: ident("z" + n) }])));

testParamPatternCombinations(function(n) ("[x" + n + "," + "y" + n + "," + "z" + n + "]"),
                             function(n) (arrPatt([ident("x" + n), ident("y" + n), ident("z" + n)])));


// destructuring variable declarations

function testVarPatternCombinations(makePattSrc, makePattPatt) {
    var pattSrcs = makePatternCombinations(function(n) ("x" + n), makePattSrc);
    var pattPatts = makePatternCombinations(function(n) ({ id: ident("x" + n), init: null }), makePattPatt);

    for (var i = 0; i < pattSrcs.length; i++) {
        // variable declarations in blocks
        assertDecl("var " + pattSrcs[i].join(",") + ";", varDecl(pattPatts[i]));

        assertGlobalDecl("let " + pattSrcs[i].join(",") + ";", varDecl(pattPatts[i]));
        assertLocalDecl("let " + pattSrcs[i].join(",") + ";", varDecl(pattPatts[i]));
        assertBlockDecl("let " + pattSrcs[i].join(",") + ";", letDecl(pattPatts[i]));

        assertDecl("const " + pattSrcs[i].join(",") + ";", constDecl(pattPatts[i]));

        // variable declarations in for-loop heads
        assertStmt("for (var " + pattSrcs[i].join(",") + "; foo; bar);",
                   forStmt(varDecl(pattPatts[i]), ident("foo"), ident("bar"), emptyStmt));
        assertStmt("for (let " + pattSrcs[i].join(",") + "; foo; bar);",
                   forStmt(letDecl(pattPatts[i]), ident("foo"), ident("bar"), emptyStmt));
        assertStmt("for (const " + pattSrcs[i].join(",") + "; foo; bar);",
                   forStmt(constDecl(pattPatts[i]), ident("foo"), ident("bar"), emptyStmt));
    }
}

testVarPatternCombinations(function (n) ("{a" + n + ":x" + n + "," + "b" + n + ":y" + n + "," + "c" + n + ":z" + n + "} = 0"),
                           function (n) ({ id: objPatt([{ key: ident("a" + n), value: ident("x" + n) },
                                                        { key: ident("b" + n), value: ident("y" + n) },
                                                        { key: ident("c" + n), value: ident("z" + n) }]),
                                           init: lit(0) }));

testVarPatternCombinations(function(n) ("[x" + n + "," + "y" + n + "," + "z" + n + "] = 0"),
                           function(n) ({ id: arrPatt([ident("x" + n), ident("y" + n), ident("z" + n)]),
                                          init: lit(0) }));

// destructuring assignment

function testAssignmentCombinations(makePattSrc, makePattPatt) {
    var pattSrcs = makePatternCombinations(function(n) ("x" + n + " = 0"), makePattSrc);
    var pattPatts = makePatternCombinations(function(n) (aExpr("=", ident("x" + n), lit(0))), makePattPatt);

    for (var i = 0; i < pattSrcs.length; i++) {
        var src = pattSrcs[i].join(",");
        var patt = pattPatts[i].length === 1 ? pattPatts[i][0] : seqExpr(pattPatts[i]);

        // assignment expression statement
        assertExpr("(" + src + ")", patt);

        // for-loop head assignment
        assertStmt("for (" + src + "; foo; bar);",
                   forStmt(patt, ident("foo"), ident("bar"), emptyStmt));
    }
}

testAssignmentCombinations(function (n) ("{a" + n + ":x" + n + "," + "b" + n + ":y" + n + "," + "c" + n + ":z" + n + "} = 0"),
                           function (n) (aExpr("=",
                                               objPatt([{ key: ident("a" + n), value: ident("x" + n) },
                                                        { key: ident("b" + n), value: ident("y" + n) },
                                                        { key: ident("c" + n), value: ident("z" + n) }]),
                                               lit(0))));


// destructuring in for-in and for-each-in loop heads

var axbycz = objPatt([{ key: ident("a"), value: ident("x") },
                      { key: ident("b"), value: ident("y") },
                      { key: ident("c"), value: ident("z") }]);
var xyz = arrPatt([ident("x"), ident("y"), ident("z")]);

assertStmt("for (var {a:x,b:y,c:z} in foo);", forInStmt(varDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let {a:x,b:y,c:z} in foo);", forInStmt(letDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ({a:x,b:y,c:z} in foo);", forInStmt(axbycz, ident("foo"), emptyStmt));
assertStmt("for (var [x,y,z] in foo);", forInStmt(varDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let [x,y,z] in foo);", forInStmt(letDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ([x,y,z] in foo);", forInStmt(xyz, ident("foo"), emptyStmt));
assertStmt("for each (var {a:x,b:y,c:z} in foo);", forEachInStmt(varDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each (let {a:x,b:y,c:z} in foo);", forEachInStmt(letDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each ({a:x,b:y,c:z} in foo);", forEachInStmt(axbycz, ident("foo"), emptyStmt));
assertStmt("for each (var [x,y,z] in foo);", forEachInStmt(varDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each (let [x,y,z] in foo);", forEachInStmt(letDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each ([x,y,z] in foo);", forEachInStmt(xyz, ident("foo"), emptyStmt));
assertError("for (const x in foo);", SyntaxError);
assertError("for (const {a:x,b:y,c:z} in foo);", SyntaxError);
assertError("for (const [x,y,z] in foo);", SyntaxError);
assertError("for each (const x in foo);", SyntaxError);
assertError("for each (const {a:x,b:y,c:z} in foo);", SyntaxError);
assertError("for each (const [x,y,z] in foo);", SyntaxError);

// destructuring in for-in and for-each-in loop heads with initializers

assertStmt("for (var {a:x,b:y,c:z} = 22 in foo);", forInStmt(varDecl([{ id: axbycz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for (let {a:x,b:y,c:z} = 22 in foo);", forInStmt(letDecl([{ id: axbycz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for (var [x,y,z] = 22 in foo);", forInStmt(varDecl([{ id: xyz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for (let [x,y,z] = 22 in foo);", forInStmt(letDecl([{ id: xyz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for each (var {a:x,b:y,c:z} = 22 in foo);", forEachInStmt(varDecl([{ id: axbycz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for each (let {a:x,b:y,c:z} = 22 in foo);", forEachInStmt(letDecl([{ id: axbycz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for each (var [x,y,z] = 22 in foo);", forEachInStmt(varDecl([{ id: xyz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for each (let [x,y,z] = 22 in foo);", forEachInStmt(letDecl([{ id: xyz, init: lit(22) }]), ident("foo"), emptyStmt));
assertError("for (x = 22 in foo);", SyntaxError);
assertError("for ({a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for ([x,y,z] = 22 in foo);", SyntaxError);
assertError("for (const x = 22 in foo);", SyntaxError);
assertError("for (const {a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for (const [x,y,z] = 22 in foo);", SyntaxError);
assertError("for each (const x = 22 in foo);", SyntaxError);
assertError("for each (const {a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for each (const [x,y,z] = 22 in foo);", SyntaxError);

// expression closures

assertDecl("function inc(x) (x + 1)", funDecl(ident("inc"), [ident("x")], binExpr("+", ident("x"), lit(1))));
assertExpr("(function(x) (x+1))", funExpr(null, [ident("x")], binExpr("+"), ident("x"), lit(1)));

// generators

assertDecl("function gen(x) { yield }", genFunDecl(ident("gen"), [ident("x")], blockStmt([exprStmt(yieldExpr(null))])));
assertExpr("(function(x) { yield })", genFunExpr(null, [ident("x")], blockStmt([exprStmt(yieldExpr(null))])));
assertDecl("function gen(x) { yield 42 }", genFunDecl(ident("gen"), [ident("x")], blockStmt([exprStmt(yieldExpr(lit(42)))])));
assertExpr("(function(x) { yield 42 })", genFunExpr(null, [ident("x")], blockStmt([exprStmt(yieldExpr(lit(42)))])));

// getters and setters

assertExpr("({ get x() { return 42 } })",
           objExpr([ { key: ident("x"),
                       value: funExpr(null, [], blockStmt([returnStmt(lit(42))])),
                       kind: "get" } ]));
assertExpr("({ set x(v) { return 42 } })",
           objExpr([ { key: ident("x"),
                       value: funExpr(null, [ident("v")], blockStmt([returnStmt(lit(42))])),
                       kind: "set" } ]));

// comprehensions

assertExpr("[ x         for (x in foo)]",
           compExpr(ident("x"), [compBlock(ident("x"), ident("foo"))], null));
assertExpr("[ [x,y]     for (x in foo) for (y in bar)]",
           compExpr(arrExpr([ident("x"), ident("y")]), [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar"))], null));
assertExpr("[ [x,y,z] for (x in foo) for (y in bar) for (z in baz)]",
           compExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                    [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar")), compBlock(ident("z"), ident("baz"))],
                    null));

assertExpr("[ x         for (x in foo) if (p)]",
           compExpr(ident("x"), [compBlock(ident("x"), ident("foo"))], ident("p")));
assertExpr("[ [x,y]     for (x in foo) for (y in bar) if (p)]",
           compExpr(arrExpr([ident("x"), ident("y")]), [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar"))], ident("p")));
assertExpr("[ [x,y,z] for (x in foo) for (y in bar) for (z in baz) if (p) ]",
           compExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                    [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar")), compBlock(ident("z"), ident("baz"))],
                    ident("p")));

assertExpr("[ x         for each (x in foo)]",
           compExpr(ident("x"), [compEachBlock(ident("x"), ident("foo"))], null));
assertExpr("[ [x,y]     for each (x in foo) for each (y in bar)]",
           compExpr(arrExpr([ident("x"), ident("y")]), [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar"))], null));
assertExpr("[ [x,y,z] for each (x in foo) for each (y in bar) for each (z in baz)]",
           compExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                    [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar")), compEachBlock(ident("z"), ident("baz"))],
                    null));

assertExpr("[ x         for each (x in foo) if (p)]",
           compExpr(ident("x"), [compEachBlock(ident("x"), ident("foo"))], ident("p")));
assertExpr("[ [x,y]     for each (x in foo) for each (y in bar) if (p)]",
           compExpr(arrExpr([ident("x"), ident("y")]), [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar"))], ident("p")));
assertExpr("[ [x,y,z] for each (x in foo) for each (y in bar) for each (z in baz) if (p) ]",
           compExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                    [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar")), compEachBlock(ident("z"), ident("baz"))],
                    ident("p")));

// generator expressions

assertExpr("( x         for (x in foo))",
           genExpr(ident("x"), [compBlock(ident("x"), ident("foo"))], null));
assertExpr("( [x,y]     for (x in foo) for (y in bar))",
           genExpr(arrExpr([ident("x"), ident("y")]), [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar"))], null));
assertExpr("( [x,y,z] for (x in foo) for (y in bar) for (z in baz))",
           genExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                   [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar")), compBlock(ident("z"), ident("baz"))],
                   null));

assertExpr("( x         for (x in foo) if (p))",
           genExpr(ident("x"), [compBlock(ident("x"), ident("foo"))], ident("p")));
assertExpr("( [x,y]     for (x in foo) for (y in bar) if (p))",
           genExpr(arrExpr([ident("x"), ident("y")]), [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar"))], ident("p")));
assertExpr("( [x,y,z] for (x in foo) for (y in bar) for (z in baz) if (p) )",
           genExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                   [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar")), compBlock(ident("z"), ident("baz"))],
                   ident("p")));

assertExpr("( x         for each (x in foo))",
           genExpr(ident("x"), [compEachBlock(ident("x"), ident("foo"))], null));
assertExpr("( [x,y]     for each (x in foo) for each (y in bar))",
           genExpr(arrExpr([ident("x"), ident("y")]), [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar"))], null));
assertExpr("( [x,y,z] for each (x in foo) for each (y in bar) for each (z in baz))",
           genExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                   [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar")), compEachBlock(ident("z"), ident("baz"))],
                   null));

assertExpr("( x         for each (x in foo) if (p))",
           genExpr(ident("x"), [compEachBlock(ident("x"), ident("foo"))], ident("p")));
assertExpr("( [x,y]     for each (x in foo) for each (y in bar) if (p))",
           genExpr(arrExpr([ident("x"), ident("y")]), [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar"))], ident("p")));
assertExpr("( [x,y,z] for each (x in foo) for each (y in bar) for each (z in baz) if (p) )",
           genExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                   [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar")), compEachBlock(ident("z"), ident("baz"))],
                   ident("p")));

// NOTE: it would be good to test generator expressions both with and without upvars, just like functions above.


// sharp variables

assertExpr("#1={me:#1#}", graphExpr(1, objExpr([{ key: ident("me"), value: idxExpr(1) }])));

// let expressions

assertExpr("(let (x=1) x)", letExpr([{ id: ident("x"), init: lit(1) }], ident("x")));
assertExpr("(let (x=1,y=2) y)", letExpr([{ id: ident("x"), init: lit(1) },
                                         { id: ident("y"), init: lit(2) }],
                                        ident("y")));
assertExpr("(let (x=1,y=2,z=3) z)", letExpr([{ id: ident("x"), init: lit(1) },
                                             { id: ident("y"), init: lit(2) },
                                             { id: ident("z"), init: lit(3) }],
                                            ident("z")));
assertExpr("(let (x) x)", letExpr([{ id: ident("x"), init: null }], ident("x")));
assertExpr("(let (x,y) y)", letExpr([{ id: ident("x"), init: null },
                                     { id: ident("y"), init: null }],
                                    ident("y")));
assertExpr("(let (x,y,z) z)", letExpr([{ id: ident("x"), init: null },
                                       { id: ident("y"), init: null },
                                       { id: ident("z"), init: null }],
                                      ident("z")));
assertExpr("(let (x = 1, y = x) y)", letExpr([{ id: ident("x"), init: lit(1) },
                                              { id: ident("y"), init: ident("x") }],
                                             ident("y")));
assertError("(let (x = 1, x = 2) x)", TypeError);

// let statements

assertStmt("let (x=1) { }", letStmt([{ id: ident("x"), init: lit(1) }], blockStmt([])));
assertStmt("let (x=1,y=2) { }", letStmt([{ id: ident("x"), init: lit(1) },
                                         { id: ident("y"), init: lit(2) }],
                                        blockStmt([])));
assertStmt("let (x=1,y=2,z=3) { }", letStmt([{ id: ident("x"), init: lit(1) },
                                             { id: ident("y"), init: lit(2) },
                                             { id: ident("z"), init: lit(3) }],
                                            blockStmt([])));
assertStmt("let (x) { }", letStmt([{ id: ident("x"), init: null }], blockStmt([])));
assertStmt("let (x,y) { }", letStmt([{ id: ident("x"), init: null },
                                     { id: ident("y"), init: null }],
                                    blockStmt([])));
assertStmt("let (x,y,z) { }", letStmt([{ id: ident("x"), init: null },
                                       { id: ident("y"), init: null },
                                       { id: ident("z"), init: null }],
                                      blockStmt([])));
assertStmt("let (x = 1, y = x) { }", letStmt([{ id: ident("x"), init: lit(1) },
                                              { id: ident("y"), init: ident("x") }],
                                             blockStmt([])));
assertError("let (x = 1, x = 2) { }", TypeError);


// E4X

assertExpr("x..tagName", binExpr("..", ident("x"), lit("tagName")));
assertExpr("x.*", memExpr(ident("x"), xmlAnyName));
assertExpr("x[*]", memExpr(ident("x"), xmlAnyName));
assertExpr("x::y", xmlQualId(ident("x"), ident("y"), false));
assertExpr("x::[foo]", xmlQualId(ident("x"), ident("foo"), true));
assertExpr("x::[foo()]", xmlQualId(ident("x"), callExpr(ident("foo"), []), true));
assertExpr("*::*", xmlQualId(xmlAnyName, ident("*"), false));
assertExpr("*::[foo]", xmlQualId(xmlAnyName, ident("foo"), true));
assertExpr("*::[foo()]", xmlQualId(xmlAnyName, callExpr(ident("foo"), []), true));
assertExpr("function::x", xmlFuncQualId(ident("x"), false));
assertExpr("function::[foo]", xmlFuncQualId(ident("foo"), true));
assertExpr("@foo", xmlAttrSel(ident("foo")));
assertExpr("x.(p)", xmlFilter(ident("x"), ident("p")));
assertExpr("<{foo}/>", xmlPointTag([xmlEscape(ident("foo"))]));
assertExpr("<{foo}></{foo}>", xmlElt([xmlStartTag([xmlEscape(ident("foo"))]),
                                      xmlEndTag([xmlEscape(ident("foo"))])]));
assertExpr("<{foo} {attr}='attr'/>", xmlPointTag([xmlEscape(ident("foo")),
                                                  xmlEscape(ident("attr")),
                                                  xmlAttr("attr")]));
assertExpr("<{foo}>text</{foo}>", xmlElt([xmlStartTag([xmlEscape(ident("foo"))]),
                                          xmlText("text"),
                                          xmlEndTag([xmlEscape(ident("foo"))])]));
assertExpr("<?xml?>", xmlPI("xml", ""));
assertExpr("<?xml version='1.0'?>", xmlPI("xml", "version='1.0'"));
assertDecl("default xml namespace = 'js';", xmlDefNS(lit("js")));
assertDecl("default xml namespace = foo;", xmlDefNS(ident("foo")));

// The parser turns these into TOK_UNARY nodes with pn_op == JSOP_SETXMLNAME.

assertExpr("x::y = foo", aExpr("=", xmlQualId(ident("x"), ident("y"), false), ident("foo")));
assertExpr("function::x = foo", aExpr("=", xmlFuncQualId(ident("x"), false), ident("foo")));
assertExpr("@x = foo", aExpr("=", xmlAttrSel(ident("x")), ident("foo")));
assertExpr("x::* = foo", aExpr("=", xmlQualId(ident("x"), ident("*"), false), ident("foo")));
assertExpr("*::* = foo", aExpr("=", xmlQualId(xmlAnyName, ident("*"), false), ident("foo")));
assertExpr("x.* = foo", aExpr("=", memExpr(ident("x"), xmlAnyName), ident("foo")));
assertExpr("x[*] = foo", aExpr("=", memExpr(ident("x"), xmlAnyName), ident("foo")));

assertExpr("x::y += foo", aExpr("+=", xmlQualId(ident("x"), ident("y"), false), ident("foo")));
assertExpr("function::x += foo", aExpr("+=", xmlFuncQualId(ident("x"), false), ident("foo")));
assertExpr("@x += foo", aExpr("+=", xmlAttrSel(ident("x")), ident("foo")));
assertExpr("x::* += foo", aExpr("+=", xmlQualId(ident("x"), ident("*"), false), ident("foo")));
assertExpr("*::* += foo", aExpr("+=", xmlQualId(xmlAnyName, ident("*"), false), ident("foo")));
assertExpr("x.* += foo", aExpr("+=", memExpr(ident("x"), xmlAnyName), ident("foo")));
assertExpr("x[*] += foo", aExpr("+=", memExpr(ident("x"), xmlAnyName), ident("foo")));

assertExpr("x::y++", updExpr("++", xmlQualId(ident("x"), ident("y"), false), false));
assertExpr("function::x++", updExpr("++", xmlFuncQualId(ident("x"), false), false));
assertExpr("@x++", updExpr("++", xmlAttrSel(ident("x")), false));
assertExpr("x::*++", updExpr("++", xmlQualId(ident("x"), ident("*"), false), false));
assertExpr("*::*++", updExpr("++", xmlQualId(xmlAnyName, ident("*"), false), false));
assertExpr("x.*++", updExpr("++", memExpr(ident("x"), xmlAnyName), false));
assertExpr("x[*]++", updExpr("++", memExpr(ident("x"), xmlAnyName), false));

assertExpr("++x::y", updExpr("++", xmlQualId(ident("x"), ident("y"), false), true));
assertExpr("++function::x", updExpr("++", xmlFuncQualId(ident("x"), false), true));
assertExpr("++@x", updExpr("++", xmlAttrSel(ident("x")), true));
assertExpr("++x::*", updExpr("++", xmlQualId(ident("x"), ident("*"), false), true));
assertExpr("++*::*", updExpr("++", xmlQualId(xmlAnyName, ident("*"), false), true));
assertExpr("++x.*", updExpr("++", memExpr(ident("x"), xmlAnyName), true));
assertExpr("++x[*]", updExpr("++", memExpr(ident("x"), xmlAnyName), true));


// The parser turns these into TOK_UNARY nodes with pn_op == JSOP_BINDXMLNAME.

function singletonObjPatt(name, val) objPatt([{ key: ident(name), value: val }])

assertExpr("({a:x::y}) = foo", aExpr("=", singletonObjPatt("a", xmlQualId(ident("x"), ident("y"), false)), ident("foo")));
assertExpr("({a:function::x}) = foo", aExpr("=", singletonObjPatt("a", xmlFuncQualId(ident("x"), false)), ident("foo")));
assertExpr("({a:@x}) = foo", aExpr("=", singletonObjPatt("a", xmlAttrSel(ident("x"))), ident("foo")));
assertExpr("({a:x::*}) = foo", aExpr("=", singletonObjPatt("a", xmlQualId(ident("x"), ident("*"), false)), ident("foo")));
assertExpr("({a:*::*}) = foo", aExpr("=", singletonObjPatt("a", xmlQualId(xmlAnyName, ident("*"), false)), ident("foo")));
assertExpr("({a:x.*}) = foo", aExpr("=", singletonObjPatt("a", memExpr(ident("x"), xmlAnyName)), ident("foo")));
assertExpr("({a:x[*]}) = foo", aExpr("=", singletonObjPatt("a", memExpr(ident("x"), xmlAnyName)), ident("foo")));

function emptyForInPatt(val, rhs) forInStmt(val, rhs, emptyStmt)

assertStmt("for (x::y in foo);", emptyForInPatt(xmlQualId(ident("x"), ident("y"), false), ident("foo")));
assertStmt("for (function::x in foo);", emptyForInPatt(xmlFuncQualId(ident("x"), false), ident("foo")));
assertStmt("for (@x in foo);", emptyForInPatt(xmlAttrSel(ident("x")), ident("foo")));
assertStmt("for (x::* in foo);", emptyForInPatt(xmlQualId(ident("x"), ident("*"), false), ident("foo")));
assertStmt("for (*::* in foo);", emptyForInPatt(xmlQualId(xmlAnyName, ident("*"), false), ident("foo")));
assertStmt("for (x.* in foo);", emptyForInPatt(memExpr(ident("x"), xmlAnyName), ident("foo")));
assertStmt("for (x[*] in foo);", emptyForInPatt(memExpr(ident("x"), xmlAnyName), ident("foo")));


// I'm not quite sure why, but putting XML in the callee of a call expression is
// the only way I've found to be able to preserve TOK_XMLNAME, TOK_XMLSPACE,
// TOK_XMLCDATA, and TOK_XMLCOMMENT parse nodes.

assertExpr("(<x> </x>)()", callExpr(xmlElt([xmlStartTag([xmlName("x")]),
                                            xmlText(" "),
                                            xmlEndTag([xmlName("x")])]),
                                    []));
assertExpr("(<x>    </x>)()", callExpr(xmlElt([xmlStartTag([xmlName("x")]),
                                               xmlText("    "),
                                               xmlEndTag([xmlName("x")])]),
                                       []));
assertExpr("(<x><![CDATA[hello, world]]></x>)()", callExpr(xmlElt([xmlStartTag([xmlName("x")]),
                                                                   xmlCdata("hello, world"),
                                                                   xmlEndTag([xmlName("x")])]),
                                                           []));
assertExpr("(<x><!-- hello, world --></x>)()", callExpr(xmlElt([xmlStartTag([xmlName("x")]),
                                                                xmlComment(" hello, world "),
                                                                xmlEndTag([xmlName("x")])]),
                                                        []));


// Source location information


var withoutFileOrLine = Reflect.parse("42");
var withFile = Reflect.parse("42", {source:"foo.js"});
var withFileAndLine = Reflect.parse("42", {source:"foo.js", line:111});

Pattern({ source: null, start: { line: 1, column: 0 }, end: { line: 1, column: 2 } }).match(withoutFileOrLine.loc);
Pattern({ source: "foo.js", start: { line: 1, column: 0 }, end: { line: 1, column: 2 } }).match(withFile.loc);
Pattern({ source: "foo.js", start: { line: 111, column: 0 }, end: { line: 111, column: 2 } }).match(withFileAndLine.loc);

var withoutFileOrLine2 = Reflect.parse("foo +\nbar");
var withFile2 = Reflect.parse("foo +\nbar", {source:"foo.js"});
var withFileAndLine2 = Reflect.parse("foo +\nbar", {source:"foo.js", line:111});

Pattern({ source: null, start: { line: 1, column: 0 }, end: { line: 2, column: 3 } }).match(withoutFileOrLine2.loc);
Pattern({ source: "foo.js", start: { line: 1, column: 0 }, end: { line: 2, column: 3 } }).match(withFile2.loc);
Pattern({ source: "foo.js", start: { line: 111, column: 0 }, end: { line: 112, column: 3 } }).match(withFileAndLine2.loc);

var nested = Reflect.parse("(-b + sqrt(sqr(b) - 4 * a * c)) / (2 * a)", {source:"quad.js"});
var fourAC = nested.body[0].expression.left.right.arguments[0].right;

Pattern({ source: "quad.js", start: { line: 1, column: 20 }, end: { line: 1, column: 29 } }).match(fourAC.loc);


// No source location

assertEq(Reflect.parse("42", {loc:false}).loc, null);
program([exprStmt(lit(42))]).assert(Reflect.parse("42", {loc:false}));


reportCompare(true, true);
