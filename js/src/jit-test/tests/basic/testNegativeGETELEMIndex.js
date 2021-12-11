function testNegativeGETELEMIndex()
{
    for (let i=0;i<3;++i) /x/[-4];
    return "ok";
}
assertEq(testNegativeGETELEMIndex(), "ok");
