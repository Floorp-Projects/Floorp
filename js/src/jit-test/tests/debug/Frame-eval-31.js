// evalInFrame with non-syntactic scopes.

load(libdir + "asserts.js");
load(libdir + "evalInFrame.js");

evalReturningScope(`
  var x = 42;
  assertEq(evalInFrame(0, "x"), 42);
`);
