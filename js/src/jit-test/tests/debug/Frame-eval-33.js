load(libdir + "evalInFrame.js");

// Test that computing the implicit 'this' in calls for D.F.eval is as if it
// were a pasted-in eval.

var G = this;

function globalFun(check, expectedThis) {
  if (check)
    assertEq(this, expectedThis);
  return this;
}
var expectedGlobalFunThis = globalFun(false);
evalInFrame(0, "globalFun(true, expectedGlobalFunThis)");

(function testInnerFun() {
  function innerFun(check, expectedThis) {
    if (check)
      assertEq(this, expectedThis);
    return this;
  }
  var expectedInnerFunThis = innerFun(false);
  evalInFrame(0, "innerFun(true, expectedInnerFunThis)");
  return [innerFun, expectedInnerFunThis]; // To prevent the JIT from optimizing out vars.
})();

(function testWith() {
  var o = {
    withFun: function withFun(check, expectedThis) {
      if (check)
        assertEq(this, expectedThis);
      return this;
    }
  };
  with (o) {
    var expectedWithFunThis = withFun(false);
    evalInFrame(0, "withFun(true, expectedWithFunThis)");
  }
})();
