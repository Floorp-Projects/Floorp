// Test that drainJobQueue() drains all jobs, including those queued
// by FinalizationRegistry callbacks.

let finalizeRan = false;
let promiseRan = false;

let fr = new FinalizationRegistry(() => {
  finalizeRan = true;
  Promise.resolve().then(() => {
    promiseRan = true;
  });
});

fr.register({}, {});

gc();

assertEq(finalizeRan, false);
assertEq(promiseRan, false);

drainJobQueue();

assertEq(finalizeRan, true);
assertEq(promiseRan, true);
