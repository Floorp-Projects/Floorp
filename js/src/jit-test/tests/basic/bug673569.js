function qualified_tests(prefix) {
  let scopes = evalReturningScope(prefix + "var x = 1");
  assertEq(scopes.vars.x, 1);
}

qualified_tests("");
qualified_tests("'use strict'; ");

let scopes = evalReturningScope("x = 1");
assertEq(scopes.vars.x, 1);

let fail = true;
try {
  evalReturningScope("'use strict'; x = 1");
} catch (e) {
  fail = false;
}
assertEq(fail, false);
