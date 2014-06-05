// Test that invoking the interrupt callback counts as a step.

function testResumptionVal(resumptionVal, turnOffDebugMode) {
  var g = newGlobal();
  var dbg = new Debugger;
  g.log = "";
  g.resumptionVal = resumptionVal;

  setInterruptCallback(function () {
    g.log += "i";
    dbg.addDebuggee(g);
    var frame = dbg.getNewestFrame();
    frame.onStep = function () {
      g.log += "s";
      frame.onStep = undefined;

      if (turnOffDebugMode)
        dbg.removeDebuggee(g);

      return resumptionVal;
    };
    return true;
  });

  try {
    return g.eval("(" + function f() {
      log += "f";
      invokeInterruptCallback(function (interruptRv) {
        log += "r";
        assertEq(interruptRv, resumptionVal == undefined);
      });
      log += "a";
      return 42;
    } + ")();");
  } finally {
    assertEq(g.log, resumptionVal == undefined ? "fisra" : "fisr");
  }
}

assertEq(testResumptionVal(undefined), 42);
assertEq(testResumptionVal({ return: "not 42" }), "not 42");
try {
  testResumptionVal({ throw: "thrown 42" });
} catch (e) {
  assertEq(e, "thrown 42");
}

assertEq(testResumptionVal(undefined, true), 42);
assertEq(testResumptionVal({ return: "not 42" }, true), "not 42");

try {
  testResumptionVal({ throw: "thrown 42" }, true);
} catch (e) {
  assertEq(e, "thrown 42");
}
