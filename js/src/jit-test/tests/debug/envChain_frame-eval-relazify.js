// |jit-test| skip-if: isLcovEnabled()

function func(doEval) {
  if (doEval) {
    const dbg = this.newGlobal({sameZoneAs: {}}).Debugger({});
    dbg.getNewestFrame().eval(`
  function reaction() {
    // Access global variable to walk through the environment chain.
    return Map;
  };
  Promise.resolve(1).then(reaction);
`);
  }
}

assertEq(isLazyFunction(func), true);
assertEq(isRelazifiableFunction(func), false);

// Delazify here.
func(false);

// Delazified function should be marked relazifiable.
assertEq(isLazyFunction(func), false);
assertEq(isRelazifiableFunction(func), true);

// Perform Frame.prototype.eval
func(true);

// Frame.prototype.eval should mark the enclosing script non-relazifiable.
assertEq(isLazyFunction(func), false);
assertEq(isRelazifiableFunction(func), false);

// This shouldn't relazify `func`.
relazifyFunctions();
relazifyFunctions();

assertEq(isLazyFunction(func), false);
assertEq(isRelazifiableFunction(func), false);

// Execute the inner function to make sure the enclosing script is not lazy.
drainJobQueue();
