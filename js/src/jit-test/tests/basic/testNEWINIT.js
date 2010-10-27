function testNEWINIT()
{
    var a;
    for (var i = 0; i < 10; ++i)
        a = [{}];
    return uneval(a);
}
assertEq(testNEWINIT(), "[{}]");
