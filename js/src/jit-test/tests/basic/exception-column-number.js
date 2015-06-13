try {
    Array.indexOf();
} catch (e) {
    assertEq(e.columnNumber, 5);
    // Filter the filename from the stack, since we have no clue what
    // to expect there.
    assertEq(e.stack.replace(/[^:]*/, ""), ":2:5\n");
}
