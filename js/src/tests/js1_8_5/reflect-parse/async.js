// |reftest| skip-if(!xulRuntime.shell)

// async function declaration.
assertDecl("async function foo() {}", asyncFunDecl(ident("foo"), [], blockStmt([])));

// async function expression.
assertExpr("(async function() {})", asyncFunExpr(null, [], blockStmt([])));
assertExpr("(async function foo() {})", asyncFunExpr(ident("foo"), [], blockStmt([])));

// await expression.
assertDecl("async function foo() { await bar }", asyncFunDecl(ident("foo"), [], blockStmt([exprStmt(unExpr("await", ident("bar")))])));

if (typeof reportCompare === 'function')
    reportCompare(true, true);
