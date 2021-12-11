// |reftest| skip-if(!xulRuntime.shell)
// Classes
function testClassStaticBlock() {

    assertExpr("(class C { static { 2; } })", classExpr(ident("C"), null, [staticClassBlock(blockStmt([exprStmt(lit(2))]))]));
}

runtest(testClassStaticBlock);