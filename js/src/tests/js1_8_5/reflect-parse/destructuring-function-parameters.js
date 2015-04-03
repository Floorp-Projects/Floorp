// |reftest| skip-if(!xulRuntime.shell)
function test() {

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
                             function(n) (objPatt([assignProp("a" + n, ident("x" + n)),
                                                   assignProp("b" + n, ident("y" + n)),
                                                   assignProp("c" + n, ident("z" + n))])));

testParamPatternCombinations(function(n) ("{a" + n + ":x" + n + " = 10," + "b" + n + ":y" + n + " = 10," + "c" + n + ":z" + n + " = 10}"),
                             function(n) (objPatt([assignProp("a" + n, ident("x" + n), lit(10)),
                                                   assignProp("b" + n, ident("y" + n), lit(10)),
                                                   assignProp("c" + n, ident("z" + n), lit(10))])));

testParamPatternCombinations(function(n) ("[x" + n + "," + "y" + n + "," + "z" + n + "]"),
                             function(n) (arrPatt([assignElem("x" + n), assignElem("y" + n), assignElem("z" + n)])));

testParamPatternCombinations(function(n) ("[a" + n + ", ..." + "b" + n + "]"),
                             function(n) (arrPatt([assignElem("a" + n), spread(ident("b" + n))])));

testParamPatternCombinations(function(n) ("[a" + n + ", " + "b" + n + " = 10]"),
                             function(n) (arrPatt([assignElem("a" + n), assignElem("b" + n, lit(10))])));

}

runtest(test);
