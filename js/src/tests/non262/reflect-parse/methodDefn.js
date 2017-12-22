// |reftest| skip-if(!xulRuntime.shell)
function test() {

// Bug 924672: Method definitions
assertExpr("b = { a() { } }", aExpr("=", ident("b"),
              objExpr([{ key: ident("a"), value: funExpr(ident("a"), [], blockStmt([])), method:
              true}])));

assertExpr("b = { *a() { } }", aExpr("=", ident("b"),
              objExpr([{ key: ident("a"), value: genFunExpr("es6", ident("a"), [], blockStmt([])), method:
              true}])));

// Method definitions without braces
assertError("({ a() void 0 })", SyntaxError);
assertError("({ a() 1 })", SyntaxError);
assertError("({ a() false })", SyntaxError);

// getters and setters

assertExpr("({ get x() { return 42 } })",
           objExpr([ { key: ident("x"),
                       value: funExpr(ident("get x"), [], blockStmt([returnStmt(lit(42))])),
                       kind: "get" } ]));
assertExpr("({ set x(v) { return 42 } })",
           objExpr([ { key: ident("x"),
                       value: funExpr(ident("set x"), [ident("v")], blockStmt([returnStmt(lit(42))])),
                       kind: "set" } ]));

// Bug 1073809 - Getter/setter syntax with generators
assertExpr("({*get() { }})", objExpr([{ type: "Property", key: ident("get"), method: true,
                                        value: genFunExpr("es6", ident("get"), [], blockStmt([]))}]));
assertExpr("({*set() { }})", objExpr([{ type: "Property", key: ident("set"), method: true,
                                        value: genFunExpr("es6", ident("set"), [], blockStmt([]))}]));
assertError("({*get foo() { }})", SyntaxError);
assertError("({*set foo() { }})", SyntaxError);

assertError("({ *get 1() {} })", SyntaxError);
assertError("({ *get \"\"() {} })", SyntaxError);
assertError("({ *get foo() {} })", SyntaxError);
assertError("({ *get []() {} })", SyntaxError);
assertError("({ *get [1]() {} })", SyntaxError);

assertError("({ *set 1() {} })", SyntaxError);
assertError("({ *set \"\"() {} })", SyntaxError);
assertError("({ *set foo() {} })", SyntaxError);
assertError("({ *set []() {} })", SyntaxError);
assertError("({ *set [1]() {} })", SyntaxError);

}

runtest(test);
