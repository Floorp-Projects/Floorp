// |jit-test| crash; skip-if: getBuildConfiguration()['arm64']
//
// Test skipped on ARM64 due to bug 1549763.

for (var actual = .5; actual < 100; actual++) {
  var test2 = {
    test4: actual + 6,
    test2: actual + 9,
    printStatus: actual + 10,
    isPrototypeOf: actual + 12,
    expect: actual + 14,
    printErr: actual + 17,
    ret2: actual + 19,
    printBugNumber: actual + 32,
    test3: actual + 33,
    String: actual + 34,
    summary: actual + 40,
    test1: actual + 42,
    Array: actual + 43,
    BUGNUMBER: actual + 44,
    assertEq: actual + 45,
    __call__: actual + 47,
    x: actual + 48,
    test0: actual + 49,
    res: actual + 50
  };
}
