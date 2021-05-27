// |reftest| skip-if(!xulRuntime.shell) shell-option(--enable-class-static-blocks)
// Classes
function testClassStaticBlock() {

    assertExpr("(class C { static { 2; } })", classExpr(ident("C"), null, [staticClassBlock(blockStmt([exprStmt(lit(2))]))]));
}

runtest(testClassStaticBlock);