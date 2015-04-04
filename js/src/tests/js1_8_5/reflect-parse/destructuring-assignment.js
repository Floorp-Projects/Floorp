// |reftest| skip-if(!xulRuntime.shell)
function test() {

// destructuring assignment

function testAssignmentCombinations(makePattSrc, makePattPatt) {
    var pattSrcs = makePatternCombinations(n => ("x" + n + " = 0"), makePattSrc);
    var pattPatts = makePatternCombinations(n => (aExpr("=", ident("x" + n), lit(0))), makePattPatt);

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

testAssignmentCombinations(n => ("{a" + n + ":x" + n + "," + "b" + n + ":y" + n + "," + "c" + n + ":z" + n + "} = 0"),
                           n => (aExpr("=",
                                       objPatt([assignProp("a" + n, ident("x" + n)),
                                                assignProp("b" + n, ident("y" + n)),
                                                assignProp("c" + n, ident("z" + n))]),
                                       lit(0))));

}

runtest(test);
