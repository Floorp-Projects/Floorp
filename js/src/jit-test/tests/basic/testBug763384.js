var caught = false;
try {
    ''.match('(');
} catch (e) {
    caught = true;
    assertEq(e instanceof Error, true);
    assertEq(('' + e).indexOf('SyntaxError') === -1, false);
}
assertEq(caught, true);
