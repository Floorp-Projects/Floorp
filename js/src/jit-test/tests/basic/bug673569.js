function qualified_tests(prefix) {
  let scope = evalReturningScope(prefix + "let x = 1");
  assertEq(scope.x, 1);

  scope = evalReturningScope(prefix + "var x = 1");
  assertEq(scope.x, 1);

  scope = evalReturningScope(prefix + "const x = 1");
  assertEq(scope.x, 1);
}

qualified_tests("");
qualified_tests("'use strict'; ");

let scope = evalReturningScope("x = 1");
assertEq(scope.x, 1);

let fail = true;
try {
  evalReturningScope("'use strict'; x = 1");
} catch (e) {
  fail = false;
}
assertEq(fail, false);
