var g = newGlobal();
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);

function check(expr, expected) {
  print("checking " + uneval(expr) + ", expecting " +
        (expected ? "script" : "no script"));
  let val = gDO.executeInGlobal(expr).return;
  if (expected)
    assertEq(val.script instanceof Debugger.Script, true);
  else
    assertEq(val.script, undefined);
}

check('(function g(){})', true);
check('(function* h() {})', true);
check('(async function j() {})', true);
check('(async function* k() {})', true);
check('({})', false);
check('Math.atan2', false);
