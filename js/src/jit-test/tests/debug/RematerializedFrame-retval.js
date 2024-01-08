// |jit-test| slow; error: InternalError; --baseline-eager; --ion-eager
// Make sure that return values we store in RematerializedFrames via resumption
// values get propagated to the BaselineFrames we build from them.
//
// Test case from bug 1285939; there's another in bug 1282518, but this one
// takes less time to run, when it doesn't crash.

var lfLogBuffer = `
function testResumptionVal(resumptionVal, turnOffDebugMode) {
  var g = newGlobal({newCompartment: true});
  var dbg = new Debugger;
  setInterruptCallback(function () {
    dbg.addDebuggee(g);
    var frame = dbg.getNewestFrame();
    frame.onStep = function () {
      frame.onStep = undefined;
      return resumptionVal;
    };
    return true;
  });
  try {
    return g.eval("(" + function f() {
      invokeInterruptCallback(function (interruptRv) {
    f({ valueOf: function () { dbg.g(dbg); }});
  });
    } + ")();");
  } finally {  }
}
assertEq(testResumptionVal({ return: "not 42" }), "not 42");
`;
loadFile(lfLogBuffer);
function loadFile(lfVarx) {
    try {
         let m = parseModule(lfVarx);
         moduleLink(m);
         moduleEvaluate(m);
    } catch (lfVare) {}
}

