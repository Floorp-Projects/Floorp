var caught = false;
try {
  new Function("switch (x) {} }");
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  assertEq(e.message.startsWith("unexpected garbage after function body, starting with '}'") == -1, false);
  caught = true;
}
assertEq(caught, true);
