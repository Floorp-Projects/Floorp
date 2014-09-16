var caught = false;
try {
  new Function("throw;");
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  assertEq(e.message.startsWith("throw statement is missing an expression") == -1, false);
  caught = true;
}
assertEq(caught, true);

caught = false;
try {
  new Function("throw\n1;");
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  assertEq(e.message.startsWith("no line break is allowed between 'throw' and its expression") == -1, false);
  caught = true;
}
assertEq(caught, true);

caught = false;
try {
  new Function("throw}");
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  assertEq(e.message.startsWith("throw statement is missing an expression") == -1, false);
  caught = true;
}
assertEq(caught, true);

caught = false;
try {
  new Function("throw");
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  assertEq(e.message.startsWith("throw statement is missing an expression") == -1, false);
  caught = true;
}
assertEq(caught, true);
