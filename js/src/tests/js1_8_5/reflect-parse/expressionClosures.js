// |reftest| skip-if(!xulRuntime.shell||!release_or_beta)
function test() {

// expression closures

assertDecl("function inc(x) (x + 1)", funDecl(ident("inc"), [ident("x")], binExpr("+", ident("x"), lit(1))));
assertExpr("(function(x) (x+1))", funExpr(null, [ident("x")], binExpr("+", ident("x"), lit(1))));

}

runtest(test);
