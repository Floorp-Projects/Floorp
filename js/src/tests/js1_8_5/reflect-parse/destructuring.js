function test() {

// Bug 632027: array holes should reflect as null
assertExpr("[,]=[,]", aExpr("=", arrPatt([null]), arrExpr([null])));

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

// destructuring variable declarations

function testVarPatternCombinations(makePattSrc, makePattPatt) {
    var pattSrcs = makePatternCombinations(function(n) ("x" + n), makePattSrc);
    var pattPatts = makePatternCombinations(function(n) ({ id: ident("x" + n), init: null }), makePattPatt);
    // It's illegal to have uninitialized const declarations, so we need a
    // separate set of patterns and sources.
    var constSrcs = makePatternCombinations(function(n) ("x" + n + " = undefined"), makePattSrc);
    var constPatts = makePatternCombinations(function(n) ({ id: ident("x" + n), init: ident("undefined") }), makePattPatt);

    for (var i = 0; i < pattSrcs.length; i++) {
        // variable declarations in blocks
        assertDecl("var " + pattSrcs[i].join(",") + ";", varDecl(pattPatts[i]));

        assertGlobalDecl("let " + pattSrcs[i].join(",") + ";", varDecl(pattPatts[i]));
        assertLocalDecl("let " + pattSrcs[i].join(",") + ";", letDecl(pattPatts[i]));
        assertBlockDecl("let " + pattSrcs[i].join(",") + ";", letDecl(pattPatts[i]));

        assertDecl("const " + constSrcs[i].join(",") + ";", constDecl(constPatts[i]));

        // variable declarations in for-loop heads
        assertStmt("for (var " + pattSrcs[i].join(",") + "; foo; bar);",
                   forStmt(varDecl(pattPatts[i]), ident("foo"), ident("bar"), emptyStmt));
        assertStmt("for (let " + pattSrcs[i].join(",") + "; foo; bar);",
                   letStmt(pattPatts[i], forStmt(null, ident("foo"), ident("bar"), emptyStmt)));
        assertStmt("for (const " + constSrcs[i].join(",") + "; foo; bar);",
                   letStmt(constPatts[i], forStmt(null, ident("foo"), ident("bar"), emptyStmt)));
    }
}

testVarPatternCombinations(function (n) ("{a" + n + ":x" + n + "," + "b" + n + ":y" + n + "," + "c" + n + ":z" + n + "} = 0"),
                           function (n) ({ id: objPatt([assignProp("a" + n, ident("x" + n)),
                                                        assignProp("b" + n, ident("y" + n)),
                                                        assignProp("c" + n, ident("z" + n))]),
                                           init: lit(0) }));

testVarPatternCombinations(function (n) ("{a" + n + ":x" + n + " = 10," + "b" + n + ":y" + n + " = 10," + "c" + n + ":z" + n + " = 10} = 0"),
                           function (n) ({ id: objPatt([assignProp("a" + n, ident("x" + n), lit(10)),
                                                        assignProp("b" + n, ident("y" + n), lit(10)),
                                                        assignProp("c" + n, ident("z" + n), lit(10))]),
                                           init: lit(0) }));

testVarPatternCombinations(function(n) ("[x" + n + "," + "y" + n + "," + "z" + n + "] = 0"),
                           function(n) ({ id: arrPatt([assignElem("x" + n), assignElem("y" + n), assignElem("z" + n)]),
                                          init: lit(0) }));

testVarPatternCombinations(function(n) ("[a" + n + ", ..." + "b" + n + "] = 0"),
                           function(n) ({ id: arrPatt([assignElem("a" + n), spread(ident("b" + n))]),
                                          init: lit(0) }));

testVarPatternCombinations(function(n) ("[a" + n + ", " + "b" + n + " = 10] = 0"),
                           function(n) ({ id: arrPatt([assignElem("a" + n), assignElem("b" + n, lit(10))]),
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
                                               objPatt([assignProp("a" + n, ident("x" + n)),
                                                        assignProp("b" + n, ident("y" + n)),
                                                        assignProp("c" + n, ident("z" + n))]),
                                               lit(0))));


// destructuring in for-in and for-each-in loop heads

var axbycz = objPatt([assignProp("a", ident("x")),
                      assignProp("b", ident("y")),
                      assignProp("c", ident("z"))]);
var xyz = arrPatt([assignElem("x"), assignElem("y"), assignElem("z")]);

assertStmt("for (var {a:x,b:y,c:z} in foo);", forInStmt(varDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let {a:x,b:y,c:z} in foo);", forInStmt(letDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ({a:x,b:y,c:z} in foo);", forInStmt(axbycz, ident("foo"), emptyStmt));
assertStmt("for (var [x,y,z] in foo);", forInStmt(varDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let [x,y,z] in foo);", forInStmt(letDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ([x,y,z] in foo);", forInStmt(xyz, ident("foo"), emptyStmt));
assertStmt("for (var {a:x,b:y,c:z} of foo);", forOfStmt(varDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let {a:x,b:y,c:z} of foo);", forOfStmt(letDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ({a:x,b:y,c:z} of foo);", forOfStmt(axbycz, ident("foo"), emptyStmt));
assertStmt("for (var [x,y,z] of foo);", forOfStmt(varDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for (let [x,y,z] of foo);", forOfStmt(letDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for ([x,y,z] of foo);", forOfStmt(xyz, ident("foo"), emptyStmt));
assertStmt("for each (var {a:x,b:y,c:z} in foo);", forEachInStmt(varDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each (let {a:x,b:y,c:z} in foo);", forEachInStmt(letDecl([{ id: axbycz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each ({a:x,b:y,c:z} in foo);", forEachInStmt(axbycz, ident("foo"), emptyStmt));
assertStmt("for each (var [x,y,z] in foo);", forEachInStmt(varDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each (let [x,y,z] in foo);", forEachInStmt(letDecl([{ id: xyz, init: null }]), ident("foo"), emptyStmt));
assertStmt("for each ([x,y,z] in foo);", forEachInStmt(xyz, ident("foo"), emptyStmt));
assertError("for (const x in foo);", SyntaxError);
assertError("for (const {a:x,b:y,c:z} in foo);", SyntaxError);
assertError("for (const [x,y,z] in foo);", SyntaxError);
assertError("for (const x of foo);", SyntaxError);
assertError("for (const {a:x,b:y,c:z} of foo);", SyntaxError);
assertError("for (const [x,y,z] of foo);", SyntaxError);
assertError("for each (const x in foo);", SyntaxError);
assertError("for each (const {a:x,b:y,c:z} in foo);", SyntaxError);
assertError("for each (const [x,y,z] in foo);", SyntaxError);

// destructuring in for-in and for-each-in loop heads with initializers

assertStmt("for (var {a:x,b:y,c:z} = 22 in foo);", forInStmt(varDecl([{ id: axbycz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for (var [x,y,z] = 22 in foo);", forInStmt(varDecl([{ id: xyz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for each (var {a:x,b:y,c:z} = 22 in foo);", forEachInStmt(varDecl([{ id: axbycz, init: lit(22) }]), ident("foo"), emptyStmt));
assertStmt("for each (var [x,y,z] = 22 in foo);", forEachInStmt(varDecl([{ id: xyz, init: lit(22) }]), ident("foo"), emptyStmt));
assertError("for (x = 22 in foo);", SyntaxError);
assertError("for ({a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for ([x,y,z] = 22 in foo);", SyntaxError);
assertError("for (const x = 22 in foo);", SyntaxError);
assertError("for (const {a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for (const [x,y,z] = 22 in foo);", SyntaxError);
assertError("for each (const x = 22 in foo);", SyntaxError);
assertError("for each (const {a:x,b:y,c:z} = 22 in foo);", SyntaxError);
assertError("for each (const [x,y,z] = 22 in foo);", SyntaxError);

}

runtest(test);
