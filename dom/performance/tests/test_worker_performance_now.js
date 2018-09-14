function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + ": " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function workerTestDone() {
  postMessage({ type: 'finish' });
}

ok(self.performance, "Performance object should exist.");
ok(typeof self.performance.now == 'function', "Performance object should have a 'now' method.");
var n = self.performance.now(), d = Date.now();
ok(n >= 0, "The value of now() should be equal to or greater than 0.");
ok(self.performance.now() >= n, "The value of now() should monotonically increase.");

// Spin on setTimeout() until performance.now() increases. Due to recent
// security developments, the hr-time working group has not yet reached
// consensus on what the recommend minimum clock resolution should be:
// https://w3c.github.io/hr-time/#clock-resolution
// Since setTimeout might return too early/late, our goal is for
// performance.now() to increase before a 2 ms deadline rather than specific
// number of setTimeout(N) invocations.
// See bug 749894 (intermittent failures of this test)
setTimeout(checkAfterTimeout, 1);

var checks = 0;

function checkAfterTimeout() {
  checks++;
  var d2 = Date.now();
  var n2 = self.performance.now();

  // Spin on setTimeout() until performance.now() increases. Abort the test
  // if it runs for more than 2 ms or 50 timeouts.
  let elapsedTime = d2 - d;
  let elapsedPerf = n2 - n;
  if (elapsedPerf == 0 && elapsedTime < 2 && checks < 50) {
    setTimeout(checkAfterTimeout, 1);
    return;
  }

  // Our implementation provides 1 ms resolution (bug 1451790), but we
  // can't assert that elapsedPerf >= 1 ms because this worker test runs with
  // "privacy.reduceTimerPrecision" == false so performance.now() is not
  // limited to 1 ms resolution.
  ok(elapsedPerf > 0,
     `Loose - the value of now() should increase after 2ms. ` +
     `delta now(): ${elapsedPerf} ms`);

  // If we need more than 1 iteration, then either performance.now() resolution
  // is shorter than 1 ms or setTimeout() is returning too early.
  ok(checks == 1,
     `Strict - the value of now() should increase after one setTimeout. ` +
     `iters: ${checks}, dt: ${elapsedTime}, now(): ${n2}`);

  workerTestDone();
};
