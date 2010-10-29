function testNEWINIT_DOUBLE()
{
    for (var z = 0; z < 2; ++z) { ({ 0.1: null })}
    return "ok";
}
assertEq(testNEWINIT_DOUBLE(), "ok");
