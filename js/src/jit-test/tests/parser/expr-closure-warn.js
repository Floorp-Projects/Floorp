// Expression closure should be warned once and only once.

var release = getBuildConfiguration().release;

function testWarn(code) {
  if (release) {
    // Warning for expression closure is non-release-only.
    testPass(code);
    return;
  }

  enableLastWarning();
  var g = newGlobal();
  g.code = code;
  g.eval('eval(code)');
  var warning = getLastWarning();
  assertEq(warning !== null, true, "warning should be caught for " + code);
  assertEq(warning.name, "Warning");

  clearLastWarning();
  g.eval('eval(code)');
  warning = getLastWarning();
  assertEq(warning, null, "warning should not be caught for 2nd ocurrence");

  clearLastWarning();
  g = newGlobal();
  g.code = code;
  g.eval('Reflect.parse(code);');
  warning = getLastWarning();
  assertEq(warning !== null, true, "warning should be caught for " + code);
  assertEq(warning.name, "Warning");

  clearLastWarning();
  g.eval('Reflect.parse(code);');
  warning = getLastWarning();
  assertEq(warning, null, "warning should not be caught for 2nd ocurrence");
  disableLastWarning();
}

function testPass(code) {
  enableLastWarning();
  var g = newGlobal();
  g.code = code;
  g.eval('eval(code)');
  var warning = getLastWarning();
  assertEq(warning, null, "warning should not be caught for " + code);

  clearLastWarning();
  g = newGlobal();
  g.code = code;
  g.eval('Reflect.parse(code);');
  warning = getLastWarning();
  assertEq(warning, null, "warning should not be caught for " + code);
  disableLastWarning();
}

testWarn("function f() 1");
testWarn("(function() 1)");
testWarn("({ get x() 1 })");
testWarn("({ set x(v) 1 })");

testPass("function f() { 1 }");
testPass("() => 1");
