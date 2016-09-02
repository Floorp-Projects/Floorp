// for-each should be warned once and only once.

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

testWarn("for each (var x in {}) {}");
testWarn("for each (x in {}) {}");
testWarn("for each (let y in {}) {}");
testWarn("for each (const y in {}) {}");
testWarn("for each ([x, y] in {}) {}");
