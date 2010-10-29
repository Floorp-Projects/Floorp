function testArrayNaNIndex()
{
    for (var j = 0; j < 4; ++j) { [this[NaN]]; }
    for (var j = 0; j < 5; ++j) { if([1][-0]) { } }
    return "ok";
}
assertEq(testArrayNaNIndex(), "ok");
