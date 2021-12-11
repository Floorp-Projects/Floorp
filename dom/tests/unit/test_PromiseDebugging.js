function run_test() {
  // Hack around Promise.jsm being stuck on my global
  Assert.equal(false, PromiseDebugging === undefined);
  var res;
  var p = new Promise(function(resolve, reject) {
    res = resolve;
  });
  var state = PromiseDebugging.getState(p);
  Assert.equal(state.state, "pending");

  do_test_pending();

  p.then(function() {
    var state2 = PromiseDebugging.getState(p);
    Assert.equal(state2.state, "fulfilled");
    Assert.equal(state2.value, 5);
    do_test_finished();
  });

  res(5);
}
