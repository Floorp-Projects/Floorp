// |reftest| skip-if(!xulRuntime.shell)
function testNewTarget() {

    // new.target is currently valid inside any non-arrow, non-generator function
    assertInFunctionExpr("new.target", newTarget());

    // even with gratuitous whitespace.
    assertInFunctionExpr(`new.
                            target`, newTarget());

    // invalid in top-level scripts
    assertError("new.target", SyntaxError);

    // valid in arrow functions inside functions
    assertInFunctionExpr("()=>new.target", arrowExpr([], newTarget()));
    assertError("(() => new.target))", SyntaxError);

    // valid in generators, too!
    assertStmt("function *foo() { new.target; }", genFunDecl(ident("foo"), [],
               blockStmt([exprStmt(newTarget())])));

    // new.target is a member expression. You should be able to call, invoke, or
    // access properties of it.
    assertInFunctionExpr("new.target.foo", dotExpr(newTarget(), ident("foo")));
    assertInFunctionExpr("new.target[\"foo\"]", memExpr(newTarget(), literal("foo")));

    assertInFunctionExpr("new.target()", callExpr(newTarget(), []));
    assertInFunctionExpr("new new.target()", newExpr(newTarget(), []));

    // assignment to newTarget is an error
    assertError("new.target = 4", SyntaxError);

    // only new.target is a valid production, no shorn names or other names
    assertError("new.", SyntaxError);
    assertError("new.foo", SyntaxError);
    assertError("new.targe", SyntaxError);

    // obj.new.target is still a member expression
    assertExpr("obj.new.target", dotExpr(dotExpr(ident("obj"), ident("new")), ident("target")));
}

runtest(testNewTarget);
