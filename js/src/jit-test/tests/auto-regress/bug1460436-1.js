for (var j = 0; j < 2; ++j) {
    for (var k = 0; k < 2; ++k) {
        assertEq(Math.sign(Math.PI ? 2 : (function() {})() ? h : l), 1)
    }
}
