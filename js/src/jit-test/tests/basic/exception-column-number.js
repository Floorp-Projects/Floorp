try {
    Array.indexOf();
} catch (e) {
    assertEq(e.columnNumber, 5);
    // Filter the filename from the stack, since we have no clue what
    // to expect there.  Search for ':' from the end, because the file
    // path may contain ':' in it.
    var lastColon = e.stack.lastIndexOf(':');
    var afterPath = e.stack.lastIndexOf(':', lastColon - 1);
    assertEq(e.stack.substring(afterPath), ":2:5\n");
}
