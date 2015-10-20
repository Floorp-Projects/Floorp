// |reftest| skip-if(!xulRuntime.shell)
function test() {

function testVarPatternCombinations(makePattSrc, makePattPatt) {
    var pattSrcs = makePatternCombinations(n => ("x" + n), makePattSrc);
    var pattPatts = makePatternCombinations(n => ({ id: ident("x" + n), init: null }), makePattPatt);
    // It's illegal to have uninitialized const declarations, so we need a
    // separate set of patterns and sources.
    var constSrcs = makePatternCombinations(n => ("x" + n + " = undefined"), makePattSrc);
    var constPatts = makePatternCombinations(n => ({ id: ident("x" + n), init: ident("undefined") }), makePattPatt);

    for (var i = 0; i < pattSrcs.length; i++) {
        // variable declarations in blocks
        assertDecl("var " + pattSrcs[i].join(",") + ";", varDecl(pattPatts[i]));

        assertGlobalDecl("let " + pattSrcs[i].join(",") + ";", letDecl(pattPatts[i]));
        assertLocalDecl("let " + pattSrcs[i].join(",") + ";", letDecl(pattPatts[i]));
        assertBlockDecl("let " + pattSrcs[i].join(",") + ";", letDecl(pattPatts[i]));

        assertDecl("const " + constSrcs[i].join(",") + ";", constDecl(constPatts[i]));

        // variable declarations in for-loop heads
        assertStmt("for (var " + pattSrcs[i].join(",") + "; foo; bar);",
                   forStmt(varDecl(pattPatts[i]), ident("foo"), ident("bar"), emptyStmt));
        assertStmt("for (let " + pattSrcs[i].join(",") + "; foo; bar);",
                   forStmt(letDecl(pattPatts[i]), ident("foo"), ident("bar"), emptyStmt));
        assertStmt("for (const " + constSrcs[i].join(",") + "; foo; bar);",
                   forStmt(constDecl(constPatts[i]), ident("foo"), ident("bar"), emptyStmt));
    }
}

testVarPatternCombinations(n => ("{a" + n + ":x" + n + "," + "b" + n + ":y" + n + "," + "c" + n + ":z" + n + "} = 0"),
                           n => ({ id: objPatt([assignProp("a" + n, ident("x" + n)),
                                                assignProp("b" + n, ident("y" + n)),
                                                assignProp("c" + n, ident("z" + n))]),
                                   init: lit(0) }));

testVarPatternCombinations(n => ("{a" + n + ":x" + n + " = 10," + "b" + n + ":y" + n + " = 10," + "c" + n + ":z" + n + " = 10} = 0"),
                           n => ({ id: objPatt([assignProp("a" + n, ident("x" + n), lit(10)),
                                                assignProp("b" + n, ident("y" + n), lit(10)),
                                                assignProp("c" + n, ident("z" + n), lit(10))]),
                                   init: lit(0) }));

testVarPatternCombinations(n => ("[x" + n + "," + "y" + n + "," + "z" + n + "] = 0"),
                           n => ({ id: arrPatt([assignElem("x" + n), assignElem("y" + n), assignElem("z" + n)]),
                                   init: lit(0) }));

testVarPatternCombinations(n => ("[a" + n + ", ..." + "b" + n + "] = 0"),
                           n => ({ id: arrPatt([assignElem("a" + n), spread(ident("b" + n))]),
                                   init: lit(0) }));

testVarPatternCombinations(n => ("[a" + n + ", " + "b" + n + " = 10] = 0"),
                           n => ({ id: arrPatt([assignElem("a" + n), assignElem("b" + n, lit(10))]),
                                   init: lit(0) }));

}

runtest(test);
