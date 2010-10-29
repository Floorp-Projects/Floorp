function testConstantBooleanExpr()
{
    for (var j = 0; j < 3; ++j) { if(true <= true) { } }
    return "ok";
}
assertEq(testConstantBooleanExpr(), "ok");
