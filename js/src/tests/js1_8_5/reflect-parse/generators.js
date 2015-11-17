// |reftest| skip-if(!xulRuntime.shell)
function test() {

// generators

assertDecl("function gen(x) { yield }", genFunDecl("legacy", ident("gen"), [ident("x")], blockStmt([exprStmt(yieldExpr(null))])));
assertExpr("(function(x) { yield })", genFunExpr("legacy", null, [ident("x")], blockStmt([exprStmt(yieldExpr(null))])));
assertDecl("function gen(x) { yield 42 }", genFunDecl("legacy", ident("gen"), [ident("x")], blockStmt([exprStmt(yieldExpr(lit(42)))])));
assertExpr("(function(x) { yield 42 })", genFunExpr("legacy", null, [ident("x")], blockStmt([exprStmt(yieldExpr(lit(42)))])));

assertDecl("function* gen() {}", genFunDecl("es6", ident("gen"), [], blockStmt([])));
assertExpr("(function*() {})", genFunExpr("es6", null, [], blockStmt([])));
assertExpr("(function* gen() {})", genFunExpr("es6", ident("gen"), [], blockStmt([])));

}

runtest(test);
