function testINITELEM()
{
    var x;
    for (var i = 0; i < 10; ++i)
        x = { 0: 5, 1: 5 };
    return x[0] + x[1];
}
assertEq(testINITELEM(), 10);
