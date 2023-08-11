// Test that parsing the given source text will throw an error with location set
// to last character in source, rather than after it.
function testErrorPosition(src) {
    let failed = false;

    try {
        parse(src)
    }
    catch (e) {
        failed = true;
        assertEq(e.lineNumber, 1)
        assertEq(e.columnNumber, src.length)
    }

    assertEq(failed, true);
}

testErrorPosition("0_")  // No trailing separator - Zero
testErrorPosition("00_") // No trailing separator - Octal
testErrorPosition("1_")  // No trailing separator - Number
testErrorPosition("1__") // No repeated separator
testErrorPosition("00n") // No octal BigInt
