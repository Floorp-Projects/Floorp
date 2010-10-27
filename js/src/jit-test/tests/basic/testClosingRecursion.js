// Test no assert (bug 464089)
function shortRecursiveLoop(b, c) {
    for (var i = 0; i < c; i++) {
        if (b)
            shortRecursiveLoop(c - 1);
    }
}
function testClosingRecursion() {
    shortRecursiveLoop(false, 1);
    shortRecursiveLoop(true, 3);
    return true;
}
assertEq(testClosingRecursion(), true);
