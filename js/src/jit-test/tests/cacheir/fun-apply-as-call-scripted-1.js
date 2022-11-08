// Function.prototype.apply is inlined as Function.prototype.call when it's
// called with less than two arguments.
//
// Test monomorphic calls.

function one() {
  return 1;
}

function testThisAbsent() {
  for (let i = 0; i < 200; ++i) {
    let r = one.apply();
    assertEq(r, 1);
  }
}
for (let i = 0; i < 2; ++i) testThisAbsent();

function test0() {
  for (let i = 0; i < 200; ++i) {
    let r = one.apply(null);
    assertEq(r, 1);
  }
}
for (let i = 0; i < 2; ++i) test0();

// NOTE: We don't yet inline the case when the second argument is |null|.
function test1Null() {
  for (let i = 0; i < 200; ++i) {
    let r = one.apply(null, null);
    assertEq(r, 1);
  }
}
for (let i = 0; i < 2; ++i) test1Null();

// NOTE: We don't yet inline the case when the second argument is |undefined|.
function test1Undefined() {
  for (let i = 0; i < 200; ++i) {
    let r = one.apply(null, undefined);
    assertEq(r, 1);
  }
}
for (let i = 0; i < 2; ++i) test1Undefined();
