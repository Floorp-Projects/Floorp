// |jit-test| --ion-range-analysis=off

function f(a, ...rest) {
  return rest.length;
}

with ({});

for (let i = 0; i < 1000; ++i) {
  assertEq(f(), 0);
}
