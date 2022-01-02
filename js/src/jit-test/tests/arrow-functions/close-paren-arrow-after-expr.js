var caught = false;
try {
  eval("1\n)=>");
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  caught = true;
}
assertEq(caught, true);
