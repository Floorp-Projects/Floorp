function doTestMoreArgcThanNargs()
{
    var x = 0;
    for (var i = 0; i < 10; i++)
    {
        x = x + arguments[3];
    }
    return x;
}
function testMoreArgcThanNargs()
{
    return doTestMoreArgcThanNargs(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}
assertEq(testMoreArgcThanNargs(), 4*10);
