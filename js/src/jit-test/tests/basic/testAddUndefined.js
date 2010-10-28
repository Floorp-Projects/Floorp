function testAddUndefined() {
    for (var j = 0; j < 3; ++j)
        (0 + void 0) && 0;
}
testAddUndefined();
