// |jit-test| --enable-weak-refs

// Test that drainJobQueue() drains all jobs, including those queued
// by FinalizationGroup callbacks.

let finalizeRan = false;
let promiseRan = false;

let fg = new FinalizationGroup(() => {
  finalizeRan = true;
  Promise.resolve().then(() => {
    promiseRan = true;
  });
});

fg.register({}, {});

gc();

assertEq(finalizeRan, false);
assertEq(promiseRan, false);

drainJobQueue();

assertEq(finalizeRan, true);
assertEq(promiseRan, true);
