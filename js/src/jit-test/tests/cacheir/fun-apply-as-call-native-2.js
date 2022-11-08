// Function.prototype.apply is inlined as Function.prototype.call when it's
// called with less than two arguments.
//
// Test polymorphic calls.

function testThisAbsent() {
  let xs = [Math.min, Math.max];
  let ys = [Infinity, -Infinity];
  for (let i = 0; i < 200; ++i) {
    let r = xs[i & 1].apply();
    assertEq(r, ys[i & 1]);
  }
}
for (let i = 0; i < 2; ++i) testThisAbsent();

function test0() {
  let xs = [Math.min, Math.max];
  let ys = [Infinity, -Infinity];
  for (let i = 0; i < 200; ++i) {
    let r = xs[i & 1].apply(null);
    assertEq(r, ys[i & 1]);
  }
}
for (let i = 0; i < 2; ++i) test0();

// NOTE: We don't yet inline the case when the second argument is |null|.
function test1Null() {
  let xs = [Math.min, Math.max];
  let ys = [Infinity, -Infinity];
  for (let i = 0; i < 200; ++i) {
    let r = xs[i & 1].apply(null, null);
    assertEq(r, ys[i & 1]);
  }
}
for (let i = 0; i < 2; ++i) test1Null();

// NOTE: We don't yet inline the case when the second argument is |undefined|.
function test1Undefined() {
  let xs = [Math.min, Math.max];
  let ys = [Infinity, -Infinity];
  for (let i = 0; i < 200; ++i) {
    let r = xs[i & 1].apply(null, undefined);
    assertEq(r, ys[i & 1]);
  }
}
for (let i = 0; i < 2; ++i) test1Undefined();
