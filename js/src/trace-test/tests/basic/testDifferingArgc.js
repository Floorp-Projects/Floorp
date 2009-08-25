function doTestDifferingArgc(a, b)
{
    var k = 0;
    for (var i = 0; i < 10; i++)
    {
        k += i;
    }
    return k;
}
function testDifferingArgc()
{
    var x = 0;
    x += doTestDifferingArgc(1, 2);
    x += doTestDifferingArgc(1);
    x += doTestDifferingArgc(1, 2, 3);
    return x;
}
assertEq(testDifferingArgc(), 45*3);
