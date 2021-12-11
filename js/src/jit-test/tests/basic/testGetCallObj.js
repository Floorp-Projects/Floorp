function testGetCallObjInlined(i) {
    if (i > 7) eval("1");
    return 1;
}

function testGetCallObj() {
    for (var i = 0; i < 10; ++i)
        testGetCallObjInlined(i);
    return "ok";
}
assertEq(testGetCallObj(), "ok");
