function testReallyDeepNestedExit() {
    for (var i = 0; i < 5*4; i++) {}
    for (var o = schedule = i = 9 ; i < 5; i++) {}
}
assertEq(testReallyDeepNestedExit(), undefined);

