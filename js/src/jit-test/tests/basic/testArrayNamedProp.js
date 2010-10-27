function testArrayNamedProp() {
    for (var x = 0; x < 10; ++x) { [4].sort-- }
    return "ok";
}
assertEq(testArrayNamedProp(), "ok");
