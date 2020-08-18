// |jit-test| skip-if: helperThreadCount() === 0

function assertError(thunk) {
  let threw = false;
  try {
    thunk();
  } catch (e) {
    threw = true;
  }
  assertEq(threw, true);
}

let initialHelperThreads = helperThreadCount();

// Test that setting maxHelperThreads limits the number of threads.
gcparam("helperThreadRatio", 100);
for (let i = 1; i <= initialHelperThreads; i++) {
  gcparam("maxHelperThreads", i);
  assertEq(gcparam("helperThreadCount"), i);
}

// Test that setting helperThreadRatio works as expected.
gcparam("maxHelperThreads", 1000);
for (let i = 25; i <= 400; i *= 2) {
  gcparam("helperThreadRatio", i);
  let ratio = i / 100;
  let expected = Math.max(Math.floor(initialHelperThreads * ratio), 1);
  assertEq(gcparam("helperThreadCount"), expected);
  assertEq(helperThreadCount(), Math.max(initialHelperThreads, expected));
}

// Test that illegal settings are checked.
assertError(() => gcparam("helperThreadRatio", 0));
assertError(() => gcparam("maxHelperThreads", 0));
