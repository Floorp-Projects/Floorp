// Function.prototype.apply is inlined as Function.prototype.call when it's
// called with less than two arguments.
//
// Test polymorphic calls.

function one() {
  return 1;
}

function two() {
  return 2;
}

function testThisAbsent() {
  let xs = [one, two];
  for (let i = 0; i < 200; ++i) {
    let r = xs[i & 1].apply();
    assertEq(r, 1 + (i & 1));
  }
}
for (let i = 0; i < 2; ++i) testThisAbsent();

function test0() {
  let xs = [one, two];
  for (let i = 0; i < 200; ++i) {
    let r = xs[i & 1].apply(null);
    assertEq(r, 1 + (i & 1));
  }
}
for (let i = 0; i < 2; ++i) test0();

// NOTE: We don't yet inline the case when the second argument is |null|.
function test1Null() {
  let xs = [one, two];
  for (let i = 0; i < 200; ++i) {
    let r = xs[i & 1].apply(null, null);
    assertEq(r, 1 + (i & 1));
  }
}
for (let i = 0; i < 2; ++i) test1Null();

// NOTE: We don't yet inline the case when the second argument is |undefined|.
function test1Undefined() {
  let xs = [one, two];
  for (let i = 0; i < 200; ++i) {
    let r = xs[i & 1].apply(null, undefined);
    assertEq(r, 1 + (i & 1));
  }
}
for (let i = 0; i < 2; ++i) test1Undefined();
