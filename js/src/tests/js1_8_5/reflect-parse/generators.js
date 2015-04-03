// |reftest| skip-if(!xulRuntime.shell)
function test() {

// generators

assertDecl("function gen(x) { yield }", genFunDecl(ident("gen"), [ident("x")], blockStmt([exprStmt(yieldExpr(null))])));
assertExpr("(function(x) { yield })", genFunExpr(null, [ident("x")], blockStmt([exprStmt(yieldExpr(null))])));
assertDecl("function gen(x) { yield 42 }", genFunDecl(ident("gen"), [ident("x")], blockStmt([exprStmt(yieldExpr(lit(42)))])));
assertExpr("(function(x) { yield 42 })", genFunExpr(null, [ident("x")], blockStmt([exprStmt(yieldExpr(lit(42)))])));

}

runtest(test);
