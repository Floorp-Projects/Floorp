// |reftest| skip-if(!xulRuntime.shell)

// async function declaration.
assertDecl("async function foo() {}", asyncFunDecl(ident("foo"), [], blockStmt([])));

// async function expression.
assertExpr("(async function() {})", asyncFunExpr(null, [], blockStmt([])));
assertExpr("(async function foo() {})", asyncFunExpr(ident("foo"), [], blockStmt([])));

// async method.
assertExpr("({ async foo() {} })", objExpr([{ key: ident("foo"), value: asyncFunExpr(ident("foo"), [], blockStmt([]))}]));

assertStmt("class C { async foo() {} }", classStmt(ident("C"), null, [classMethod(ident("foo"), asyncFunExpr(ident("foo"), [], blockStmt([])), "method", false)]));
assertStmt("class C { static async foo() {} }", classStmt(ident("C"), null, [classMethod(ident("foo"), asyncFunExpr(ident("foo"), [], blockStmt([])), "method", true)]));

// await expression.
assertDecl("async function foo() { await bar }", asyncFunDecl(ident("foo"), [], blockStmt([exprStmt(unExpr("await", ident("bar")))])));

if (typeof reportCompare === 'function')
    reportCompare(true, true);
