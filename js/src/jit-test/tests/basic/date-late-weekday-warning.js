/**
 * Test deprecation warning for late weekday in Date.parse
 */

function testWarn(date) {
  const g = newGlobal();
  g.eval(`Date.parse("${date}")`);
  const warning = getLastWarning();
  assertEq(warning !== null, true, `warning should be caught for ${date}`);
  assertEq(warning.name, "Warning", warning.name);

  clearLastWarning();

  g.eval(`Date.parse("${date}")`);
  assertEq(getLastWarning(), null, "warning should not be caught for 2nd ocurrence");
}

function testNoWarn(date) {
  Date.parse(date);
  assertEq(getLastWarning(), null, `warning should not be caught for ${date}`);
}

enableLastWarning();

testWarn("Sep 26 1995 Tues");
testWarn("Sep 26 Tues 1995");
testWarn("Sep 26 Tues 1995 Tues");
testWarn("Sep 26 1995 10:Tues:00");

testNoWarn("Sep 26 1995");
testNoWarn("Tues Sep 26 1995");
testNoWarn("Sep Tues 26 1995");

disableLastWarning();
