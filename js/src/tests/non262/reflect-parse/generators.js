// |reftest| skip-if(!xulRuntime.shell)
function test() {

// generators

assertDecl("function* gen() {}", genFunDecl("es6", ident("gen"), [], blockStmt([])));
assertExpr("(function*() {})", genFunExpr("es6", null, [], blockStmt([])));
assertExpr("(function* gen() {})", genFunExpr("es6", ident("gen"), [], blockStmt([])));

}

runtest(test);
