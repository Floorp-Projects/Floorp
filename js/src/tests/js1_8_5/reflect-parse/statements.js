// |reftest| skip-if(!xulRuntime.shell)
function test() {

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
                   [],
		   catchClause(ident("e"), null, blockStmt([])),
                   null));
assertStmt("try { } catch (e) { } finally { }",
           tryStmt(blockStmt([]),
                   [],
		   catchClause(ident("e"), null, blockStmt([])),
                   blockStmt([])));
assertStmt("try { } finally { }",
           tryStmt(blockStmt([]),
                   [],
		   null,
                   blockStmt([])));
assertStmt("try { } catch (e if foo) { } catch (e if bar) { } finally { }",
           tryStmt(blockStmt([]),
                   [ catchClause(ident("e"), ident("foo"), blockStmt([])),
                     catchClause(ident("e"), ident("bar"), blockStmt([])) ],
		   null,
                   blockStmt([])));
assertStmt("try { } catch (e if foo) { } catch (e if bar) { } catch (e) { } finally { }",
           tryStmt(blockStmt([]),
                   [ catchClause(ident("e"), ident("foo"), blockStmt([])),
                     catchClause(ident("e"), ident("bar"), blockStmt([])) ],
                   catchClause(ident("e"), null, blockStmt([])),
                   blockStmt([])));


// Bug 632028: yield outside of a function should throw
(function() {
    var threw = false;
    try {
        Reflect.parse("yield 0");
    } catch (expected) {
        threw = true;
    }
    assertEq(threw, true);
})();

}

runtest(test);
