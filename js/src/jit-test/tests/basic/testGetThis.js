function testGetThis() {
    for (var i = 0; i < 3; ++i) {
        (function() { return this; })();
    }
    return "ok";
}
assertEq(testGetThis(), "ok");
