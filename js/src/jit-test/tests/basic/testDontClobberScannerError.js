var caught = false;
try {
    Function("a, `", "");
} catch(e) {
    assertEq(e.toString().search("SyntaxError: malformed formal parameter") == -1, true);
    assertEq(e.toString().search("SyntaxError: illegal character")          == -1, false);
    caught = true;
}
assertEq(caught, true);
