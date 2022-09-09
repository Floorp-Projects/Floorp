function foo(x) {
  Math.max(...[x]);
}

with ({}) {}
for (let i = 0; i < 100; i++) {
  foo(0);
}

let called = false;
const evil = { valueOf: () => { called = true; } };
foo(evil);

assertEq(called, true);
