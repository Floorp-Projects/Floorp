// |reftest| skip-if(!xulRuntime.shell)
function test() {

// Translate legacy genexprs into less legacy genexprs and test them.
function assertFormerlyES6GenExpr(expr, body, blocks, filter) {
    let match = expr.match(/^\((.*?) for (.*)\)$/);
    assertEq(match !== null, true);
    let expr2 = "(for " + match[2] + " " + match[1] + ")";
    assertExpr(expr2, genExpr(body, blocks, filter, "modern"));
}

assertFormerlyES6GenExpr("( x         for (x of foo))",
                         ident("x"), [compOfBlock(ident("x"), ident("foo"))], null);
assertFormerlyES6GenExpr("( [x,y]     for (x of foo) for (y of bar))",
                         arrExpr([ident("x"), ident("y")]), [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar"))], null);
assertFormerlyES6GenExpr("( [x,y,z] for (x of foo) for (y of bar) for (z of baz))",
                         arrExpr([ident("x"), ident("y"), ident("z")]),
                         [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar")), compOfBlock(ident("z"), ident("baz"))],
                         null);

assertFormerlyES6GenExpr("( x         for (x of foo) if (p))",
                         ident("x"), [compOfBlock(ident("x"), ident("foo"))], ident("p"));
assertFormerlyES6GenExpr("( [x,y]     for (x of foo) for (y of bar) if (p))",
                          arrExpr([ident("x"), ident("y")]), [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar"))], ident("p"));
assertFormerlyES6GenExpr("( [x,y,z] for (x of foo) for (y of bar) for (z of baz) if (p) )",
                         arrExpr([ident("x"), ident("y"), ident("z")]),
                         [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar")), compOfBlock(ident("z"), ident("baz"))],
                         ident("p"));

// FormerlyES6 generator comprehension with multiple ComprehensionIf.

assertExpr("(for (x of foo) x)",
           genExpr(ident("x"), [compOfBlock(ident("x"), ident("foo"))], null, "modern"));
assertExpr("(for (x of foo) if (c1) x)",
           genExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                compIf(ident("c1"))], null, "modern"));
assertExpr("(for (x of foo) if (c1) if (c2) x)",
           genExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                compIf(ident("c1")), compIf(ident("c2"))], null, "modern"));

assertExpr("(for (x of foo) if (c1) for (y of bar) x)",
           genExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                compIf(ident("c1")),
                                compOfBlock(ident("y"), ident("bar"))], null, "modern"));
assertExpr("(for (x of foo) if (c1) for (y of bar) if (c2) x)",
           genExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                compIf(ident("c1")),
                                compOfBlock(ident("y"), ident("bar")),
                                compIf(ident("c2"))], null, "modern"));
assertExpr("(for (x of foo) if (c1) if (c2) for (y of bar) if (c3) if (c4) x)",
           genExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                compIf(ident("c1")), compIf(ident("c2")),
                                compOfBlock(ident("y"), ident("bar")),
                                compIf(ident("c3")), compIf(ident("c4"))], null, "modern"));

// NOTE: it would be good to test generator expressions both with and without upvars, just like functions above.

}

runtest(test);
