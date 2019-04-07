
var max = 40;
setJitCompilerOption("ion.warmup.trigger", max - 10);

// This function is used to escape "g" which is a non-escaped inner function.
// As it is not escaped within "f", the lambda for "g" would be computed on the
// bailout path. Resolving the first ".caller" implies that we have to recover
// the lambda. Resolving the second ".caller" is needed such as we can build the
// test case without explicitly escaping "g", which would prevent this
// optimization.

function return_f(i) {
  if (i != max - 1)
    return f;

  // return_f.caller == g
  // return_f.caller.caller == f
  return return_f.caller.caller;
}

function f(i) {
  function g() {
    return return_f(i);
  }

  assertRecoveredOnBailout(g, true);
  return g();
}

// This function is used to cause an invalidation after having removed a branch.
// These functions are used to check if we correctly recover the lambda
// and its environment during a bailout.
var uceFault = function (i) {
    if (i == max - 1)
        uceFault = function (i) { return true; };
    return false;
};

var uceFault_lambdaCall = eval(uneval(uceFault).replace('uceFault', 'uceFault_lambdaCall'));
function lambdaCall(i) {
    function g() {
        return i;
    }

    if (uceFault_lambdaCall(i) || uceFault_lambdaCall(i))
        assertEq(g(), i);

    assertRecoveredOnBailout(g, true);
};



for (var i = 0; i < max; i++) {
  assertEq(f(i), f);
  lambdaCall(i);
}
