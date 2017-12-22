// |reftest| skip-if(!xulRuntime.shell)
function test() {

// primary expressions
assertExpr("true", lit(true));
assertExpr("false", lit(false));
assertExpr("42", lit(42));
assertExpr("(/asdf/)", lit(/asdf/));
assertExpr("this", thisExpr);
assertExpr("foo", ident("foo"));

// member expressions
assertExpr("foo.bar", dotExpr(ident("foo"), ident("bar")));
assertExpr("foo[bar]", memExpr(ident("foo"), ident("bar")));
assertExpr("foo['bar']", memExpr(ident("foo"), lit("bar")));
assertExpr("foo[42]", memExpr(ident("foo"), lit(42)));

// function expressions
assertExpr("(function(){})", funExpr(null, [], blockStmt([])));
assertExpr("(function f() {})", funExpr(ident("f"), [], blockStmt([])));
assertExpr("(function f(x,y,z) {})", funExpr(ident("f"), [ident("x"),ident("y"),ident("z")], blockStmt([])));
assertExpr("a => a", arrowExpr([ident("a")], ident("a")));
assertExpr("(a) => a", arrowExpr([ident("a")], ident("a")));
assertExpr("a => b => a", arrowExpr([ident("a")], arrowExpr([ident("b")], ident("a"))));
assertExpr("a => {}", arrowExpr([ident("a")], blockStmt([])));
assertExpr("a => ({})", arrowExpr([ident("a")], objExpr([])));
assertExpr("(a, b, c) => {}", arrowExpr([ident("a"), ident("b"), ident("c")], blockStmt([])));
assertExpr("([a, b]) => {}", arrowExpr([arrPatt([assignElem("a"), assignElem("b")])], blockStmt([])));

// Unary expressions
assertExpr("(++x)", updExpr("++", ident("x"), true));
assertExpr("(x++)", updExpr("++", ident("x"), false));
assertExpr("(+x)", unExpr("+", ident("x")));
assertExpr("(-x)", unExpr("-", ident("x")));
assertExpr("(!x)", unExpr("!", ident("x")));
assertExpr("(~x)", unExpr("~", ident("x")));
assertExpr("(delete x)", unExpr("delete", ident("x")));
assertExpr("(typeof x)", unExpr("typeof", ident("x")));
assertExpr("(void x)", unExpr("void", ident("x")));

// Bug 632026: constant-folding
assertExpr("typeof(0?0:a)", unExpr("typeof", condExpr(lit(0), lit(0), ident("a"))));

// Binary exporessions
assertExpr("(x == y)", binExpr("==", ident("x"), ident("y")));
assertExpr("(x != y)", binExpr("!=", ident("x"), ident("y")));
assertExpr("(x === y)", binExpr("===", ident("x"), ident("y")));
assertExpr("(x !== y)", binExpr("!==", ident("x"), ident("y")));
assertExpr("(x < y)", binExpr("<", ident("x"), ident("y")));
assertExpr("(x <= y)", binExpr("<=", ident("x"), ident("y")));
assertExpr("(x > y)", binExpr(">", ident("x"), ident("y")));
assertExpr("(x >= y)", binExpr(">=", ident("x"), ident("y")));
assertExpr("(x << y)", binExpr("<<", ident("x"), ident("y")));
assertExpr("(x >> y)", binExpr(">>", ident("x"), ident("y")));
assertExpr("(x >>> y)", binExpr(">>>", ident("x"), ident("y")));
assertExpr("(x + y)", binExpr("+", ident("x"), ident("y")));
assertExpr("(w + x + y + z)", binExpr("+", binExpr("+", binExpr("+", ident("w"), ident("x")), ident("y")), ident("z")));
assertExpr("(x - y)", binExpr("-", ident("x"), ident("y")));
assertExpr("(w - x - y - z)", binExpr("-", binExpr("-", binExpr("-", ident("w"), ident("x")), ident("y")), ident("z")));
assertExpr("(x * y)", binExpr("*", ident("x"), ident("y")));
assertExpr("(x / y)", binExpr("/", ident("x"), ident("y")));
assertExpr("(x % y)", binExpr("%", ident("x"), ident("y")));
assertExpr("(x | y)", binExpr("|", ident("x"), ident("y")));
assertExpr("(x ^ y)", binExpr("^", ident("x"), ident("y")));
assertExpr("(x & y)", binExpr("&", ident("x"), ident("y")));
assertExpr("(x in y)", binExpr("in", ident("x"), ident("y")));
assertExpr("(x instanceof y)", binExpr("instanceof", ident("x"), ident("y")));

// Bug 571617: eliminate constant-folding
assertExpr("2 + 3", binExpr("+", lit(2), lit(3)));

// Assignment expressions
assertExpr("(x = y)", aExpr("=", ident("x"), ident("y")));
assertExpr("(x += y)", aExpr("+=", ident("x"), ident("y")));
assertExpr("(x -= y)", aExpr("-=", ident("x"), ident("y")));
assertExpr("(x *= y)", aExpr("*=", ident("x"), ident("y")));
assertExpr("(x /= y)", aExpr("/=", ident("x"), ident("y")));
assertExpr("(x %= y)", aExpr("%=", ident("x"), ident("y")));
assertExpr("(x <<= y)", aExpr("<<=", ident("x"), ident("y")));
assertExpr("(x >>= y)", aExpr(">>=", ident("x"), ident("y")));
assertExpr("(x >>>= y)", aExpr(">>>=", ident("x"), ident("y")));
assertExpr("(x |= y)", aExpr("|=", ident("x"), ident("y")));
assertExpr("(x ^= y)", aExpr("^=", ident("x"), ident("y")));
assertExpr("(x &= y)", aExpr("&=", ident("x"), ident("y")));

// Conditional expressions
assertExpr("(x || y)", logExpr("||", ident("x"), ident("y")));
assertExpr("(x && y)", logExpr("&&", ident("x"), ident("y")));
assertExpr("(w || x || y || z)", logExpr("||", logExpr("||", logExpr("||", ident("w"), ident("x")), ident("y")), ident("z")))
assertExpr("(x ? y : z)", condExpr(ident("x"), ident("y"), ident("z")));

// Sequence expressions
assertExpr("(x,y)", seqExpr([ident("x"),ident("y")]))
assertExpr("(x,y,z)", seqExpr([ident("x"),ident("y"),ident("z")]))
assertExpr("(a,b,c,d,e,f,g)", seqExpr([ident("a"),ident("b"),ident("c"),ident("d"),ident("e"),ident("f"),ident("g")]));

// Call expressions
assertExpr("(new Object)", newExpr(ident("Object"), []));
assertExpr("(new Object())", newExpr(ident("Object"), []));
assertExpr("(new Object(42))", newExpr(ident("Object"), [lit(42)]));
assertExpr("(new Object(1,2,3))", newExpr(ident("Object"), [lit(1),lit(2),lit(3)]));
assertExpr("(String())", callExpr(ident("String"), []));
assertExpr("(String(42))", callExpr(ident("String"), [lit(42)]));
assertExpr("(String(1,2,3))", callExpr(ident("String"), [lit(1),lit(2),lit(3)]));

// Array expressions
assertExpr("[]", arrExpr([]));
assertExpr("[1]", arrExpr([lit(1)]));
assertExpr("[1,2]", arrExpr([lit(1),lit(2)]));
assertExpr("[1,2,3]", arrExpr([lit(1),lit(2),lit(3)]));
assertExpr("[1,,2,3]", arrExpr([lit(1),null,lit(2),lit(3)]));
assertExpr("[1,,,2,3]", arrExpr([lit(1),null,null,lit(2),lit(3)]));
assertExpr("[1,,,2,,3]", arrExpr([lit(1),null,null,lit(2),null,lit(3)]));
assertExpr("[1,,,2,,,3]", arrExpr([lit(1),null,null,lit(2),null,null,lit(3)]));
assertExpr("[,1,2,3]", arrExpr([null,lit(1),lit(2),lit(3)]));
assertExpr("[,,1,2,3]", arrExpr([null,null,lit(1),lit(2),lit(3)]));
assertExpr("[,,,1,2,3]", arrExpr([null,null,null,lit(1),lit(2),lit(3)]));
assertExpr("[,,,1,2,3,]", arrExpr([null,null,null,lit(1),lit(2),lit(3)]));
assertExpr("[,,,1,2,3,,]", arrExpr([null,null,null,lit(1),lit(2),lit(3),null]));
assertExpr("[,,,1,2,3,,,]", arrExpr([null,null,null,lit(1),lit(2),lit(3),null,null]));
assertExpr("[,,,,,]", arrExpr([null,null,null,null,null]));
assertExpr("[1, ...a, 2]", arrExpr([lit(1), spread(ident("a")), lit(2)]));
assertExpr("[,, ...a,, ...b, 42]", arrExpr([null,null, spread(ident("a")),, spread(ident("b")), lit(42)]));
assertExpr("[1,(2,3)]", arrExpr([lit(1),seqExpr([lit(2),lit(3)])]));
assertExpr("[,(2,3)]", arrExpr([null,seqExpr([lit(2),lit(3)])]));

// Object expressions
assertExpr("({})", objExpr([]));
assertExpr("({x:1})", objExpr([{ key: ident("x"), value: lit(1) }]));
assertExpr("({x:x, y})", objExpr([{ key: ident("x"), value: ident("x"), shorthand: false },
                                  { key: ident("y"), value: ident("y"), shorthand: true }]));
assertExpr("({x:1, y:2})", objExpr([{ key: ident("x"), value: lit(1) },
                                    { key: ident("y"), value: lit(2) } ]));
assertExpr("({x:1, y:2, z:3})", objExpr([{ key: ident("x"), value: lit(1) },
                                         { key: ident("y"), value: lit(2) },
                                         { key: ident("z"), value: lit(3) } ]));
assertExpr("({x:1, 'y':2, z:3})", objExpr([{ key: ident("x"), value: lit(1) },
                                           { key: lit("y"), value: lit(2) },
                                           { key: ident("z"), value: lit(3) } ]));
assertExpr("({'x':1, 'y':2, z:3})", objExpr([{ key: lit("x"), value: lit(1) },
                                             { key: lit("y"), value: lit(2) },
                                             { key: ident("z"), value: lit(3) } ]));
assertExpr("({'x':1, 'y':2, 3:3})", objExpr([{ key: lit("x"), value: lit(1) },
                                             { key: lit("y"), value: lit(2) },
                                             { key: lit(3), value: lit(3) } ]));
assertExpr("({__proto__:x})", objExpr([{ type: "PrototypeMutation", value: ident("x") }]));
assertExpr("({'__proto__':x})", objExpr([{ type: "PrototypeMutation", value: ident("x") }]));
assertExpr("({['__proto__']:x})", objExpr([{ type: "Property", key: comp(lit("__proto__")), value: ident("x") }]));
assertExpr("({['__proto__']:q, __proto__() {}, __proto__: null })",
           objExpr([{ type: "Property", key: comp(lit("__proto__")), value: ident("q") },
                    { type: "Property", key: ident("__proto__"), method: true },
                    { type: "PrototypeMutation", value: lit(null) }]));
}

runtest(test);
