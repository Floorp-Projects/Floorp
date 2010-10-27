function testDenseArrayProp()
{
    [].__proto__.x = 1;
    ({}).__proto__.x = 2;
    var a = [[],[],[],({}).__proto__];
    for (var i = 0; i < a.length; ++i)
        uneval(a[i].x);
    delete [].__proto__.x;
    delete ({}).__proto__.x;
    return "ok";
}
assertEq(testDenseArrayProp(), "ok");
