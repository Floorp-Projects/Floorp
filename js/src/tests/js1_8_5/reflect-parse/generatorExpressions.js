// |reftest| skip-if(!xulRuntime.shell)
function test() {

// generator expressions

assertExpr("( x         for (x in foo))",
           genExpr(ident("x"), [compBlock(ident("x"), ident("foo"))], null, "legacy"));
assertExpr("( [x,y]     for (x in foo) for (y in bar))",
           genExpr(arrExpr([ident("x"), ident("y")]), [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar"))], null, "legacy"));
assertExpr("( [x,y,z] for (x in foo) for (y in bar) for (z in baz))",
           genExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                   [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar")), compBlock(ident("z"), ident("baz"))],
                   null,
                   "legacy"));

assertExpr("( x         for (x in foo) if (p))",
           genExpr(ident("x"), [compBlock(ident("x"), ident("foo"))], ident("p"), "legacy"));
assertExpr("( [x,y]     for (x in foo) for (y in bar) if (p))",
           genExpr(arrExpr([ident("x"), ident("y")]), [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar"))], ident("p"), "legacy"));
assertExpr("( [x,y,z] for (x in foo) for (y in bar) for (z in baz) if (p) )",
           genExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                   [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar")), compBlock(ident("z"), ident("baz"))],
                   ident("p"),
                   "legacy"));

assertExpr("( x         for each (x in foo))",
           genExpr(ident("x"), [compEachBlock(ident("x"), ident("foo"))], null, "legacy"));
assertExpr("( [x,y]     for each (x in foo) for each (y in bar))",
           genExpr(arrExpr([ident("x"), ident("y")]), [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar"))], null, "legacy"));
assertExpr("( [x,y,z] for each (x in foo) for each (y in bar) for each (z in baz))",
           genExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                   [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar")), compEachBlock(ident("z"), ident("baz"))],
                   null,
                   "legacy"));

assertExpr("( x         for each (x in foo) if (p))",
           genExpr(ident("x"), [compEachBlock(ident("x"), ident("foo"))], ident("p"), "legacy"));
assertExpr("( [x,y]     for each (x in foo) for each (y in bar) if (p))",
           genExpr(arrExpr([ident("x"), ident("y")]), [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar"))], ident("p"), "legacy"));
assertExpr("( [x,y,z] for each (x in foo) for each (y in bar) for each (z in baz) if (p) )",
           genExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                   [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar")), compEachBlock(ident("z"), ident("baz"))],
                   ident("p"),
                   "legacy"));

// Generator expressions using for-of can be written in two different styles.
function assertLegacyAndModernGenExpr(expr, body, blocks, filter) {
    assertExpr(expr, genExpr(body, blocks, filter, "legacy"));

    // Transform the legacy genexpr to a modern genexpr and test it that way
    // too.
    let match = expr.match(/^\((.*?) for (.*)\)$/);
    assertEq(match !== null, true);
    let expr2 = "(for " + match[2] + " " + match[1] + ")";
    assertExpr(expr2, genExpr(body, blocks, filter, "modern"));
}

assertLegacyAndModernGenExpr("( x         for (x of foo))",
                             ident("x"), [compOfBlock(ident("x"), ident("foo"))], null);
assertLegacyAndModernGenExpr("( [x,y]     for (x of foo) for (y of bar))",
                             arrExpr([ident("x"), ident("y")]), [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar"))], null);
assertLegacyAndModernGenExpr("( [x,y,z] for (x of foo) for (y of bar) for (z of baz))",
                             arrExpr([ident("x"), ident("y"), ident("z")]),
                             [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar")), compOfBlock(ident("z"), ident("baz"))],
                             null);

assertLegacyAndModernGenExpr("( x         for (x of foo) if (p))",
                             ident("x"), [compOfBlock(ident("x"), ident("foo"))], ident("p"));
assertLegacyAndModernGenExpr("( [x,y]     for (x of foo) for (y of bar) if (p))",
                             arrExpr([ident("x"), ident("y")]), [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar"))], ident("p"));
assertLegacyAndModernGenExpr("( [x,y,z] for (x of foo) for (y of bar) for (z of baz) if (p) )",
                             arrExpr([ident("x"), ident("y"), ident("z")]),
                             [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar")), compOfBlock(ident("z"), ident("baz"))],
                             ident("p"));

// Modern generator comprehension with multiple ComprehensionIf.

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
