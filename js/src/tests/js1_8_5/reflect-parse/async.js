// |reftest| skip-if(!xulRuntime.shell)

// async function declaration.
assertDecl("async function foo() {}", asyncFunDecl(ident("foo"), [], blockStmt([])));

// await expression.
assertDecl("async function foo() { await bar }", asyncFunDecl(ident("foo"), [], blockStmt([exprStmt(unExpr("await", ident("bar")))])));

if (typeof reportCompare === 'function')
    reportCompare(true, true);
