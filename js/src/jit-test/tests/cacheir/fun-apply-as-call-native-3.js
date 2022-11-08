// Function.prototype.apply is inlined as Function.prototype.call when it's
// called with less than two arguments.
//
// Test inlining through specialised natives.

// NOTE: We don't inline when |apply| is called with zero arguments.
function testThisAbsent() {
  for (let i = 0; i < 200; ++i) {
    let r = Array.apply();
    assertEq(r.length, 0);
  }
}
for (let i = 0; i < 2; ++i) testThisAbsent();

function test0() {
  for (let i = 0; i < 200; ++i) {
    let r = Array.apply(null);
    assertEq(r.length, 0);
  }
}
for (let i = 0; i < 2; ++i) test0();

// NOTE: We don't yet inline the case when the second argument is |null|.
function test1Null() {
  for (let i = 0; i < 200; ++i) {
    let r = Array.apply(null, null);
    assertEq(r.length, 0);
  }
}
for (let i = 0; i < 2; ++i) test1Null();

// NOTE: We don't yet inline the case when the second argument is |undefined|.
function test1Undefined() {
  for (let i = 0; i < 200; ++i) {
    let r = Array.apply(null, undefined);
    assertEq(r.length, 0);
  }
}
for (let i = 0; i < 2; ++i) test1Undefined();
