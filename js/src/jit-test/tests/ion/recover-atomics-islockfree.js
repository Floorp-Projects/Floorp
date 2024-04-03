// |jit-test| --fast-warmup; --ion-offthread-compile=off

function foo(n, trigger) {
  let result = Atomics.isLockFree(n * -1);
  if (trigger) {
    assertEq(result, false);
  }
}

for (var i = 0; i < 100; i++) {
  foo(-50, false);
}
foo(0, true);

function bar(n, trigger) {
  let result = Atomics.isLockFree(n * 4);
  if (trigger) {
    assertEq(result, false);
  }
}

for (var i = 0; i < 100; i++) {
  bar(1, false);
}
bar(0x40000001, true);
