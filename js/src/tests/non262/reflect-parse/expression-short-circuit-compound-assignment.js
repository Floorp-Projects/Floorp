// |reftest| skip-if(!xulRuntime.shell)

function test() {

assertExpr("(x ??= y)", aExpr("??=", ident("x"), ident("y")));
assertExpr("(x ||= y)", aExpr("||=", ident("x"), ident("y")));
assertExpr("(x &&= y)", aExpr("&&=", ident("x"), ident("y")));

}

runtest(test);
