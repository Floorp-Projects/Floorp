// |reftest| skip-if(!xulRuntime.shell)
function test() {

// Bug 632029: constant-folding
assertExpr("[x for each (x in y) if (false)]", compExpr(ident("x"), [compEachBlock(ident("x"), ident("y"))], lit(false), "legacy"));

// comprehensions

assertExpr("[ x         for (x in foo)]",
           compExpr(ident("x"), [compBlock(ident("x"), ident("foo"))], null, "legacy"));
assertExpr("[ [x,y]     for (x in foo) for (y in bar)]",
           compExpr(arrExpr([ident("x"), ident("y")]), [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar"))], null, "legacy"));
assertExpr("[ [x,y,z] for (x in foo) for (y in bar) for (z in baz)]",
           compExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                    [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar")), compBlock(ident("z"), ident("baz"))],
                    null,
                    "legacy"));

assertExpr("[ x         for (x in foo) if (p)]",
           compExpr(ident("x"), [compBlock(ident("x"), ident("foo"))], ident("p"), "legacy"));
assertExpr("[ [x,y]     for (x in foo) for (y in bar) if (p)]",
           compExpr(arrExpr([ident("x"), ident("y")]), [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar"))], ident("p"), "legacy"));
assertExpr("[ [x,y,z] for (x in foo) for (y in bar) for (z in baz) if (p) ]",
           compExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                    [compBlock(ident("x"), ident("foo")), compBlock(ident("y"), ident("bar")), compBlock(ident("z"), ident("baz"))],
                    ident("p"),
                    "legacy"));

assertExpr("[ x         for each (x in foo)]",
           compExpr(ident("x"), [compEachBlock(ident("x"), ident("foo"))], null, "legacy"));
assertExpr("[ [x,y]     for each (x in foo) for each (y in bar)]",
           compExpr(arrExpr([ident("x"), ident("y")]), [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar"))], null, "legacy"));
assertExpr("[ [x,y,z] for each (x in foo) for each (y in bar) for each (z in baz)]",
           compExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                    [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar")), compEachBlock(ident("z"), ident("baz"))],
                    null,
                    "legacy"));

assertExpr("[ x         for each (x in foo) if (p)]",
           compExpr(ident("x"), [compEachBlock(ident("x"), ident("foo"))], ident("p"), "legacy"));
assertExpr("[ [x,y]     for each (x in foo) for each (y in bar) if (p)]",
           compExpr(arrExpr([ident("x"), ident("y")]), [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar"))], ident("p"), "legacy"));
assertExpr("[ [x,y,z] for each (x in foo) for each (y in bar) for each (z in baz) if (p) ]",
           compExpr(arrExpr([ident("x"), ident("y"), ident("z")]),
                    [compEachBlock(ident("x"), ident("foo")), compEachBlock(ident("y"), ident("bar")), compEachBlock(ident("z"), ident("baz"))],
                    ident("p"),
                    "legacy"));

// Comprehension expressions using for-of can be written in two different styles.
function assertLegacyAndModernArrayComp(expr, body, blocks, filter) {
    assertExpr(expr, compExpr(body, blocks, filter, "legacy"));

    // Transform the legacy comprehension to a modern comprehension and test it
    // that way too.
    let match = expr.match(/^\[(.*?) for (.*)\]$/);
    assertEq(match !== null, true);
    let expr2 = "[for " + match[2] + " " + match[1] + "]";
    assertExpr(expr2, compExpr(body, blocks, filter, "modern"));
}

assertLegacyAndModernArrayComp("[ x         for (x of foo)]",
                               ident("x"), [compOfBlock(ident("x"), ident("foo"))], null);
assertLegacyAndModernArrayComp("[ [x,y]     for (x of foo) for (y of bar)]",
                               arrExpr([ident("x"), ident("y")]), [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar"))], null);
assertLegacyAndModernArrayComp("[ [x,y,z] for (x of foo) for (y of bar) for (z of baz)]",
                               arrExpr([ident("x"), ident("y"), ident("z")]),
                               [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar")), compOfBlock(ident("z"), ident("baz"))],
                               null);

assertLegacyAndModernArrayComp("[ x         for (x of foo) if (p)]",
                               ident("x"), [compOfBlock(ident("x"), ident("foo"))], ident("p"));
assertLegacyAndModernArrayComp("[ [x,y]     for (x of foo) for (y of bar) if (p)]",
                               arrExpr([ident("x"), ident("y")]), [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar"))], ident("p"));
assertLegacyAndModernArrayComp("[ [x,y,z] for (x of foo) for (y of bar) for (z of baz) if (p) ]",
                               arrExpr([ident("x"), ident("y"), ident("z")]),
                               [compOfBlock(ident("x"), ident("foo")), compOfBlock(ident("y"), ident("bar")), compOfBlock(ident("z"), ident("baz"))],
                               ident("p"));

// Modern comprehensions with multiple ComprehensionIf.

assertExpr("[for (x of foo) x]",
           compExpr(ident("x"), [compOfBlock(ident("x"), ident("foo"))], null, "modern"));
assertExpr("[for (x of foo) if (c1) x]",
           compExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                 compIf(ident("c1"))], null, "modern"));
assertExpr("[for (x of foo) if (c1) if (c2) x]",
           compExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                 compIf(ident("c1")), compIf(ident("c2"))], null, "modern"));

assertExpr("[for (x of foo) if (c1) for (y of bar) x]",
           compExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                 compIf(ident("c1")),
                                 compOfBlock(ident("y"), ident("bar"))], null, "modern"));
assertExpr("[for (x of foo) if (c1) for (y of bar) if (c2) x]",
           compExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                 compIf(ident("c1")),
                                 compOfBlock(ident("y"), ident("bar")),
                                 compIf(ident("c2"))], null, "modern"));
assertExpr("[for (x of foo) if (c1) if (c2) for (y of bar) if (c3) if (c4) x]",
           compExpr(ident("x"), [compOfBlock(ident("x"), ident("foo")),
                                 compIf(ident("c1")), compIf(ident("c2")),
                                 compOfBlock(ident("y"), ident("bar")),
                                 compIf(ident("c3")), compIf(ident("c4"))], null, "modern"));

assertExpr("[for (x of y) if (false) for (z of w) if (0) x]",
           compExpr(ident("x"), [compOfBlock(ident("x"), ident("y")),
                                 compIf(lit(false)),
                                 compOfBlock(ident("z"), ident("w")),
                                 compIf(lit(0))], null, "modern"));

}

runtest(test);
