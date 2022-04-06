let trigger = false;

function bar(x) {
  with ({}) {}
  if (trigger) {
    gc(foo, "shrinking");
    trigger = false;
  }
  return Object(x);
}

function foo() {
  let result = undefined;
  const arr = [8];
  for (var i = 0; i < 10; i++) {
    result = bar(...arr);
    assertEq(Number(result), 8);
  }
  return result;
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  foo();
}

trigger = true;
foo();
