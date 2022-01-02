function testSetPropNeitherMissNorHit() {
    for (var j = 0; j < 5; ++j) { if (({}).__proto__ = 1) { } }
    return "ok";
}
assertEq(testSetPropNeitherMissNorHit(), "ok");
