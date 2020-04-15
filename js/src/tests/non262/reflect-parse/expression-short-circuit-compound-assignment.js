// |reftest| skip-if(!xulRuntime.shell||release_or_beta)

function test() {

assertExpr("(x ??= y)", aExpr("??=", ident("x"), ident("y")));
assertExpr("(x ||= y)", aExpr("||=", ident("x"), ident("y")));
assertExpr("(x &&= y)", aExpr("&&=", ident("x"), ident("y")));

}

runtest(test);
