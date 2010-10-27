function testCALLELEM()
{
    function f() {
        return 5;
    }

    function g() {
        return 7;
    }

    var x = [f,f,f,f,g];
    var y = 0;
    for (var i = 0; i < 5; ++i)
        y = x[i]();
    return y;
}
assertEq(testCALLELEM(), 7);
