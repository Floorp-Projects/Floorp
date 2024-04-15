// |jit-test| skip-if: helperThreadCount() === 0

// Allow maximum number of helper threads
gcparam('maxHelperThreads', 8);
gcparam('helperThreadRatio', 100);

check();

for (let i of [0, 1, 4, 8, 4, 0]) {
  gcparam('maxMarkingThreads', i);
  assertEq(gcparam('maxMarkingThreads'), i);
  check();
}

function check() {
  assertEq(gcparam('markingThreadCount') <= gcparam('maxMarkingThreads'), true);
  assertEq(gcparam('markingThreadCount') < gcparam('helperThreadCount'), true);
}
