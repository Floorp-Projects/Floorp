// Legacy generators should be warned once and only once.

version(170);

function testWarn(code) {
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

testWarn("(function() { yield; })");
testWarn("function a() { yield; }");

testPass("(function*() { yield; })");
testPass("function* a() { yield; }");
