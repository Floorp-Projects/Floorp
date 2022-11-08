// Function.prototype.apply is inlined as Function.prototype.call when it's
// called with less than two arguments.
//
// Test monomorphic calls.

function testThisAbsent() {
  for (let i = 0; i < 200; ++i) {
    let r = Math.min.apply();
    assertEq(r, Infinity);
  }
}
for (let i = 0; i < 2; ++i) testThisAbsent();

function test0() {
  for (let i = 0; i < 200; ++i) {
    let r = Math.min.apply(null);
    assertEq(r, Infinity);
  }
}
for (let i = 0; i < 2; ++i) test0();

// NOTE: We don't yet inline the case when the second argument is |null|.
function test1Null() {
  for (let i = 0; i < 200; ++i) {
    let r = Math.min.apply(null, null);
    assertEq(r, Infinity);
  }
}
for (let i = 0; i < 2; ++i) test1Null();

// NOTE: We don't yet inline the case when the second argument is |undefined|.
function test1Undefined() {
  for (let i = 0; i < 200; ++i) {
    let r = Math.min.apply(null, undefined);
    assertEq(r, Infinity);
  }
}
for (let i = 0; i < 2; ++i) test1Undefined();
