// |reftest| skip-if(!xulRuntime.shell)
function test() {

// Bug 632056: constant-folding
program([exprStmt(ident("f")),
         ifStmt(lit(1),
                blockStmt([funDecl(ident("f"), [], blockStmt([]))]),
                null)]).assert(Reflect.parse("f; if (1) function f(){}"));
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

assertDecl("function foo(...rest) { }",
           funDecl(ident("foo"), [], blockStmt([]), [], ident("rest")));

assertDecl("function foo(a=4) { }", funDecl(ident("foo"), [ident("a")], blockStmt([]), [lit(4)]));
assertDecl("function foo(a, b=4) { }", funDecl(ident("foo"), [ident("a"), ident("b")], blockStmt([]), [null, lit(4)]));
assertDecl("function foo(a, b=4, ...rest) { }",
           funDecl(ident("foo"), [ident("a"), ident("b")], blockStmt([]), [null, lit(4), null], ident("rest")));
assertDecl("function foo(a=(function () {})) { function a() {} }",
           funDecl(ident("foo"), [ident("a")], blockStmt([funDecl(ident("a"), [], blockStmt([]))]),
                   [funExpr(null, [], blockStmt([]))]));

// Bug 1018628: default paremeter for destructuring
assertDecl("function f(a=1, [x,y]=[2,3]) { }",
           funDecl(ident("f"),
                   [ident("a"), arrPatt([ident("x"), ident("y")])],
                   blockStmt([]),
                   [lit(1), arrExpr([lit(2), lit(3)])]));

// Bug 591437: rebound args have their defs turned into uses
assertDecl("function f(a) { function a() { } }",
           funDecl(ident("f"), [ident("a")], blockStmt([funDecl(ident("a"), [], blockStmt([]))])));
assertDecl("function f(a,b,c) { function b() { } }",
           funDecl(ident("f"), [ident("a"),ident("b"),ident("c")], blockStmt([funDecl(ident("b"), [], blockStmt([]))])));
assertDecl("function f(a,[x,y]) { function a() { } }",
           funDecl(ident("f"),
                   [ident("a"), arrPatt([assignElem("x"), assignElem("y")])],
                   blockStmt([funDecl(ident("a"), [], blockStmt([]))])));

// Bug 591450: this test currently crashes because of a bug in jsparse
// assertDecl("function f(a,[x,y],b,[w,z],c) { function b() { } }",
//            funDecl(ident("f"),
//                    [ident("a"), arrPatt([ident("x"), ident("y")]), ident("b"), arrPatt([ident("w"), ident("z")]), ident("c")],
//                    blockStmt([funDecl(ident("b"), [], blockStmt([]))])));

// redeclarations (TOK_NAME nodes with lexdef)

assertStmt("function f() { function g() { } function g() { } }",
           funDecl(ident("f"), [], blockStmt([funDecl(ident("g"), [], blockStmt([])),
                                              funDecl(ident("g"), [], blockStmt([]))])));

// Fails due to parser quirks (bug 638577)
//assertStmt("function f() { function g() { } function g() { return 42 } }",
//           funDecl(ident("f"), [], blockStmt([funDecl(ident("g"), [], blockStmt([])),
//                                              funDecl(ident("g"), [], blockStmt([returnStmt(lit(42))]))])));

assertStmt("function f() { var x = 42; var x = 43; }",
           funDecl(ident("f"), [], blockStmt([varDecl([{ id: ident("x"), init: lit(42) }]),
                                              varDecl([{ id: ident("x"), init: lit(43) }])])));


assertDecl("var {x:y} = foo;", varDecl([{ id: objPatt([assignProp("x", ident("y"))]),
                                          init: ident("foo") }]));
assertDecl("var {x} = foo;", varDecl([{ id: objPatt([assignProp("x")]),
                                        init: ident("foo") }]));

// Bug 632030: redeclarations between var and funargs, var and function
assertStmt("function g(x) { var x }",
           funDecl(ident("g"), [ident("x")], blockStmt([varDecl([{ id: ident("x"), init: null }])])));
assertProg("f.p = 1; var f; f.p; function f(){}",
           [exprStmt(aExpr("=", dotExpr(ident("f"), ident("p")), lit(1))),
            varDecl([{ id: ident("f"), init: null }]),
            exprStmt(dotExpr(ident("f"), ident("p"))),
            funDecl(ident("f"), [], blockStmt([]))]);
}

assertBlockStmt("{ function f(x) {} }",
                blockStmt([funDecl(ident("f"), [ident("x")], blockStmt([]))]));

// Annex B semantics should not change parse tree.
assertBlockStmt("{ let f; { function f(x) {} } }",
                blockStmt([letDecl([{ id: ident("f"), init: null }]),
                           blockStmt([funDecl(ident("f"), [ident("x")], blockStmt([]))])]));

runtest(test);
