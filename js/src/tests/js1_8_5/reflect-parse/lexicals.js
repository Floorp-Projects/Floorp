// |reftest| skip-if(!xulRuntime.shell)
function test() {

// global let is var
assertGlobalDecl("let {x:y} = foo;", varDecl([{ id: objPatt([assignProp("x", ident("y"))]),
                                                init: ident("foo") }]));
// function-global let is let
assertLocalDecl("let {x:y} = foo;", letDecl([{ id: objPatt([assignProp("x", ident("y"))]),
                                               init: ident("foo") }]));
// block-local let is let
assertBlockDecl("let {x:y} = foo;", letDecl([{ id: objPatt([assignProp("x", ident("y"))]),
                                               init: ident("foo") }]));

assertDecl("const {x:y} = foo;", constDecl([{ id: objPatt([assignProp("x", ident("y"))]),
                                              init: ident("foo") }]));


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

}

runtest(test);
