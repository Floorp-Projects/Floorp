// RegExp.multiline access should be warned once and only once.

function testWarn(code) {
  enableLastWarning();
  var g = newGlobal();
  g.code = code;
  g.eval('eval(code)');
  var warning = getLastWarning();
  assertEq(warning !== null, true, "warning should be caught for " + code);
  assertEq(warning.name, "SyntaxError");

  clearLastWarning();
  g.eval('eval(code)');
  warning = getLastWarning();
  assertEq(warning, null, "warning should not be caught for 2nd ocurrence");
  disableLastWarning();
}

testWarn("var a = RegExp.multiline;");
testWarn("RegExp.multiline = true;");

testWarn("var a = RegExp['$*'];");
testWarn("RegExp['$*'] = true;");
