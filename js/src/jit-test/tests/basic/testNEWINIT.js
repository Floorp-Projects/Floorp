function testNEWINIT()
{
    var a;
    for (var i = 0; i < 10; ++i)
        a = [{}];
    return JSON.stringify(a);
}
assertEq(testNEWINIT(), "[{}]");
